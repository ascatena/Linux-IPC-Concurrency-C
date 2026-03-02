
# Linux-IPC-Concurrency-C
## Comunicación entre Procesos y Concurrencia en Sistemas POSIX (C)
![C](https://img.shields.io/badge/c-%2300599C.svg?style=for-the-badge&logo=c&logoColor=white)
![Linux](https://img.shields.io/badge/Linux-FCC624?style=for-the-badge&logo=linux&logoColor=black)
![GNU Bash](https://img.shields.io/badge/GNU%20Bash-4EAA25?style=for-the-badge&logo=GNU%20Bash&logoColor=white)

Autor: Adriano Scatena  

---

## Índice

1. [Descripción General](#descripción-general)  
2. [process_manager_signals.c](#1-process_manager_signalsc)  
   2.1. [Objetivo](#objetivo)  
   2.2. [Arquitectura del Diseño](#arquitectura-del-diseño)  
   2.3. [Modelo de Ejecución](#modelo-de-ejecución)  
   2.4. [Comportamiento por Señal](#comportamiento-por-señal)  
   2.5. [Estructura de Datos Utilizada](#estructura-de-datos-utilizada)  
   2.6. [Aspectos Técnicos Relevantes](#aspectos-técnicos-relevantes)  
   2.7. [Compilación](#compilación)  

3. [shared_memory_ipc.c](#2-shared_memory_ipcc)  
   3.1. [Objetivo](#objetivo-1)  
   3.2. [Modelo General](#modelo-general)  
   3.3. [Componentes IPC Utilizados](#componentes-ipc-utilizados)  
   3.4. [Cifrado Implementado](#cifrado-implementado)  
   3.5. [Control de Estado de Proceso](#control-de-estado-de-proceso)  
   3.6. [Terminación Coordinada](#terminación-coordinada)  
   3.7. [Compilación y Ejecución](#compilación)  

4. [Decisiones de Diseño Destacadas](#decisiones-de-diseño-destacadas)  
5. [Tecnologías y Conceptos](#tecnologías-y-conceptos)  

---

## Descripción General

Este repositorio implementa mecanismos avanzados de **Comunicación entre Procesos (IPC)** y control de concurrencia en sistemas GNU/Linux bajo estándar POSIX, utilizando lenguaje C a nivel de system calls.

El proyecto está compuesto por dos programas independientes:

- `process_manager_signals.c`
- `shared_memory_ipc.c`

Ambos exploran problemas clásicos de Sistemas Operativos:

- Creación y jerarquía de procesos (`fork`)
- Reemplazo de imagen de proceso (`exec`)
- Manejo asincrónico de señales
- Sincronización entre procesos sin memoria compartida implícita
- Uso explícito de memoria compartida System V
- Comunicación mediante FIFOs
- Terminación coordinada y liberación segura de recursos
- Diseño robusto frente a condiciones de carrera

El enfoque del repositorio es práctico y técnico, orientado a demostrar comprensión profunda del modelo de ejecución de procesos en Linux.

---

# 1. process_manager_signals.c  
## Gestión Dinámica de Procesos mediante Señales

### Objetivo

Diseñar un proceso padre que responda dinámicamente a señales externas creando y gestionando procesos hijos con comportamientos diferenciados.

---

## Arquitectura del Diseño

El programa implementa:

- Manejadores de señales livianos que solo modifican flags `volatile`
- Separación estricta entre:
  - Código del manejador
  - Lógica principal
- Uso de `pause()` para bloqueo eficiente
- Lista enlazada dinámica tipo LIFO para registrar PIDs

Se evita ejecutar lógica compleja dentro de los handlers, reduciendo comportamiento indefinido.

---

## Modelo de Ejecución

El proceso padre:

- Registra manejadores para `SIGUSR1`, `SIGUSR2` y `SIGTERM`
- Permanece bloqueado esperando señales
- Reacciona según la señal recibida

---

## Comportamiento por Señal

### SIGUSR1

- Ejecuta `fork()`
- Registra el PID en una lista enlazada dinámica
- El hijo:
  - Configura `SIGALRM`
  - Imprime su PID y el del padre cada 5 segundos
  - Utiliza `alarm(5)` para temporización periódica

Se implementa una bandera `_5seg` para desacoplar la señal de la lógica de impresión.

---

### SIGUSR2

- Ejecuta `fork()`
- El hijo reemplaza su imagen con:

```c
execl("/bin/ls", "ls", NULL);
```

Esto demuestra uso correcto de `exec` y reemplazo total del espacio de memoria del proceso.

---

### SIGTERM

- Recorre la lista dinámica de hijos creados por `SIGUSR1`
- Envía `SIGTERM` a cada uno con intervalo de 1 segundo
- Libera memoria dinámica
- Finaliza ordenadamente

---

## Estructura de Datos Utilizada

```c
typedef struct nodo {
    pid_t pid;
    struct nodo *siguiente;
} nodo;
```

Permite:

- Registro incremental
- Eliminación ordenada
- Gestión explícita de memoria (`malloc` / `free`)

---

## Aspectos Técnicos Relevantes

- Uso de `volatile bool` para flags de señal
- Separación de handlers y lógica
- Uso correcto de `SIG_DFL` en procesos hijos
- Gestión manual de memoria dinámica
- Prevención de ejecución simultánea indebida

---

## Compilación

```bash
gcc process_manager_signals.c -o process_manager
```

---

# 2. shared_memory_ipc.c  
## Sincronización Bidireccional con Memoria Compartida y Señales

### Objetivo

Implementar comunicación coordinada entre dos procesos independientes (A y B) utilizando:

- Memoria compartida System V
- Pipes nombrados (FIFO)
- Señales POSIX
- Cifrado modular aplicado a los datos

---

## Modelo General

El sistema implementa un protocolo de alternancia estricta:

- A escribe posiciones 0–9
- B escribe posiciones 10–19
- Ninguno puede escribir hasta que el otro haya leído
- Escritura: 1 valor por segundo
- Lectura: sincronizada por señales
- Finalización coordinada mediante `SIGTERM`

---

## Componentes IPC Utilizados

### 1. Memoria Compartida (System V)

Funciones utilizadas:

- `ftok`
- `shmget`
- `shmat`
- `shmdt`
- `shmctl`

Estructura:

```c
typedef struct {
    int datos[20];
} shared_memory;
```

La memoria solo contiene datos, nunca flags de control.

---

### 2. FIFOs (Named Pipes)

- `pipe_fifoA`
- `pipe_fifoB`

Se utilizan exclusivamente para intercambio inicial de PIDs.

Se implementa manejo robusto ante:

- `EEXIST`
- `ENXIO`
- Apertura no bloqueante

---

### 3. Señales como Mecanismo de Sincronización

Se emplean:

- `SIGUSR1`
- `SIGUSR2`
- `SIGTERM`

Las señales activan flags `volatile sig_atomic_t`, garantizando:

- Seguridad en acceso concurrente
- No ejecución de lógica compleja dentro del handler
- Separación entre señal y decisión

---

## Cifrado Implementado

Función pública:

\[
f(x) = (4x + 5) \mod 27
\]

Función inversa:

\[
f^{-1}(x) = (7x + 19) \mod 27
\]

Cada proceso:

- Cifra antes de escribir
- Descifra al leer

Esto introduce una capa adicional de transformación de datos sobre IPC.

---

## Control de Estado de Proceso

Se implementa:

```c
kill(pid, 0)
```

Para verificar existencia del proceso remoto antes de finalizar.

Esto evita zombies y asegura terminación limpia.

---

## Terminación Coordinada

Proceso A:

- Envía `SIGTERM` a B
- Espera confirmación de muerte
- Libera memoria compartida
- Elimina FIFOs

Proceso B:

- Se desasocia
- Elimina su FIFO
- Finaliza

Se garantiza:

- No fuga de memoria
- No recursos IPC huérfanos
- No procesos zombies

---

## Compilación

```bash
gcc shared_memory_ipc.c -o shared_memory_ipc -lrt
```

---

## Ejecución

Terminal 1:

```bash
./shared_memory_ipc A
```

Terminal 2:

```bash
./shared_memory_ipc B
```

Finalización:

```bash
kill -SIGTERM <PID_de_A>
```

---

# Decisiones de Diseño Destacadas

- Uso de `volatile sig_atomic_t` en vez de tipos primitivos simples
- Separación estricta entre datos y control
- No utilización de busy waiting
- Uso de `pause()` para espera eficiente
- Manejo explícito de errores (`errno`)
- Liberación manual de recursos IPC
- Implementación de protocolo de alternancia sin mutexes

---

# Tecnologías y Conceptos

- C (GNU)
- POSIX Signals
- System V Shared Memory
- FIFOs
- fork / exec
- alarm
- kill
- IPC
- Concurrencia sin threads
- Sincronización asincrónica

---

Este repositorio demuestra dominio en programación de sistemas, diseño concurrente y gestión explícita de recursos a bajo nivel en entornos Linux.
