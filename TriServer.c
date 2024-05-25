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

// Variabili globali e funzioni ------------------------------------------------------//
int delay = -1, timeOut;
int semId = -2, shmId = -2, msgId = -2;
char symbol1, symbol2, symbols[6];
sigset_t disabledSigSet;
struct sembuf p_ops[1];
struct sembuf v_ops[1];

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
my message;
size_t msgSize;

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

void closure()
{
    // Rimozione semafori e memoria condivisa -----------------------------------------//

    if (semId != -2)
    {
        if (semctl(semId, 0, IPC_RMID) < 0) // #TODO
        {
            perror("Semaphores Elimination Error");
        }
    }
    if (&memPointer != 0)
    {
        if (shmdt(memPointer) < 0)
        {
            perror("Shared Memory Detachment Error");
        }
    }
    if (msgId != -2)
    {
        if (msgctl(msgId, IPC_RMID, NULL) < 0)
        {
            perror("Message Queue Elimination Error");
        }
    }
    if (shmId != -2)
    {

        if (shmctl(shmId, IPC_RMID, NULL) < 0)
        {
            perror("Shared Memory Cancellation Error");
        }
    }

    printf("\nEseguita chiusura SM, MQ e Sem\n");
    fflush(stdout);
    return;
}

void disableSetandLock(int n)
{

    sigfillset(&disabledSigSet);
    sigprocmask(SIG_SETMASK, &disabledSigSet, NULL);

    if (semop(semId, &p_ops[n], 1) == -1)
    {
        perror("Error in Semaphore Operation (S, p)");
        closure();
    }
}

void enableSetandUnlock(int n)
{
    if (semop(semId, &v_ops[n], 1) == -1)
    {
        perror("Error in Semaphore Operation (S, v)");
        closure();
    }

    sigdelset(&disabledSigSet, SIGINT);
    sigdelset(&disabledSigSet, SIGUSR1);
    sigdelset(&disabledSigSet, SIGUSR2);
    sigdelset(&disabledSigSet, SIGALRM);
    sigdelset(&disabledSigSet, SIGTERM);
    sigprocmask(SIG_SETMASK, &disabledSigSet, NULL);
}

void sendMessage(char *msg)
{
    memcpy(message.Text, msg, strlen(msg) + 1);
    message.Type = 1;
    msgSize = sizeof(message) - sizeof(long);
    if (msgsnd(msgId, &message, msgSize, IPC_NOWAIT) < 0)
    {
        if (errno == EAGAIN)
        {
            perror("Queue full");
        }
        else
        {
            perror("Messange sending Error");
        }
        return;
    }
    kill(memPointer->Client1, SIGUSR2);
    message.Type = 2;
    msgSize = sizeof(message) - sizeof(long);
    if (msgsnd(msgId, &message, msgSize, IPC_NOWAIT) < 0)
    {
        if (errno == EAGAIN)
        {
            perror("Queue full");
        }
        else
        {
            perror("Messange sending Error");
        }
        return;
    }

    disableSetandLock(0);
    kill(memPointer->Client2, SIGUSR2);
    enableSetandUnlock(0);
}

