#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>
#include <signal.h> //En esta están todos los nombres de las señales y signal y demás declarado
#include <sys/types.h>
#include <stdlib.h>
#include <sys/wait.h>

volatile bool _5seg = true, usr1 = false, usr2 = false, Ssigterm = false; // Declaro como volatile para que el compilador no haga optimizaciones raras sobre las variables
volatile int contador = 0;
int cant_procesos = 0;

// Defino una estructura para guardar los PID de los hijos y la conecto con una estructura siguiente (Listas dinámicas de progra)
typedef struct nodo
{
    pid_t pid;
    struct nodo *siguiente;
} nodo;

nodo *head = NULL; // Creo una primera estructura (por eso head)

void agregar_pid(pid_t pid); // Funcion para crear un nuevo nodo de la lista y asignarle el pid del nuevo hijo
void eliminar_hijos();       // Funcion para recorrer la lista dinamica y matar los hijos y liberar el nodo

/* Manejadores de las señales */
void sigusr1(int signum)
{
    printf("Recibí la señal USR1\n");
    usr1 = true;
}
void sigusr2(int signum)
{
    printf("Recibí la señal USR2\n");
    usr2 = true;
}
void sigalarm(int signum)
{
    _5seg = true; // Activo la bandera de 5 segundos
}
void sigterm(int signum)
{
    printf("Recibí la señal SIGTERM\n");
    Ssigterm = true;
}


int main(int argc, char *argv[])
{
    pid_t var = getpid(); // Inicia siendo el pid del padre

    signal(SIGUSR1, sigusr1); // NOTA:Hay otra función parecida que la recomiendan en un par de foros, signalaction, es mejor?
    signal(SIGUSR2, sigusr2);
    signal(SIGTERM, sigterm);

    printf("Soy el \033[0;32mpadre\033[0m y mi PID es: \033[0;32m%d\033[0m. Espero señales.\n", getpid());
    while (true)
    {
        if (var > 0)
        { // Proceso del PADRE
            pause();        //El Proceso Padre espera a la llegada de alguna señal

            if (Ssigterm)   // Si recibe SIGTERM elimina los prcesos hijos creados a partir de recibir USR1
            {
                printf("Terminando todos los procesos hijos creados por USR1...\n");
                eliminar_hijos();
                break;
            }
            if (usr1)       // Si recibe USR1, lleva un conteo de cuantos hijos creó y realiza el fork.
            {
                cant_procesos++;
                printf("Procesos hijo creados por USR1: %d\n",cant_procesos);
                var = fork();
                if (var > 0)        //Agrego el PID del hijo yla variable usr1 la cambio  solo para el proceso PADRE
                {
                    agregar_pid(var);
                    usr1 = false;
                }
                else if(var==0){    // Los manejadores se los quito al proceso hijo ya que no los usará y SIGTERM tiene que andar con su manejador defaut así termina.
                    signal(SIGALRM, sigalarm);  //establezco un manejadr para alarm
                    signal(SIGUSR1, SIG_DFL);
                    signal(SIGUSR2, SIG_DFL);
                    signal(SIGTERM, SIG_DFL);
                }
                else                // Si VAR es negativa entonces ocurrió un error en el fork
                    perror("Error al crear el Proceso Hijo\n");
            }
            if (usr2)       //Si recibe USR2, realiza el fork, generando el proceso hijo copia del padre.
            {
                var = fork();
                if (var > 0)        //Reinicio usr2 solo para el padre, para el hijo sigue siendo true y la uso para identificar procesos USR1 y procesos USR2
                    usr2 = false;
                else if(var==0){    //Quito los manejadores para el proceso hijo
                    signal(SIGUSR1, SIG_DFL);
                    signal(SIGUSR2, SIG_DFL);
                    signal(SIGTERM, SIG_DFL);
                }
                else    //idem error
                    perror("Error al crear el Proceso Hijo\n");
            }
        }
        else if (var == 0)
        { // Procesos HIJOS
            if (usr1)   //Para los procesos hijos que tienen usr1=true, esta es su rutina
            {
                if (_5seg)
                {
                    _5seg = false;
                    pid_t pid_hijo = getpid();
                    printf("Soy el \033[0;36mhijo\033[0m y mi PID es: \033[0;36m%d\033[0m\n", pid_hijo);
                    printf("El PID de mi \033[0;32mpadre\033[0m es: \033[0;32m%d\033[0m\n", getppid());
                    alarm(5);
                }
            }
            else if (usr2)  //Para los procesos hijos que tienen usr2=true, esta es su rutina
            {
                printf("Soy el proceso hijo (PID=\033[0;36m%d\033[0m) creado por SIGUSR2 y ejecutaré \033[0;33mls\033[0m:\n", getpid());
                execl("/bin/ls", "ls", NULL);   //Ejecuto el comando de bash UNIX 'ls'
                exit(0);        //Termino el proceso hijo con exit
            }
        }
    }
    exit(0);
};

void agregar_pid(pid_t pid)
{
    nodo *nuevo = malloc(sizeof(nodo)); // reservo memoria para el nuevo nodo de la lista
    nuevo->pid = pid;                   // Asigno el valor de pid pasado como argumento al dato que pertenece a la estructura del nodo
    nuevo->siguiente = head;            // Apunto como nodo siguiente el nodo más reciente (OSEA QUE VA A SER LIFO)
    head = nuevo;                       // actualizo como nodo actual el recién creado
}

void eliminar_hijos()
{
    nodo *temp; // Puntero temporal que apunte al inicio de la lista (último nodo creado last-in)
    while (head != NULL)
    {
        temp = head; // en el temporal traigo el nodo actual (siempre será el último agregado)
        printf("Terminando con el hijo cuyo PID es: \033[0;31m%d\033[0m\n", temp->pid);
        kill(temp->pid, SIGTERM);
        head = head->siguiente; // acrualizo el nodo actual con el que le sigue
        free(temp);             // libero el temporal
        sleep(1);
    }
}
