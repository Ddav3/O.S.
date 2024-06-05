/************************************
 *VR485815
 *Davide Sala
 *Data di realizzazione  <--------------------------------------------------------------
 *************************************/

#include <stdio.h>
#include <sys/sem.h>
#include <signal.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#define semNum 4

// Variabili Globali, Strutture e Funzioni -----------------------------------------------------------
char *name;
int shmId = -2, msgId = -2, semId = -2;
int s; // variabile provvisoria per informare il server

typedef union semUnion
{
    int val;
    struct semid_ds *dataStruct; //!!
    unsigned short *array;

} mySU;

typedef struct myMsg
{
    long Type;
    char Text[512];
} my;
my receiver;
size_t msgSize = 512;

typedef struct sharedMemoryStruct
{
    char table[3][3];
    pid_t Server;
    pid_t Client1;
    pid_t Client2;
    int onGame;
    pid_t current;
    int move;

} shMem;
shMem *memPointer;

//->Funzioni
void print(char *txt)
{
    if (write(1, txt, strlen(txt)) < 0)
    {
        perror("Error in writing messange");
    }
}

void closure()
{
    if (&memPointer != 0)
    {
        if (shmdt(memPointer) < 0)
        {
            perror("Shared Memory Detaching Error");
            return;
        }
        semId = -4;
    }

    free(name);

    kill(s, SIGUSR2); // quando arriva di là un sigusr2, il server deve fare qualcosa per chiudere
    return;
}

int main(int argc, char *argv[])
{

    // controllo degli argomenti ---------------------------------------------------------------//
    if (argc == 3)
    {
        if (argv[2] != "*")
        {
            printf("Inserimento scorretto dei parametri richiesti.\nIl giocatore deve indicare (spaziati): il proprio nome ed eventualmente * per giocare contro la CPU.\nRiprovare\n");
            return 0;
        }
    }
    else
    {
        if (argc != 2)
        {
            print("Inserimento scorretto dei parametri richiesti.\nIl giocatore deve indicare (spaziati): il proprio nome ed eventualmente * per giocare contro la CPU.\nRiprovare\n");
            return 0;
        }
        else
        {
            name = (char *)malloc(strlen(argv[1]) * sizeof(char));
            name = argv[2];
        }
    }
    //----------------------------------------------------------------------------------------//
    return 0;
}