void sigHandler(int signal) // #TODO
{

    if (signal == SIGINT)
    {
        if (delay == 2)
        {
            printf("Pressione del Ctrl-C eseguita.\nUn'altra pressione consecutiva del tasto comporterà la terminazione del gioco.\n");
            fflush(stdout);
            delay--;
        }
        else
        {
            delay--;
            disableSetandLock(0);
            memPointer->onGame = 0;
            enableSetandUnlock(0);
        }
    }
    else
    {
        if (delay == 1) // #TODO migliora
        {
            delay = 2;
        }
        if (signal == SIGALRM)
        {
            disableSetandLock(0);
            // kill(memPointer->current, SIGALRM); messaggio di fine tempo
            if (memPointer->current == memPointer->Client1)
            {

                memPointer->current = memPointer->Client2;
            }
            else
            {
                memPointer->current = memPointer->Client1;
            }
            enableSetandUnlock(0);
            alarm(timeOut);
        }
        else if (signal == SIGUSR1) // il client 1 comunica l'arrivo e l'uscita + fatte delle mosse
        {
            disableSetandLock(0);
            if (memPointer->onGame == 0)
            {
                memPointer->Client1 = -11;
            }
            else if (memPointer->current == -1)
            {
                printf("Giocatore Individuato.\n");
                fflush(stdout);
            }
            else // è una mossa
            {
                alarm(0);
                if (memPointer->table[memPointer->move / 3][memPointer->move % 3] != ' ')
                {
                    // penitenza
                }
                else
                {
                    // compilo la matrice
                    if (memPointer->current == memPointer->Client1)
                    {
                        memPointer->table[memPointer->move / 3][memPointer->move % 3] = symbol1;
                    }
                    else
                    {
                        memPointer->table[memPointer->move / 3][memPointer->move % 3] = symbol2;
                    }
                    printf("%d, %d \n", memPointer->move / 3, memPointer->move % 3);

                    // controllo potenziale vittoria/fine partita
                    for (int i = 0; i < 3; i++)
                    {
                        if ((memPointer->table[i][0] == memPointer->table[i][1] && memPointer->table[i][1] == memPointer->table[i][2] && (!memPointer->table[i][1] == ' ')) || (memPointer->table[0][i] == memPointer->table[1][i] && memPointer->table[1][i] == memPointer->table[2][i] && !(memPointer->table[1][i] == ' ')))
                        {

                            printf("1 partita conclusa");
                            fflush(stdout);
                            memPointer->onGame = 0;
                        }
                    }
                    if ((memPointer->table[0][0] == memPointer->table[1][1] && memPointer->table[2][2] == memPointer->table[1][1] && memPointer->table[1][1] != ' ') || memPointer->table[0][2] == memPointer->table[1][1] && memPointer->table[2][0] == memPointer->table[1][1] && !(memPointer->table[1][1] == ' '))
                    {
                        printf("2 partita conclusa");
                        fflush(stdout);
                        memPointer->onGame = 0;
                    }

                    printf("\nTocca a %d\n", memPointer->current);
                    if (memPointer->onGame == 1)
                    {

                        alarm(timeOut);
                        sendMessage("Tocca a Te!");
                        kill(memPointer->current, SIGUSR2);
                    }
                }
            }
            enableSetandUnlock(0);
        }
        else if (signal == SIGUSR2) // il client 2 comunica l'arrivo e l'uscita + gestione inizio partita
        {
            disableSetandLock(0);
            if (memPointer->onGame == 0)
            {
                memPointer->Client2 = -12;
            }
            else if (memPointer->current == -1)
            {
                printf("Entrambi i giocatori Individuati.\n");
                fflush(stdout);
                memPointer->current = memPointer->Client1;
                memPointer->move = timeOut; // #TODO togli
                // #TODO il Server deve informare dei simboli per i giocatori

                sendMessage("Entrambi i giocatori sono stati individuati. Partita avviata.\n");
            }

            enableSetandUnlock(0);
        }
        else if (signal == SIGTERM)
        {
            printf("quit ricevuto");
            fflush(stdout);
            disableSetandLock(0);
            if (memPointer->move == memPointer->Client1)
            {
                sendMessage("Il giocatore 1 ha abbandonato, vince il giocatore 2.\n");
            }
            else if (memPointer->move == memPointer->Client2)
            {
                sendMessage("Il giocatore 2 ha abbandonato, vince il giocatore 1.\n");
            }
            else
            {
                perror("Abandoning procedure went wrong");
                closure();
            }
            delay = 0;
            enableSetandUnlock(0);
        }
        else
        {
            perror("Unpredicted Signal Error");
        }
    }
    return;
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
    //---------------------------------------------------------------------------//

    // Predisposizione memoria condivisa, code di messaggi, semafori e segnali ---------------------------------------//
    size_t size = sizeof(shMem);

    shmId = shmget(ftok("./TriServer.c", 's'), size, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR); // #TODO gestisci errore con errno EEXIST
    if (errno == EEXIST)
    {
        perror("Shared Memory existance error");
        printf("Deleting...\n");
        shmId = shmget(ftok("./TriServer.c", 's'), size, S_IRUSR | S_IWUSR);
        if (shmctl(shmId, IPC_RMID, NULL) < 0)
        {
            perror("Shared Memory Cancellation Error");
        }
    }
    else if (shmId < 0)
    {
        perror("Shared Memory creation error");
        closure();
    }

    memPointer = shmat(shmId, NULL, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR); // #TODO qui e nel client ho sbagliato forse coi permessi
    if (memPointer == (void *)-1)
    {
        perror("Shared Memory Attachment Error");
        closure();
    }

    msgId = msgget(ftok("./TriServer.c", 's'), IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
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
    else if (msgId < 0)
    {
        perror("Message Queue creation Error");
        closure();
    }

    semId = semget(ftok("./TriServer.c", 's'), 3, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    if (errno == EEXIST)
    {
        perror("Semaphore Already Existing");
        printf("Deleting...\n");
        fflush(stdout);
        semId = semget(ftok("./TriServer.c", 's'), 3, S_IRUSR | S_IWUSR);
        if (semctl(semId, 0, IPC_RMID) < 0)
        {
            perror("Semaphores Elimination Error");
        }
        return -1;
    }
    else if (semId < 0)
    {
        perror("semaphore's ID creation Error");
        closure();
    }
    struct semid_ds dataStruct;
    mySU semArgs;
    semArgs.dataStruct = &dataStruct;
    unsigned short values[] = {1, 1, 0};
    semArgs.array = values;

    for (int i = 0; i < 3; i++)
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
        perror("Server Semaphore value association Error");
        closure();
    }

    sigfillset(&disabledSigSet);
    sigdelset(&disabledSigSet, SIGINT);
    sigdelset(&disabledSigSet, SIGUSR1);
    sigdelset(&disabledSigSet, SIGUSR2);
    sigdelset(&disabledSigSet, SIGALRM);
    sigdelset(&disabledSigSet, SIGTERM); // uso per informare che è avvenuta la pressione del doppio control c

    sigprocmask(SIG_SETMASK, &disabledSigSet, NULL);
    signal(SIGINT, sigHandler);
    signal(SIGUSR1, sigHandler);
    signal(SIGUSR2, sigHandler);
    signal(SIGALRM, sigHandler);
    signal(SIGTERM, sigHandler);

    //----------------------------------------------------------------------------------------//

    // corpo del codice ------------------------------------------------------------------#TODO
    printf("Process Pid: %d\n", getpid());
    fflush(stdout);

    disableSetandLock(0);
    if (memPointer->onGame == 1)
    {
        printf("Una partita è già in corso\n");
        enableSetandUnlock(0);
        closure();
    }
    memPointer->Server = getpid();
    memPointer->Client1 = -11;
    memPointer->Client2 = -12;
    memPointer->onGame = 1;
    memPointer->current = -1;
    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            memPointer->table[i][j] = ' ';
        }
    }

    //....
    // timer del timeout
    // gestisci pareggi

    delay = 2;
    do
    {
        enableSetandUnlock(0);
        printf("entro");
        fflush(stdout);
        pause();
        printf("esco");
        fflush(stdout);
        if (semctl(semId, 0 /*ignored*/, GETALL, semArgs) == -1)
        {
            perror("get error");
            closure();
        }
        disableSetandLock(0);
    } while ((memPointer->Client2 == -12) && delay != 0);
    // controlla solo il secondo client: dovesse essere -10: giocatore 2 automatico;
    // altrimenti, se il valore -12 è stato sistituito, si è trovato il giocatore 2
    enableSetandUnlock(0);

    while (delay != 0)
    {
        pause();

        // system('clear');
    }

    disableSetandLock(0);
    memPointer->onGame = 0;
    kill(memPointer->Client1, SIGTERM);
    kill(memPointer->Client2, SIGTERM);
    while ((memPointer->Client1 != -11 || memPointer->Client2 != -12) && delay != 0)
    {
        enableSetandUnlock(0);
        sleep(1); // #TODO migliora
        disableSetandLock(0);
        printf("%d e %d", memPointer->Client1, memPointer->Client2);
    }
    enableSetandUnlock(0);
    closure();

    return 0;
}