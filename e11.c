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
///////////////////////////////////

// Flag de señal de terminar.
volatile sig_atomic_t signalTerm = 0;
volatile sig_atomic_t flag_1 = 1;       // flag_1 = 1 -> A puede escribir. flag_1 = 0 -> B puede leer.
volatile sig_atomic_t flag_2 = 1;       // flag_2 = 1 -> B puede escribir. flag_2 = 0 -> A puede leer.
volatile sig_atomic_t signRecv = true; // Flag que indica que se recibio una senial.
/////////////////////////////

// Estructura para datos en memoria compartida.
typedef struct {
    int datos[20];
} shared_memory;
////////////////////////////////////////////

// Manejadores de señales.
void sigterm(int signum) {
    signalTerm = 1;
    signRecv = true; 
}
void sigusr1(int signum) { 
    flag_1 = !flag_1;       // Invierte siempre el estado de la bandera.
    signRecv = true;
}
void sigusr2(int signum) { 
    flag_2 = !flag_2;       // Invierte siempre el estado de la bandera.
    signRecv = true;
}
/////////////////////////

// Funciones de cifrado.
int cifrar(int x);
int descifrar(int x);
///////////////////////

int main(int argc, char *argv[]) {
    /* Declaraciones. */

    // Para usar PID y mandar seniales.
    pid_t pid;
    pid_t pid_Externo;

    // Correspondientes a la memoria compartida.
    int shm_ID;
    shared_memory *shm;
    ////////////////////////////////////////////

    // Clave para la memoria.
    key_t shm_key = ftok("/bin/mkdir", NUMERO);
    if (shm_key == -1) {
        perror("Error al generar la clave para la memoria compartida \n");
        exit(1);
    }
    /////////////////////////

    // Declaraciones para PIPE.
    int fd;                     // Descriptor de archivo.
    int bufferPipe;             // Buffer para enviar/recibir el PID.
    //////////////////////////

    srand(time(NULL));          // Semilla para rand(), sirve para la escritura.

    // Creacion de memoria compartida.
    shm_ID = shmget(shm_key, sizeof(shared_memory), 0700 | IPC_CREAT);
    if (shm_ID == -1){
        perror("Error al crear la memoria compartida");
        exit(2);
    }
    //////////////////////////////////

    // Asociacion de memoria.
    shm = (shared_memory *)shmat(shm_ID, NULL,0);
    if (shm == (void *)(-1)){
        perror("Error al asociar la memoria compartida");
        exit(3);
    }
    ////////////////////////

    // Creaacion de PIPES para compartir PID's.
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
    ///////////////////////////////////////////

    /* COMIENZO DE LOGICA DE CADA PROCESO */
    if (argc > 1 && strcasecmp(argv[1], "a") == 0) {
        signal(SIGUSR1,sigusr1);
        signal(SIGUSR2,sigusr2);
        signal(SIGTERM, sigterm);

        printf("Soy el \033[0;32mproceso A\033[0m y mi PID es \033[0;32m%d\033[0m\n",getpid());
        printf("Quedo a la espera de señal \033[0;33mSIGTERM\033[0m si desea finalizar \n");

        // Envio PID proceso A.
        bool una_linea = true;
        while ((fd = open("pipe_fifoA", O_WRONLY | O_NONBLOCK)) == -1) {
            if (errno == ENXIO) {       // Error si no hay ningún lector en el FIFO.
                if (una_linea) {
                    printf("\033[0;33mEsperando proceso lector para pipe_fifoA\033[0m\n");
                }
                una_linea = false;
                if (signalTerm) { // Si recibe SIGTERM debe terminar. El proceso B se supone no activo.
                    signalTerm = 0;
                    printf("\033[2J"); // Limpia la terminal
                    printf("\033[H");  // Reset de cursor
                    printf("Libero recursos de memoria compartida.\n \n\033[0;33mFinalizando...\033[0m \n");
                    shmdt((const void *)shm);
                    if (shmctl(shm_ID, IPC_RMID, (struct shmid_ds *)NULL) ==-1) {
                        perror("\033[0;31mError al marcar el segmento para eliminación\033[0m");
                    }
                    remove("pipe_fifoA");
                    remove("pipe_fifoB");
                    exit(0);
                }
            }
        }
        bufferPipe = getpid();                          // PID del proceso A.
        write(fd, &bufferPipe, sizeof(bufferPipe));     // Cargado al PIPE A.
        close(fd);
        printf("\033[0;33mEnvio mi PID\033[0m \n");
        ///////////////////////

        // Recibo PID de proceso B
        fd = open("pipe_fifoB", O_RDONLY);
        if (fd == -1) {
            perror("\033[0;31mError al abrir pipe_fifoB en lectura\033[0m");
            exit(6);
        }
        read(fd, &bufferPipe, sizeof(bufferPipe));
        close(fd);
        pid_Externo = bufferPipe;                       // Guardo el PID de PIPE B como pid_Externo.
        printf("Recibo el PID del \033[0;35mproceso B: %d \n\033[0m",pid_Externo);
        ///////////////////////////

        bool saltear = false;
        /* BUCLE PRINCIPAL PROCESO A */
        while (1) {

            // Si se recibio alguna senial, ingresa.
            if(signRecv){

                // Si Proceso A puede escribir (flag_1 == 1). ESCRIBE A.
                if (flag_1 == 1) {
                    signRecv = false;       // Indico que se recibio una senial, hubo un cambio.
                    printf("\n\033[0;32mProceso A\033[0m escribiendo primeros 10 \n");
                    for (int i = 0; i < 10; i++) {
                        int rnum = rand() % 27;
                        shm->datos[i] = cifrar(rnum);
                        printf("Escribo \033[0;36m%d\033[0m (C: %d), en shm[%d] \n", rnum, shm->datos[i], i);
                        sleep(1);
                        if(signalTerm){     // Por si recibio mientras estaba escribiendo. Saltea a finalizar.
                            saltear = true;
                        }
                    }
                    if(!saltear){
                        kill(pid_Externo,SIGUSR1);  // B podra leer. Mando senial y el manejador se ajusta flag_1 = 0.
                        flag_1 = 0;                 // Actualizo Flag para este proceso y que no se repita indeseadamente.
                    }
                }
                ////////////////////////////////////////////////////////

                // Si Proceso A puede leer (flag_2 == 0). LEE A.
                else if (flag_2 == 0) {
                    signRecv = false;       // Indico que se recibio una senial, hubo un cambio.          
                    printf("\n\033[0;32mProceso A\033[0m leyendo ultimos 10 \n");
                    for (int i = 10; i < 20; i++) {
                        int rnum_d = descifrar(shm->datos[i]);
                        printf("Leyendo %d (D: \033[0;36m%d\033[0m) en shm[%d]\n", shm->datos[i], rnum_d, i);
                        sleep(1);
                        if(signalTerm){     // Por si recibio mientras estaba leyendo. Saltea a finalizar.
                            saltear = true;
                        }
                    }
                    if(!saltear){
                    kill(pid_Externo,SIGUSR2);  // B podra escribir. Mando senial y el manejador se ajusta flag_2 = 1.
                    flag_2 = 1;                 // Actualizo Flag para este proceso y que no se repita indeseadamente.    
                    }
                }
                ////////////////////////////////////////////////

                // Verifica si se recibió la señal SIGTERM. 
                if (signalTerm) {
                    signRecv = false;                                // Indico que se recibio una senial, hubo un cambio.
                    signalTerm = 0;
                    printf("\033[2J");                              // Limpia la terminal.
                    printf("\033[H");                               // Reset de cursor.
                    printf("Envio \033[0;33mSIGTERM\033[0m al \033[0;35mproceso B\033[0m \n");
                    kill(pid_Externo, SIGTERM);
                    waitpid(pid_Externo, NULL, 0);                  // Espero que muera B.
                    printf("\033[0;35mProceso B terminado\033[0m.\n");
                    printf("Libero recursos de memoria compartida.\n \n\033[0;33mFinalizando A...\033[0m \n");
                    shmdt((const void *)shm);
                    if (shmctl(shm_ID, IPC_RMID, (struct shmid_ds *)NULL) == -1) {
                        perror("\033[0;31mError al marcar el segmento a eliminar\033[0m");
                    }
                    remove("pipe_fifoA");
                    exit(0);
                }
            } else if (!signRecv){
                // PAUSA si no se recibio ninguna senial.
                pause(); // No puede decidir que acción tomar sin la comunicación del proceso.
            }
        }
    }
    else if (argc > 1 && strcasecmp(argv[1], "b") == 0) {
        signal(SIGUSR1, sigusr1);
        signal(SIGUSR2, sigusr2);
        signal(SIGTERM, sigterm);

        printf("\nSoy el \033[0;35mproceso B\033[0m y mi PID es \033[0;35m%d\033[0m\n", getpid());
        printf("Solo \033[0;32mProceso A\033[0m puede eliminarme!\n");

        // Recibo PID proceso A.
        fd = open("pipe_fifoA", O_RDONLY);
        if (fd == -1) {
            perror("\033[0;31mError al abrir pipe_fifoA en lectura\033[0m");
            exit(1);
        }
        read(fd, &bufferPipe, sizeof(bufferPipe));
        close(fd);
        pid_Externo = bufferPipe;                   // Guardo el PID de PIPE A como pid_Externo.
        printf("Recibo el PID del \033[0;32mproceso A: %d\033[0m \n", pid_Externo);
        ////////////////////////

        // Envio de PID Proceso B.
        fd = open("pipe_fifoB", O_WRONLY);
        if (fd == -1) {
            perror("\033[0;31mError al abrir pipe_fifoB en escritura\033[0m");
            exit(1);
        }
        bufferPipe = getpid();                      // PID del Proceso B.
        write(fd, &bufferPipe, sizeof(bufferPipe));
        close(fd);
        printf("\033[0;33mEnvio mi PID\033[0m \n");
        //////////////////////////

        bool saltear = false;
        /* BUCLE PRINCIPAL PROCESO B */
        while (1) {
            // Si se recibio alguna senial, ingresa.
            if(signRecv){
                // Si Proceso B puede escribir (flag_2 == 1). ESCRIBE B.
                if (flag_2 == 1) {
                    signRecv = false;           // Indico que se recibio una senial, hubo un cambio.
                    printf("\n\033[0;32mProceso B\033[0m escribiendo ultimos 10 \n");
                    for (int i = 10; i < 20; i++) {
                        int rnum = rand() % 27;
                        shm->datos[i] = cifrar(rnum);
                        printf("Escribo \033[0;36m%d\033[0m (C: %d), en shm[%d] \n", rnum, shm->datos[i], i);
                        sleep(1);    
                        if(signalTerm){     // Por si recibio mientras estaba escribiendo. Saltea a finalizar.
                            saltear = true;
                        }
                    }
                    if(!saltear){
                        kill(pid_Externo,SIGUSR2);  // A podra leer. Mando senial y el manejador se ajusta flag_2 = 0.
                        flag_2 = 0;                 // Actualizo Flag para este proceso y que no se repita indeseadamente.
                    }
                }
                ////////////////////////////////////////////////////////

                // Si Proceso B puede leer (flag_1 == 0). LEE B.
                else if (flag_1 == 0) {
                    signRecv = false;
                    printf("\n\033[0;32mProceso B\033[0m leyendo primeros 10 \n");
                    for (int i = 0; i < 10; i++) {
                        int rnum_d = descifrar(shm->datos[i]);
                        printf("Leyendo %d (D: \033[0;36m%d\033[0m) en shm[%d]\n", shm->datos[i], rnum_d, i);
                        sleep(1);
                        if(signalTerm){     // Por si recibio mientras estaba escribiendo. Saltea a finalizar.
                            saltear = true;
                        }
                    }
                    if(!saltear){
                    kill(pid_Externo,SIGUSR1);  // A podra escribir. Mando senial y el manejador se ajusta flag_1 = 1.
                    flag_1 = 1;                 // Actualizo Flag para este proceso y que no se repita indeseadamente.
                    }
                }
                /////////////////////////////////////////////////
            
            // Verifica si se recibió la señal SIGTERM. 
                if (signalTerm) {
                    printf("\033[2J"); // Limpia la terminal
                    printf("\033[H");  // Reset de cursor
                    signRecv = false;
                    signalTerm = 0;
                    printf("Libero recursos de memoria compartida.\n \n\033[0;33mFinalizando B...\033[0m \n");
                    shmdt((const void *)shm);
                    remove("pipe_fifoB");
                    exit(0);
                }
                /////////////////////////////////////////
            }
            // PAUSA si no se recibio ninguna senial.
            if (!signRecv){
                pause();
            }
            /////////////////////////////////////////
        }
    } else {
        printf("Debe ingresarse el parametro \033[0;33m'a'\033[0m o\033[0;33m'b'\033[0m\n");
    }
    return 0;
}

/* Funciones */
//  Función para cifrar.
int cifrar(int x) { return (4 * x + 5) % 27; }
// Función para descifrar
int descifrar(int x) { return (7 * x + 19) % 27; }
///////////

