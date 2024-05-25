/************************************
 *VR485815
 *Davide Sala
 *Data di realizzazione  <--------------------------------------------------------------
 *************************************/

#include <stdio.h>
#include <sys/sem.h>
#include <signal.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <sys/shm.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

// variabili globali --------------------------------------------------------------//

sigset_t disabledSigSet;
int shmId = -3, semId = -4, msgId = -5;
char *name, choice[1];
struct sembuf p_ops[1];
struct sembuf v_ops[1];
int c1, c2, s;

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
    int onGame; // 1 = in partita, 0 = fuori partita
    pid_t current;
    int move; // #TODO decidi come fare la mossa

    // #TODO aggiungi campi

} shMem;
shMem *memPointer;

// Rimozione della memoria condivisa e di memoria allocata--------------------------//
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

    if (c1 == getpid())
    {
        kill(s, SIGUSR1);
    }
    else if (c2 == getpid())
    {

        kill(s, SIGUSR2);
    }
    return;
}

void disableSetandLock(int n)
{

    sigfillset(&disabledSigSet);
    sigprocmask(SIG_SETMASK, &disabledSigSet, NULL);

    if (semop(semId, &p_ops[n], 1) == -1)
    {
        perror("Error in Semaphore Operation (C, p)");
    }
}

void enableSetandUnlock(int n)
{
    if (semop(semId, &v_ops[n], 1) == -1)
    {
        perror("Error in Semaphore Operation (C, v)");
        closure();
    }

    sigdelset(&disabledSigSet, SIGINT);
    sigdelset(&disabledSigSet, SIGUSR1);
    sigdelset(&disabledSigSet, SIGUSR2);
    sigdelset(&disabledSigSet, SIGALRM);
    sigdelset(&disabledSigSet, SIGTERM);
    sigprocmask(SIG_SETMASK, &disabledSigSet, NULL);
}

void receiveMessage()
{
    if (msgrcv(msgId, &receiver, msgSize, receiver.Type, 0) < 0)
    {
        perror("Message reception Error");
        return;
    }
    write(1, receiver.Text, msgSize);
}
void sigHandlerC(int signal) // #TODO
{

    if (signal == SIGINT)
    {
        printf("Abbandono della partita in corso...\n");
        fflush(stdout);
        disableSetandLock(0);
        memPointer->move = getpid();
        kill(memPointer->Server, SIGTERM);
        enableSetandUnlock(0);
    }
    else if (signal == SIGUSR1) //
    {
        disableSetandLock(0);

        enableSetandUnlock(0);
    }
    else if (signal == SIGUSR2) // è arrivato un messaggio
    {
        receiveMessage();
    }

    else if (signal == SIGTERM) // c'è un abbandono #TODO gestisci i casi per non mischiarli
    {

        // #TODO valuta di migliorare qua sotto

        disableSetandLock(0);
        if (memPointer->onGame == 0)
        {
            c1 = memPointer->Client1;
            c2 = memPointer->Client2;
            s = memPointer->Server;
        }
        else
        {
        }
        enableSetandUnlock(0);
        closure();
    }
    else if (signal == SIGALRM)
    {
    }
    else
    {
    }
    return;
}

void showMatrix()
{
    disableSetandLock(0);
    printf("\n");
    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            printf(" %c ", memPointer->table[i][j]);
            fflush(stdout);
            if (j != 2)
            {
                printf("|");
                fflush(stdout);
            }
        }
        if (i != 2)
        {

            printf("\n___|___|___\n");
            fflush(stdout);
        }
        else
        {
            printf("\n   |   |   \n");
            fflush(stdout);
        }
    }
    enableSetandUnlock(0);
}

// funzione che esegue la mossa del singolo giocatore
void makeMove()
{
    disableSetandLock(0);
    if (choice[0] < '1' || choice[0] > '9')
    {

        memPointer->move = 0;
    }
    else
    {
        memPointer->move = (choice[0] - '0') - 1;
        printf("; %d è la tua choice", memPointer->move);
    }

    kill(memPointer->Server, SIGUSR1);
    enableSetandUnlock(0);
}

// Funzione dedicata alla stampa delle stringhe #TODO
void print(char *message)
{
}

