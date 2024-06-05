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
// Variabili Globali, Strutture e Funzioni -----------------------------------------------------------//

int timeOut;           // Variabile per il tempo della mossa
char symbol1, symbol2; // simboli delle mosse
int shmId = -2, msgId = -2, semId = -2;
sigset_t disabledSigSet;
struct sembuf p_ops[semNum];
struct sembuf v_ops[semNum];

typedef struct sharedMemoryStruct
{
    char table[3][3];
    pid_t Server;
    pid_t Client1;
    pid_t Client2;
    int onGame; // 1 = in partita, 0 = fuori partita
    pid_t current;
    int move;

} shMem;
shMem *memPointer;

size_t size = sizeof(shMem);

typedef union semUnion
{
    int val;
    struct semid_ds *dataStruct;
    unsigned short *array;

} mySU;

typedef struct myMsg
{
    long Type;
    char Text[512];
} my;
my message;
size_t msgSize;

//->Funzioni
void print(char *txt)
{
    if (write(1, txt, strlen(txt)) < 0)
    {
        perror("Error in writing messange");
    }
}

void enableSigSet()
{
    sigdelset(&disabledSigSet, SIGUSR1);
    sigdelset(&disabledSigSet, SIGUSR2);
    sigdelset(&disabledSigSet, SIGALRM);
    sigdelset(&disabledSigSet, SIGTERM);
    sigprocmask(SIG_SETMASK, &disabledSigSet, NULL);
}

void closure()
{
    kill(memPointer->Client1, SIGTERM);
    kill(memPointer->Client2, SIGTERM);
    while (memPointer->Client1 != -11 && memPointer->Client2 != -12) // #TODO puoi forse implementare il timer che s enon risponde lo killi
    {
        if (semop(semId, &v_ops[0], 1) == -1)
        {
            perror("Error in Semaphore Operation (S, v, 4)");
            closure();
        }
        enableSigSet();

        pause(); // se si blocca, controlla di non doverlo mettere a sleep

        sigfillset(&disabledSigSet);
        sigdelset(&disabledSigSet, SIGINT);
        sigprocmask(SIG_SETMASK, &disabledSigSet, NULL);
        if (semop(semId, &p_ops[0], 1) == -1)
        {
            perror("Error in Semaphore Operation (S, p, 4)");
            closure();
        }
    }
    if (semop(semId, &v_ops[0], 1) == -1)
    {
        perror("Error in Semaphore Operation (S, v, 5)");
        closure();
    }

    if (semId != -2)
    {
        if (semctl(semId, 0, IPC_RMID) < 0) // #TODO
        {
            perror("Semaphores Elimination Error");
        }
    }
    else
    {
    }
    if (msgId != -2)
    {
        if (msgctl(msgId, IPC_RMID, NULL) < 0)
        {
            perror("Message Queue Elimination Error");
        }
    }

    if (&memPointer != NULL)
    {
        if (shmdt(memPointer) < 0)
        {
            perror("Shared Memory Detachment Error");
        }
    }
    if (shmId != -2)
    {

        if (shmctl(shmId, IPC_RMID, NULL) < 0)
        {
            perror("Shared Memory Cancellation Error");
        }
    }

    print("Chiusure eseguite\n");
}

void sigHandler(int signal)
{
    sigfillset(&disabledSigSet);
    sigdelset(&disabledSigSet, SIGINT);
    sigprocmask(SIG_SETMASK, &disabledSigSet, NULL);
    if (semop(semId, &p_ops[0], 1) == -1)
    {
        perror("Error in Semaphore Operation (S, p, 0)");
        closure();
    }

    if (signal == SIGINT)
    {
        memPointer->onGame--;
        if (memPointer->Client1 != -11 && memPointer->Client2 != -12)
        {
            if (semop(semId, &p_ops[3], 1) == -1)
            {
                perror("Error in Semaphore Operation (S, p3, 0)");
                closure();
            }
        }
    }
    else
    {
        memPointer->onGame = 3;
        if (signal == SIGALRM)
        {
        }
    }

    if (semop(semId, &v_ops[0], 1) == -1)
    {
        perror("Error in Semaphore Operation (S, v, 0)");
        closure();
    }
}

