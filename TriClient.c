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
sigset_t disabledSigSet;
struct sembuf p_ops[semNum];
struct sembuf v_ops[semNum];

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
size_t size = sizeof(shMem);

//->Funzioni

void disableSigSet()
{
    sigfillset(&disabledSigSet);
    sigdelset(&disabledSigSet, SIGINT);
    sigprocmask(SIG_SETMASK, &disabledSigSet, NULL);
}

void enableSigSet()
{
    sigdelset(&disabledSigSet, SIGUSR1);
    sigdelset(&disabledSigSet, SIGUSR2);
    sigdelset(&disabledSigSet, SIGALRM);
    sigdelset(&disabledSigSet, SIGTERM);
    sigprocmask(SIG_SETMASK, &disabledSigSet, NULL);
}

void print(char *txt)
{
    if (write(1, txt, strlen(txt)) < 0)
    {
        perror("Error in writing messange");
    }
}

int quit = 0;
void closure()
{
    disableSigSet();
    if (semop(semId, &p_ops[0], 1) == -1)
    {
        perror("Error in Semaphore Operation (S, p, 50)");
        return;
    }
    s = memPointer->Server;

    if (memPointer->Client1 == getpid())
    {
        memPointer->Client1 = -11;
    }
    else if (memPointer->Client2 == getpid())
    {
        memPointer->Client2 = -12;
    }
    else
    {
        perror("PID unutilized");
    }

    if (semop(semId, &v_ops[0], 1) == -1)
    {
        perror("Error in Semaphore Operation (S, v, 50)");
        return;
    }
    enableSigSet();

    if (&memPointer != 0)
    {
        if (shmdt(memPointer) < 0)
        {
            perror("Shared Memory Detaching Error");
            return;
        }
        semId = -2;
    }

    free(name);

    kill(s, SIGUSR2); // quando arriva di là un sigusr2, il server deve fare qualcosa per chiudere

    return;
}

void receiveMessage()
{
    if (msgrcv(msgId, &receiver, msgSize, receiver.Type, 0) < 0)
    {
        perror("Message reception Error");
        return;
    }
    print(receiver.Text);
}

void sigHandlerC(int signal)
{
    disableSigSet();
    if (semop(semId, &p_ops[0], 1) == -1)
    {
        perror("Error in Semaphore Operation (C, p, 50)");
        return;
    }

    if (signal == SIGINT)
    {
        if (memPointer->current == -1)
        {
            print("Partita conclusa\n");
            closure();
        }
        else if (memPointer->current == getpid())
        {
            print("Ti sei arreso.\n");
            closure();
        }
    }
    else if (signal == SIGUSR2)
    {
        receiveMessage();
    }

    if (semop(semId, &v_ops[0], 1) == -1)
    {
        perror("Error in Semaphore Operation (C, v, 50)");
        return;
    }
    enableSigSet();
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

    // Predisposizione memoria condivisa, code di messaggi, semafori e segnali----------------------------------------//

    {
        key_t key = ftok("./TriServer.c", 's');
        if (key < 0)
        {
            perror("Key Creation error");
            return 0;
        }

        shmId = shmget(key, size, S_IWUSR | S_IRUSR);
        if (shmId < 0)
        {
            perror("Shared Memory ID creation Error");
            return 0;
        }

        memPointer = shmat(shmId, NULL, IPC_CREAT | S_IRUSR | S_IWUSR);
        if (memPointer == (void *)-1)
        {
            perror("Shared Memory Attachment Error");
            closure();
        }

        msgId = msgget(ftok("./TriServer.c", 's'), S_IRUSR | S_IWUSR);
        if (msgId < 0)
        {
            perror("Message Queue association error");
            return 0;
        }

        semId = semget(ftok("./TriServer.c", 's'), 3, S_IRUSR | S_IWUSR);
        if (semId < 0)
        {
            perror("Semaphore ID creation Error");
            return 0;
        }

        for (int i = 0; i < semNum; i++)
        {
            p_ops[i].sem_num = i;
            p_ops[i].sem_op = -1;
            p_ops[i].sem_flg = 0;

            v_ops[i].sem_num = i;
            v_ops[i].sem_op = 1;
            v_ops[i].sem_flg = 0;
        }
    }

    sigfillset(&disabledSigSet);
    sigdelset(&disabledSigSet, SIGINT);
    enableSigSet();
    signal(SIGINT, sigHandlerC);
    signal(SIGUSR1, sigHandlerC);
    signal(SIGUSR2, sigHandlerC);
    signal(SIGALRM, sigHandlerC);
    signal(SIGTERM, sigHandlerC); // userò questo segnale per informare della fine della partita
    //----------------------------------------------------------------------------------//

    // blocco di codice #TODO ----------------------------------------------------------//
    printf("Process Pid: %d\n", getpid());
    fflush(stdout);

    disableSigSet();
    if (semop(semId, &p_ops[0], 1) == -1)
    {
        perror("Error in Semaphore Operation (C, p, 0)");
        return 0;
    }

    if (memPointer->Client1 == -11)
    {
        memPointer->Client1 = getpid();
        receiver.Type = 1;
        // caso del bot #TODO: execl del bot, e termini il client

        kill(memPointer->Server, SIGUSR1);
        if (semop(semId, &v_ops[0], 1) == -1)
        {
            perror("Error in Semaphore Operation (C1, v, 1)");
            return 0;
        }
        enableSigSet();
        print("In attesa di un altro giocatore...\n");
        pause();
        disableSigSet();
        if (semop(semId, &p_ops[0], 1) == -1)
        {
            perror("Error in Semaphore Operation (C1, p, 1)");
            return 0;
        }

        // while (memPointer->onGame > 1)
        // {
        //     if (semop(semId, &v_ops[0], 1) == -1)
        //     {
        //         perror("Error in Semaphore Operation (C1, v, 2)");
        //         return 0;
        //     }
        //     enableSigSet();

        //     pause();
        //     disableSigSet();
        //     if (semop(semId, &p_ops[0], 1) == -1)
        //     {
        //         perror("Error in Semaphore Operation (C1, p, 1)");
        //         return 0;
        //     }
        // }
        if (semop(semId, &v_ops[0], 1) == -1)
        {
            perror("Error in Semaphore Operation (C1, v, 3)");
            return 0;
        }
        enableSigSet();
        closure();
        return 0;
    }
    else if (memPointer->Client2 == -12)
    {
        memPointer->Client2 = getpid();
        receiver.Type = 2;

        kill(memPointer->Server, SIGUSR1);
        if (semop(semId, &v_ops[0], 1) == -1)
        {
            perror("Error in Semaphore Operation (C2, v, 1)");
        }
        enableSigSet();

        pause();

        disableSigSet();
        if (semop(semId, &p_ops[0], 1) == -1)
        {
            perror("Error in Semaphore Operation (C2, p, 1)");
        }

        // while (memPointer->onGame > 1)
        // {
        //     if (semop(semId, &v_ops[0], 1) == -1)
        //     {
        //         perror("Error in Semaphore Operation (C2, v, 2)");
        //     }
        //     enableSigSet();

        //     disableSigSet();
        //     if (semop(semId, &p_ops[0], 1) == -1)
        //     {
        //         perror("Error in Semaphore Operation (C2, p, 1)");
        //     }
        // }
        if (semop(semId, &v_ops[0], 1) == -1)
        {
            perror("Error in Semaphore Operation (C2, v, 3)");
        }
        enableSigSet();
        closure();
        return 0;
    }
    else
    {
        print("Partita occupata.\n");
        closure();
    }

    return 0;
}