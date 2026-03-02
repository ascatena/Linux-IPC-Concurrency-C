#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

// Definicion global para la clave.
#define NUMERO 123

// Flag de señal de terminar.
volatile sig_atomic_t signalTerm = 0;
volatile sig_atomic_t lectA = true, lectB = true;
volatile sig_atomic_t escA = false, escB = false;
volatile sig_atomic_t signRecv = true;

// Estructura para las banderas R/W y datos.
typedef struct {
    int datos[20];
} shared_memory;
////////////////////////////////////////////

// Manejadores de señales.
void sigterm(int signum) {
    signRecv = true;
    signalTerm = 1;
}
void sigusr1A(int signum) {
    signRecv = true;
    escB = true;
}
void sigusr2A(int signum) {
    signRecv = true;
    lectB = true;
}
void sigusr1B(int signum) {
    signRecv = true;
    escA = true;
}
void sigusr2B(int signum) {
    signRecv = true;
    lectA = true;
}
/////////////////////////

// Funciones de cifrado y otras.
int cifrar(int x);
int descifrar(int x);
int cheq_proc(pid_t pid);
///////////////////////

int main(int argc, char *argv[]) {
    // Declaraciones.
    // Correspondientes a la memoria compartida.
    int shm_ID;
    shared_memory *shm;

    // Para usar PID y mandar seniales.
    pid_t pid;
    pid_t pid_Externo;

    // Clave para la memoria.
    key_t shm_key = ftok("/bin/rmdir", NUMERO);
    if (shm_key == -1) {
        perror("Error al generar la clave para la memoria compartida \n");
        exit(1);
    }

    // Declaraciones para PIPE.
    int fd;         // Descriptor de archivo.
    int bufferPipe; // Buffer para enviar/recibir el PID.

    srand(time(NULL)); // Semilla para rand(), sirve para la escritura.

    // Creacion de memoria compartida.
    shm_ID = shmget(shm_key, sizeof(shared_memory),
                    0700 | IPC_CREAT); // Permisos de ejecución creo que no
                                       // tienen sentido en shm
    if (shm_ID == -1)                  // Le asigno solo al usuario propietario permisos de
                                       // lectura, escritura y ejecucion
    {
        perror("Error al crear la memoria compartida");
        exit(2); // Error de uso de comandos
    }
    //////////////////////////////////

    // Asociacion de memoria.
    shm = (shared_memory *)shmat(shm_ID, NULL,
                                 0); // La conversión se debe a que shmat devueleve un puntero a char
    if (shm == (void *)(-1))         // shmat devuelve (void*)(-1) en caso de error, asi
                                     // que lo convierto
    {
        perror("Error al asociar la memoria compartida");
        exit(3); // Error de archivo no encontrado.
    }
    ////////////////////////

    // Crear PIPEs nombrado FIFOs.
    if (mkfifo("pipe_fifoA", 0666) == -1) {
        if (errno != EEXIST) {
            perror("Error al crear pipe_fifoA");
            exit(4);
        }
    }
    if (mkfifo("pipe_fifoB", 0666) == -1) {
        if (errno != EEXIST) {
            perror("Error al crear pipe_fifoB");
            exit(4);
        }
    }
    ///////////////////////////

    // COMIENZO DE LOGICA DE CADA PROCESO //
    if (argc > 1 && strcasecmp(argv[1], "a") == 0) {

        signal(SIGUSR1, sigusr1A);
        signal(SIGUSR2, sigusr2A);
        signal(SIGTERM, sigterm);

        printf("Soy el \033[0;32mproceso A\033[0m y mi PID es "
               "\033[0;32m%d\033[0m\n",
               getpid());
        printf("Espero señal \033[0;33mSIGTERM\033[0m si desea "
               "finalizar \n");

        // Envio PID proceso A.
        bool una_linea = true;
        while ((fd = open("pipe_fifoA", O_WRONLY | O_NONBLOCK)) == -1) {
            if (errno == ENXIO) { // Error si no hay ningún lector en el FIFO
                if (una_linea) {
                    perror("\033[0;33mEsperando proceso lector para "
                           "pipe_fifoA\033[0m\n");
                }
                una_linea = false;
                if (signalTerm) { // Si recibe SIGTERM debe terminar. El proceso
                                  // B se supone no activo.
                    signRecv = false;
                    signalTerm = 0;
                    printf("\033[2J"); // Limpia la terminal
                    printf("\033[H");  // Reset de cursor
                    printf("Libero recursos de memoria compartida.\n "
                           "\n\033[0;33mFinalizando...\033[0m \n");
                    shmdt((const void *)shm);
                    if (shmctl(shm_ID, IPC_RMID, (struct shmid_ds *)NULL) == -1) {
                        perror("\033[0;31mError al marcar el segmento "
                               "para eliminación\033[0m");
                    }
                    remove("pipe_fifoA");
                    remove("pipe_fifoB");
                    exit(0);
                }
            }
        }
        bufferPipe = getpid(); // PID del proceso A.
        write(fd, &bufferPipe, sizeof(bufferPipe));
        close(fd);
        printf("\033[0;33mEnvio mi PID\033[0m \n");

        // Recibo PID de proceso B
        fd = open("pipe_fifoB", O_RDONLY);
        if (fd == -1) {
            perror("\033[0;31mError al abrir pipe_fifoB en lectura\033[0m");
            exit(6);
        }
        read(fd, &bufferPipe, sizeof(bufferPipe));
        close(fd);
        pid_Externo = bufferPipe;
        printf("Recibo el PID del \033[0;35mproceso B: %d \n\033[0m", pid_Externo);

        // Bucle principal proceso A
        bool saltear = false;
        while (1) {
            if (signRecv) {
                // Si esta habilitada la ESCRITURA para A (osea que B terminó de
                // leer).
                if (lectB) {
                    signRecv = false;
                    lectB = false;
                    printf("\n\033[0;32mProceso A\033[0m escribiendo primeros 10 \n");
                    for (int i = 0; i < 10; i++) {
                        int rnum = rand() % 27; // lo restrinjo entre 0 y 26
                        shm->datos[i] = cifrar(rnum);
                        printf("Escribo \033[0;36m%d\033[0m (C: %d), en shm[%d] \n", rnum, shm->datos[i], i);
                        sleep(1);         // Simular un retraso
                        if (signalTerm) { // por si recibe sigterm mientras escribe,
                                          // rompe el proceso  se dedica a finalizar
                            saltear = true;
                        }
                    }
                    // Le aviso a B que puede leer.
                    if (!saltear) {
                        printf("\n\033[0;32mProceso A\033[0m permite leer "
                               "al\033[0;35m Proceso B\033[0m \n");
                        kill(pid_Externo, SIGUSR1);
                    }
                }
                // Si esta habilitada la LECTURA para A.
                else if (escB) {
                    signRecv = false;
                    escB = false;
                    printf("\n\033[0;32mProceso A\033[0m leyendo ultimos 10 \n");
                    for (int i = 10; i < 20; i++) {
                        int rnum_d = descifrar(shm->datos[i]);
                        printf("Leyendo %d (D: \033[0;36m%d\033[0m) en shm[%d]\n", shm->datos[i], rnum_d, i);
                        sleep(1);
                        if (signalTerm) { // por si recibe sigterm mientras escribe,
                                          // rompe el proceso  se dedica a finalizar
                            saltear = true;
                        }
                    }
                    // Le aviso a B que puede escribir
                    if (!saltear) {
                        printf("\n\033[0;32mProceso A \033[0mpermite escribir al "
                               "\033[0;35mProceso "
                               "B \033[0m\n");
                        kill(pid_Externo, SIGUSR2);
                    }
                }
                // Verifica si se recibió la señal SIGTERM
                if (signalTerm) {
                    signRecv = false;
                    signalTerm = 0;
                    printf("\033[2J"); // Limpia la terminal
                    printf("\033[H");  // Reset de cursor
                    printf("Envio \033[0;33mSIGTERM\033[0m al \033[0;35mproceso "
                           "B\033[0m \n");
                    kill(pid_Externo, SIGTERM);
                    while (cheq_proc(pid_Externo) == 1) // Espero que muera B
                        sleep(1);
                    printf("\033[0;35mProceso B terminado\033[0m.\n"); // Me aseguro
                                                                       // que B se
                                                                       // desasoció
                                                                       // y terminó
                    printf("Libero recursos de memoria compartida.\n "
                           "\n\033[0;33mFinalizando...\033[0m \n");
                    shmdt((const void *)shm);
                    if (shmctl(shm_ID, IPC_RMID, (struct shmid_ds *)NULL) == -1) {
                        perror("\033[0;31mError al marcar el segmento a "
                               "eliminar\033[0m");
                    }
                    remove("pipe_fifoA");
                    exit(0);
                }
            } else if (!signRecv) // Si no recibió señal alguna, espera a hacerlo. No
                                  // puede decidir que acción tomar sin la comunicación
                                  // del proceso.
            {
                pause();
            }
        }
    } else if (argc > 1 && strcasecmp(argv[1], "b") == 0) {
        signal(SIGUSR1, sigusr1B);
        signal(SIGUSR2, sigusr2B);
        signal(SIGTERM, sigterm);

        printf("\nSoy el \033[0;35mproceso B\033[0m y mi PID es "
               "\033[0;35m%d\033[0m\n",
               getpid());
        printf("Solo \033[0;32mProceso A\033[0m puede eliminarme!\n");

        // Recibo PID proceso A.
        fd = open("pipe_fifoA", O_RDONLY);
        if (fd == -1) {
            perror("\033[0;31mError al abrir pipe_fifoA en lectura\033[0m");
            exit(1);
        }
        read(fd, &bufferPipe, sizeof(bufferPipe));
        close(fd);
        pid_Externo = bufferPipe;
        printf("Recibo el PID del \033[0;32mproceso A: %d\033[0m \n", pid_Externo);
        ////////////////////////

        // Envio de PID Proceso B.
        fd = open("pipe_fifoB", O_WRONLY);
        if (fd == -1) {
            perror("\033[0;31mError al abrir pipe_fifoB en escritura\033[0m");
            exit(1);
        }
        bufferPipe = getpid();
        write(fd, &bufferPipe, sizeof(bufferPipe));
        close(fd);
        printf("\033[0;33mEnvio mi PID\033[0m \n");
        //////////////////////////
        bool saltear = false;
        // Bucle principal proceso B
        while (1) {
            if (signRecv) {
                // Si esta activada la ESCRITURA para B.
                if (lectA) {
                    signRecv = false;
                    lectA = false;
                    printf("\n\033[0;35mProceso B\033[0m escribiendo ultimos 10 \n");
                    for (int i = 10; i < 20; i++) {
                        int rnum = rand() % 27;
                        shm->datos[i] = cifrar(rnum);
                        printf("Escribo \033[0;36m%d\033[0m (C: %d) en shm[%d]\n", rnum, shm->datos[i], i);
                        sleep(1);
                        if (signalTerm) { // por si recibe sigterm mientras lee,
                                          // rompe el proceso  se dedica a finalizar
                            saltear = true;
                        }
                    }
                    if (!saltear) {
                        // Se le notifica al proceso A que puede leer.
                        printf("\n\033[0;35mProceso B\033[0m permite leer al "
                               "\033[0;32mProceso A\033[0m \n");
                        kill(pid_Externo, SIGUSR1);
                    }
                }

                // Si esta habilitada la LECTURA para B.
                else if (escA) {
                    signRecv = false;
                    escA = false;
                    printf("\n\033[0;35mProceso B\033[0m leyendo primeros 10 \n");
                    for (int i = 0; i < 10; i++) {
                        int rnum_d = descifrar(shm->datos[i]);
                        printf("Leyendo %d (D: \033[0;36m%d\033[0m) en shm[%d]\n", shm->datos[i], rnum_d, i);
                        sleep(1);
                        if (signalTerm) { // por si recibe sigterm mientras lee,
                                          // rompe el proceso  se dedica a finalizar
                            saltear = true;
                        }
                    }
                    // Se le notifica al proceso A que puede escribir.
                    if (!saltear) {
                        printf("\n\033[0;35mProceso B\033[0m permite escribir al "
                               "\033[0;32mProceso "
                               "A\033[0m \n");
                        kill(pid_Externo, SIGUSR2);
                    }
                }
                if (signalTerm) {
                    /* printf("\033[2J"); // Limpia la terminal
                     printf("\033[H");  // Reset de cursor*/
                    printf("\n \033[0;33mFinalizando...\033[0m \n");
                    signRecv = false;
                    signalTerm = 0;
                    shmdt((const void *)shm);
                    remove("pipe_fifoB");
                    exit(0);
                }
            }
            if (!signRecv) // Si no recibió señal alguna, espera a hacerlo. No
                           // puede decidir que acción tomar sin la comunicación
                           // del proceso.
            {
                pause();
            }
        }
    } else {
        perror("Debe ingresarse el parametro \033[0;33m'a'\033[0m o "
               "\033[0;33m'b'\033[0m");
    }
    return 0;
}

//////* Funciones *//////
//  Función para cifrar.
int cifrar(int x) { return (4 * x + 5) % 27; }
// Función para descifrar
int descifrar(int x) { return (7 * x + 19) % 27; }
// Función de comprobación de proceso activo
int cheq_proc(pid_t pid) {
    if (kill(pid, 0) == 0) { // proceso activo
        return 1;
    } else if (errno == ESRCH) { // proceso no existe
        return 0;
    } else { // error
        perror("\033[0;31mError al comprobar estado del proceso\033[0m");
        return -1;
    }
}
///////////