int main(int argc, char *argv[])
{

    // controllo degli argomenti ---------------------------------------------------------------//
    if (argc == 3)
    {
        if (argv[2] != "*") // #TODO controlla che sia valido
        {
            printf("Inserimento scorretto dei parametri richiesti.\nIl giocatore deve indicare (spaziati): il proprio nome ed eventualmente * per giocare contro la CPU.\nRiprovare\n");
            return 0;
        }
    }
    else
    {
        if (argc != 2)
        {
            printf("Inserimento scorretto dei parametri richiesti.\nIl giocatore deve indicare (spaziati): il proprio nome ed eventualmente * per giocare contro la CPU.\nRiprovare\n");
            return 0;
        }
        else // si attende il secondo giocatore
        {
            name = (char *)malloc(strlen(argv[1]) * sizeof(char));
            name = argv[2];

            // #TODO semaforo per altro giocatore
        }
    }
    //----------------------------------------------------------------------------------------//

    // Predisposizione memoria condivisa, code di messaggi, semafori e segnali----------------------------------------//
    size_t size = sizeof(shMem);
    key_t key = ftok("./TriServer.c", 's');
    if (key < 0)
    {
        perror("Key error");
        return -1;
    }
    shmId = shmget(key, size, S_IWUSR | S_IRUSR);
    if (shmId < 0)
    {
        perror("Shared Memory ID creation Error");
        return -1;
    }

    msgId = msgget(ftok("./TriServer.c", 's'), S_IRUSR | S_IWUSR);
    if (msgId < 0)
    {
        perror("Message Queue association error");
        return -1;
    }

    memPointer = shmat(shmId, NULL, IPC_CREAT | S_IRUSR | S_IWUSR);
    if (memPointer == (void *)-1)
    {
        perror("Shared Memory Attachment Error");
        return -1;
    }

    semId = semget(ftok("./TriServer.c", 's'), 3, S_IRUSR | S_IWUSR);
    if (semId < 0)
    {
        perror("Semaphore ID creation Error");
        return -1;
    }

    for (int i = 0; i < 3; i++)
    {
        p_ops[i].sem_num = i;
        p_ops[i].sem_op = -1;
        p_ops[i].sem_flg = 0;

        v_ops[i].sem_num = i;
        v_ops[i].sem_op = 1;
        v_ops[i].sem_flg = 0;
    }

    sigfillset(&disabledSigSet);
    sigdelset(&disabledSigSet, SIGINT);
    sigdelset(&disabledSigSet, SIGUSR1);
    sigdelset(&disabledSigSet, SIGUSR2);
    sigdelset(&disabledSigSet, SIGALRM);
    sigdelset(&disabledSigSet, SIGTERM);
    sigprocmask(SIG_SETMASK, &disabledSigSet, NULL);
    signal(SIGINT, sigHandlerC);
    signal(SIGUSR1, sigHandlerC);
    signal(SIGUSR2, sigHandlerC);
    signal(SIGALRM, sigHandlerC);
    signal(SIGTERM, sigHandlerC); // userò questo segnale per informare della fine della partita
                                  //----------------------------------------------------------------------------------//

    // blocco di codice #TODO ----------------------------------------------------------//
    printf("Process Pid: %d\n", getpid());
    fflush(stdout);

    disableSetandLock(0);
    if (memPointer->Client1 == -11)
    {
        memPointer->Client1 = getpid();
        receiver.Type = 1;
        if (argc == 3)
        {
            // giocatore automatico
            memPointer->Client2 = -10;
            kill(memPointer->Server, SIGUSR1);
            enableSetandUnlock(0);
            pause();
        }
        else
        {
            enableSetandUnlock(0);
            // pause(); // #TODO controlla se c'è qualche caso da gestire
        }

        kill(memPointer->Server, SIGUSR1);
        printf("In attesa di un secondo giocatore...\n");
        fflush(stdout);
        enableSetandUnlock(0);
        pause();
    }
    else if (memPointer->Client2 == -12)
    {
        memPointer->Client2 = getpid();
        receiver.Type = 2;
        kill(memPointer->Server, SIGUSR2);
        enableSetandUnlock(0);
        pause();
    }
    else
    {
        printf("Partita occupata. Impossibile partecipare.");
        fflush(stdout);
        enableSetandUnlock;
    }

    // segnalazione mossa sbagliata (dopo mossa scelta)
    // Abbandono con CTRL C + notifica altro giocatore
    // Time out per mossa + conseguenze
    // Modalità automatica (1 o 2 player)

    // disableSetandLock(0);
    // while (memPointer->onGame == 1)
    // {
    //     if (memPointer->Client1 == getpid())
    //     {
    //         enableSetandUnlock(0);

    //         if (semop(semId, &p_ops[1], 1) < 0)
    //         {
    //             perror("Error in P operation (C1)");
    //             closure();
    //         }
    //     }
    //     else if (memPointer->Client2 == getpid())
    //     {
    //         enableSetandUnlock(0);

    //         if (semop(semId, &p_ops[2], 1) < 0)
    //         {
    //             perror("Error in P operation (C2)");
    //             closure();
    //         }
    //     }
    //     disableSetandLock(0);
    //     memPointer->current = getpid();
    //     enableSetandUnlock(0);

    //     showMatrix();

    //     while (read(0, choice, 1) < 0)
    //     {
    //         printf("Errore nella lettura.\nRiprovare");
    //         fflush(stdout);
    //     }
    //     makeMove();
    //     showMatrix();

    //     disableSetandLock(0);
    //     if (memPointer->Client1 == getpid())
    //     {
    //         enableSetandUnlock(0);
    //         if (semop(semId, &v_ops[2], 1) < 0)
    //         {
    //             perror("Error in P operation (C1)");
    //             closure();
    //         }
    //     }
    //     else if (memPointer->Client2 == getpid())
    //     {
    //         enableSetandUnlock(0);
    //         if (semop(semId, &v_ops[1], 1) < 0)
    //         {
    //             perror("Error in P operation (C2)");
    //             closure();
    //         }
    //     }
    //     disableSetandLock(0);
    // }
    // enableSetandUnlock(0);

    //------------------------------------------------------------------//

    printf("\nchiudo\n");

    return 0;
}