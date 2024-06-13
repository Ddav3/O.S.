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
char *name1, *name2;
int shmId = -2, msgId = -2, semId = -2;
int s, c1, c2; // variabili provvisorie per sostituire i pid in memoria
sigset_t disabledSigSet;
struct sembuf p_ops[semNum];
struct sembuf v_ops[semNum];
char choice[50];

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
    sigdelset(&disabledSigSet, SIGALRM);
    sigprocmask(SIG_SETMASK, &disabledSigSet, NULL);
}

void enableSigSet()
{
    sigdelset(&disabledSigSet, SIGUSR1);
    sigdelset(&disabledSigSet, SIGUSR2);
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
        perror("Error in Semaphore Operation (C, p, 50)");
        return;
    }
    s = memPointer->Server;
    c1 = memPointer->Client1;
    c2 = memPointer->Client2;

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

    printf("%s %s\n", name1, name2);
    fflush(stdout);
    if (c1 == getpid())
    {
        print("chiudo\n");
        free(name1);
    }
    else if (c2 == getpid())
    {
        print("chiudo\n");
        free(name2);
    }
    else
    {
        perror("No name was relesable");
        exit(-1);
    }
    print("closing..\n");
    printf("%s %s\n", name1, name2);

    kill(s, SIGUSR2); // quando arriva di là un sigusr2, il server deve fare qualcosa per chiudere

    exit(-1);
}

void showMatrix() // senza semafori
{
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
        perror("Error in Semaphore Operation (C, p, 60)");
        return;
    }

    if (signal == SIGINT)
    {
        if (memPointer->current == -1)
        {
            if (semop(semId, &v_ops[0], 1) == -1)
            {
                perror("Error in Semaphore Operation (C, v, 51)");
                return;
            }
            enableSigSet();

            print("Partita terminata.\n");

            closure();
            exit(0);
        }
        else
        {
            print("Ti sei arreso.\n");
            if (memPointer->Client1 == getpid())
            {
                memPointer->current = -10;
            }
            else if (memPointer->Client2 == getpid())
            {
                memPointer->current = -20;
            }
            memPointer->onGame = 1;
            kill(memPointer->Server, SIGINT);

            if (semop(semId, &v_ops[0], 1) == -1)
            {
                perror("Error in Semaphore Operation (C, v, 54)");
                return;
            }

            closure();
            exit(0);
        }
    }
    else if (signal == SIGUSR2)
    {
        receiveMessage();
    }
    else if (signal == SIGALRM)
    {
        if (semop(semId, &v_ops[0], 1) == -1)
        {
            perror("Error in Semaphore Operation (C, v, 500)");
            return;
        }
        enableSigSet();
        closure();
    }

    if (semop(semId, &v_ops[0], 1) == -1)
    {
        perror("Error in Semaphore Operation (C, v, 50)");
        return;
    }
    enableSigSet();
}

void makeMove() // senza semafori
{
    do
    {
        scanf("%s", choice);
    } while (choice[0] == '\n');

    if (choice[0] < '1' || choice[0] > '9')
    {
        memPointer->move = -1;
    }
    else
    {
        memPointer->move = (choice[0] - '0') - 1;
    }
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
    sigdelset(&disabledSigSet, SIGALRM);
    enableSigSet();
    signal(SIGINT, sigHandlerC);
    signal(SIGUSR1, sigHandlerC);
    signal(SIGUSR2, sigHandlerC);
    signal(SIGALRM, sigHandlerC);
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
        name1 = (char *)malloc(strlen(argv[1]) * sizeof(char));
        // name1 = argv[2];
        strncpy(name1, argv[1], strlen(argv[1]));

        {
            kill(memPointer->Server, SIGUSR1);
            if (semop(semId, &v_ops[0], 1) == -1)
            {
                perror("Error in Semaphore Operation (C1, v, 1)");
                return 0;
            }
            enableSigSet();
            print("In attesa di un altro giocatore...\n");
            pause();
        }

        while (1)
        {
            disableSigSet();
            if (semop(semId, &p_ops[1], 1) < 0)
            {
                perror("Error in Semaphore Operation (C1, p1, 1)");
                return 0;
            }
            memPointer->current = getpid();

            if (semop(semId, &p_ops[0], 1) < 0)
            {
                perror("Error in Semaphore Operation (C1, p, 2)");
                return 0;
            }

            showMatrix();

            if (semop(semId, &v_ops[0], 1) < 0)
            {
                perror("Error in Semaphore Operation (C1, v, 2)");
                return 0;
            }
            enableSigSet();

            makeMove();

            if (semop(semId, &v_ops[3], 1) < 0)
            {
                perror("Error in Semaphore Operation (C1, v3, 1)");
                return 0;
            }
            enableSigSet();
        }

        return 0;
    }
    else if (memPointer->Client2 == -12)
    {
        memPointer->Client2 = getpid();
        receiver.Type = 2;
        name2 = (char *)malloc(strlen(argv[1]) * sizeof(char));
        name2 = argv[1];
        strcpy(name2, argv[1]);

        {
            kill(memPointer->Server, SIGUSR1);
            if (semop(semId, &v_ops[0], 1) == -1)
            {
                perror("Error in Semaphore Operation (C2, v, 1)");
            }
            enableSigSet();
            pause();
        }
        while (1)
        {
            disableSigSet();
            if (semop(semId, &p_ops[2], 1) < 0)
            {
                perror("Error in Semaphore Operation (C2, p2, 1)");
                return 0;
            }

            // mossa
            memPointer->current = getpid();

            if (semop(semId, &p_ops[0], 1) < 0)
            {
                perror("Error in Semaphore Operation (C2, p, 2)");
                return 0;
            }

            showMatrix();

            if (semop(semId, &v_ops[0], 1) < 0)
            {
                perror("Error in Semaphore Operation (C2, v, 2)");
                return 0;
            }
            enableSigSet();

            makeMove();

            if (semop(semId, &v_ops[3], 1) < 0)
            {
                perror("Error in Semaphore Operation (C2, v3, 1)");
                return 0;
            }
        }

        return 0;
    }
    else
    {
        print("Partita occupata.\n");
        if (&memPointer != 0)
        {
            if (shmdt(memPointer) < 0)
            {
                perror("Shared Memory Detaching Error");
                return 0;
            }
        }
    }

    return 0;
}