int main(int argc, char *argv[])
{
    // Controllo degli argomenti -----------------------------------------------------------------//
    if (argc != 4)
    {
        printf("Inserimento scorretto dei parametri di gioco.\nL'arbitro deve ricevere (spaziati): tempo di time-out, simbolo per il giocatore 1 e simbolo per il giocatore 2.\nRiprovare\n");
        return 0;
    }
    for (int i = 0; i < strlen(argv[1]); i++)
    {
        if (argv[1][i] < '0' || argv[1][i] > '9')
        {
            printf("Valore del Time-out passato non valido. Il valore deve essere rigorosamente numerico.\n");
            fflush(stdout);
            return 0;
        }
    }

    timeOut = atoi(argv[1]); // #TODO casistica senza timeout

    if (strlen(argv[2]) != 1 || strlen(argv[3]) != 1)
    {
        printf("Uno o più simboli sono stati inseriti in maniera errata. Ogni giocatore può disporre di un simbolo composto da 1 solo carattere.\n");
        fflush(stdout);
        return 0;
    }
    symbol1 = argv[2][0];
    symbol2 = argv[3][0];
    //--------------------------------------------------------------------------------//

    // Predisposizione Memoria Condivisa, code di Messaggi, semafori e segnali -----------//  #TODO vedi se migliorare le chiusure
    {
        semId = semget(ftok("./TriServer.c", 's'), semNum, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
        if (semId < 0)
        {
            if (errno == EEXIST)
            {
                print("Semaphore Already Existing. Deleting...\n");
                semId = semget(ftok("./TriServer.c", 's'), semNum, S_IRUSR | S_IWUSR);
                if (semctl(semId, 0, IPC_RMID) < 0)
                {
                    perror("Semaphores Elimination Error");
                }
            }
            else
            {
                perror("Semaphore's ID creation Error");
            }
            closure();
        }

        struct semid_ds dataStruct;
        mySU semArgs;
        semArgs.dataStruct = &dataStruct;
        unsigned short values[] = {1, 1, 0, 0};
        semArgs.array = values;

        for (int i = 0; i < semNum; i++)
        {
            p_ops[i].sem_num = i;
            p_ops[i].sem_op = -1;
            p_ops[i].sem_flg = 0;

            v_ops[i].sem_num = i;
            v_ops[i].sem_op = 1;
            v_ops[i].sem_flg = 0;
        }

        if (semctl(semId, 0, SETALL, semArgs) < 0)
        {
            perror("Server Semaphore Value Association Error");
            closure();
        }

        shmId = shmget(ftok("./TriServer.c", 's'), size, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
        if (shmId < 0)
        {
            if (errno == EEXIST)
            {
                // #TODO qui devi gestire il semaforo in modo che appena creato, controlli se c'è una partita in corso, se c'è
                // esci, se c'è memoria condivisa ma la partita non è in corso, la togli
                perror("Shared Memory existance error");
                printf("Deleting...\n");
                shmId = shmget(ftok("./TriServer.c", 's'), size, S_IRUSR | S_IWUSR);
                if (shmctl(shmId, IPC_RMID, NULL) < 0)
                {
                    perror("Shared Memory Cancellation Error");
                }
            }
            perror("Shared Memory Id Creation error");
        }

        memPointer = shmat(shmId, NULL, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR); // #TODO qui e nel client ho sbagliato forse coi permessi
        if (memPointer == (void *)-1)
        {
            perror("Shared Memory Attachment Error");
            closure();
            return 0;
        }

        msgId = msgget(ftok("./TriServer.c", 's'), IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);

        if (msgId < 0)
        {
            if (errno == EEXIST)
            {
                perror("Message Queue Already present");
                printf("Deleting...\n");
                fflush(stdout);
                msgId = msgget(ftok("./TriServer.c", 's'), S_IRUSR | S_IWUSR);
                if (msgctl(msgId, IPC_RMID, NULL) < 0)
                {
                    perror("Message Queue Elimination Error");
                }
            }
            else
            {
                perror("Message Queue creation Error");
            }
            closure();
            return 0;
        }

        sigfillset(&disabledSigSet);
        sigdelset(&disabledSigSet, SIGINT);
        enableSigSet();
        signal(SIGINT, sigHandler);
        signal(SIGUSR1, sigHandler);
        signal(SIGUSR2, sigHandler);
        signal(SIGALRM, sigHandler);
        signal(SIGTERM, sigHandler);
    }
    //----------------------------------------------------------------------------------------------//

    // corpo del codice ------------------------------------------------------------------//
    printf("Process Pid: %d\n", getpid());
    fflush(stdout);

    sigfillset(&disabledSigSet);
    sigdelset(&disabledSigSet, SIGINT);
    sigprocmask(SIG_SETMASK, &disabledSigSet, NULL);
    if (semop(semId, &p_ops[0], 1) == -1)
    {
        perror("Error in Semaphore Operation (S, p, 1)");
        closure();
    }

    if (memPointer->onGame > 0) // una partita è già in corso
    {
        print("Una partita è già in corso.\n");
        if (semop(semId, &v_ops[0], 1) < 0)
        {
            perror("Error in Semaphore Operation (S, v, 1)");
            closure();
        }
        enableSigSet();
        closure();
        return 0;
    }
    memPointer->Server = getpid();
    memPointer->Client1 = -11;
    memPointer->Client2 = -12;
    memPointer->onGame = 3; // il valore è a 3 per consentire di diminuire 2 volte causa ctrl c
    memPointer->current = -1;

    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            memPointer->table[i][j] = ' ';
        }
    }

    /*
        l'idea è che adesso sia i server che i client hanno come punto di riferimento la variabile ongame: così da evitare
        segmentation fault (forse) la funzione closure utilizza questa variabile per accertarsi che prima si stacchi il client
        1, poi che si stacchi il client 2, infine esce da solo
    */

    do
    {
        if (semop(semId, &v_ops[0], 1) == -1)
        {
            perror("Error in Semaphore Operation (S, v, 2)");
            closure();
        }
        enableSigSet();

        pause();

        sigfillset(&disabledSigSet);
        sigdelset(&disabledSigSet, SIGINT);
        sigprocmask(SIG_SETMASK, &disabledSigSet, NULL);
        if (semop(semId, &p_ops[0], 1) == -1)
        {
            perror("Error in Semaphore Operation (S, v, 2)");
            closure();
        }

    } while ((memPointer->Client2 == -12 || memPointer->Client1 == -11) && memPointer->onGame > 0);

    while (memPointer->onGame > 1)
    {
        if (semop(semId, &v_ops[0], 1) == -1)
        {
            perror("Error in Semaphore Operation (S, v, 3)");
            closure();
        }
        enableSigSet();

        // partita

        sigfillset(&disabledSigSet);
        sigdelset(&disabledSigSet, SIGINT);
        sigprocmask(SIG_SETMASK, &disabledSigSet, NULL);
        if (semop(semId, &p_ops[0], 1) == -1)
        {
            perror("Error in Semaphore Operation (S, p, 3)");
            closure();
        }
    }

    closure();

    return 0;
}