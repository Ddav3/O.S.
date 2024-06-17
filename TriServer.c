/************************************
 *VR485815
 *Davide Sala
 *Data di realizzazione: 16/06/2024
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
pid_t bPid = 0;

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

int quit = 0;
void closure()
{
    disableSigSet();
    if (semop(semId, &p_ops[0], 1) == -1)
    {
        perror("Error in Semaphore Operation (S, p, 50)");
        return;
    }
    memPointer->current = -1;
    memPointer->onGame = 1;

    if (!(quit > 0))
    {

        if (memPointer->Client1 != -11)
        {
            kill(memPointer->Client1, SIGINT);
        }
        if (memPointer->Client2 != -12)
        {
            kill(memPointer->Client2, SIGINT);
        }

        while ((memPointer->Client1 != -11 || memPointer->Client2 != -12))
        {
            if (semop(semId, &v_ops[0], 1) == -1)
            {
                perror("Error in Semaphore Operation (S, v, 50)");
                return;
            }
            enableSigSet();

            sleep(1);

            disableSigSet();
            if (semop(semId, &p_ops[0], 1) == -1)
            {
                perror("Error in Semaphore Operation (S, p, 50)");
                return;
            }
        }
    }

    if (semop(semId, &v_ops[0], 1) == -1)
    {
        perror("Error in Semaphore Operation (S, v, 51)");
        return;
    }

    if (shmId != -2)
    {
        if (shmctl(shmId, IPC_RMID, NULL) < 0)
        {
            perror("Shared Memory Cancellation Error");
        }
        shmId = -2;
    }
    if (&memPointer != NULL)
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
    if (semId != -2)
    {
        if (semctl(semId, 0, IPC_RMID) < 0)
        {
            perror("Semaphores Elimination Error");
        }
        semId = -2;
    }

    exit(0);
}
void sendMessage(char *msg, int who1, int who2)
{
    memcpy(message.Text, msg, strlen(msg) + 1);
    if (who1 == 1)
    {
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
    }

    if (who2 == 1 && bPid == 0)
    {
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
        kill(memPointer->Client2, SIGUSR2);
    }

    if (memset(message.Text, 0, msgSize) < 0)
    {
        perror("Resetting Message queue Error");
        closure();
        return;
    }
}

int draw = 0;
void winCondition()
{
    for (int i = 0; i < 3; i++)
    {
        if (((memPointer->table[i][0] == memPointer->table[i][1] && memPointer->table[i][1] == memPointer->table[i][2])) && memPointer->table[i][1] != ' ')
        {
            if (memPointer->table[i][1] == symbol1)
            {
                sendMessage("Vince il giocatore 1 per riga! Congratulazioni!\n", 1, 1);
                kill(memPointer->Client1, SIGUSR1);
                kill(memPointer->Client2, SIGUSR1);
            }
            else if (memPointer->table[i][1] == symbol2)
            {
                if (bPid == 0)
                {
                    sendMessage("Vince il giocatore 2 per riga! Congratulazioni!\n", 1, 1);
                    kill(memPointer->Client2, SIGUSR1);
                    kill(memPointer->Client1, SIGUSR1);
                }
                else
                {
                    sendMessage("Vince il bot!\n", 1, 0);
                    kill(memPointer->Client1, SIGUSR1);
                }
            }
            if (semop(semId, &v_ops[1], 1) < 0)
            {
                perror("Error in Semaphore Operation (S, v1, 20)");
                return;
            }
            if (semop(semId, &v_ops[2], 1) < 0)
            {
                perror("Error in Semaphore Operation (S, v2, 20)");
                return;
            }
            closure();
        }
        else if ((memPointer->table[0][i] == memPointer->table[1][i] && memPointer->table[1][i] == memPointer->table[2][i]) && memPointer->table[1][i] != ' ')
        {
            if (memPointer->table[1][i] == symbol1)
            {
                sendMessage("Vince il giocatore 1 per colonna! Congratulazioni!\n", 1, 1);
                kill(memPointer->Client2, SIGUSR1);
                kill(memPointer->Client1, SIGUSR1);
            }
            else if (memPointer->table[1][i] == symbol2)
            {
                if (bPid == 0)
                {
                    sendMessage("Vince il giocatore 2 per colonna! Congratulazioni!\n", 1, 1);
                    kill(memPointer->Client2, SIGUSR1);
                    kill(memPointer->Client1, SIGUSR1);
                }
                else
                {
                    sendMessage("Vince il bot!\n", 1, 0);
                    kill(memPointer->Client1, SIGUSR1);
                }
            }
            if (semop(semId, &v_ops[1], 1) < 0)
            {
                perror("Error in Semaphore Operation (S, v1, 21)");
                return;
            }
            if (semop(semId, &v_ops[2], 1) < 0)
            {
                perror("Error in Semaphore Operation (S, v2, 21)");
                return;
            }
            closure();
        }
    }

    if ((memPointer->table[1][1] != ' ') && ((memPointer->table[0][0] == memPointer->table[1][1] && memPointer->table[2][2] == memPointer->table[1][1]) || (memPointer->table[2][0] == memPointer->table[1][1] && memPointer->table[0][2] == memPointer->table[1][1])))
    {
        if (memPointer->table[1][1] == symbol1)
        {
            sendMessage("Vince il giocatore 1 per diagonale! Congratulazioni!\n", 1, 1);
            kill(memPointer->Client1, SIGUSR1);
            kill(memPointer->Client2, SIGUSR1);
        }
        else if (memPointer->table[1][1] == symbol2)
        {
            if (bPid == 0)
            {
                sendMessage("Vince il giocatore 2 per diagonale! Congratulazioni!\n", 1, 1);
                kill(memPointer->Client1, SIGUSR1);
                kill(memPointer->Client2, SIGUSR1);
            }
            else
            {
                sendMessage("Vince il bot!\n", 1, 0);
                kill(memPointer->Client1, SIGUSR1);
            }
        }
        if (semop(semId, &v_ops[1], 1) < 0)
        {
            perror("Error in Semaphore Operation (S, v1, 22)");
            return;
        }
        if (semop(semId, &v_ops[2], 1) < 0)
        {
            perror("Error in Semaphore Operation (S, v2, 22)");
            return;
        }
        closure();
    }

    draw++;
    if (draw == 9)
    {
        if (memPointer->current != memPointer->Client1)
        {
            kill(memPointer->Client1, SIGUSR1);
        }
        else
        {
            kill(memPointer->Client2, SIGUSR1);
        }
        sendMessage("Pareggio!\n", 1, 1);

        if (semop(semId, &v_ops[1], 1) < 0)
        {
            perror("Error in Semaphore Operation (S, v1, 23)");
            return;
        }
        if (semop(semId, &v_ops[2], 1) < 0)
        {
            perror("Error in Semaphore Operation (S, v2, 23)");
            return;
        }
        closure();
    }
}
void compileMatrix(char symbol, int who)
{
    memPointer->table[memPointer->move / 3][memPointer->move % 3] = symbol;
    if (who == 0)
    {
        kill(memPointer->Client1, SIGUSR1);
    }
    else
    {
        if (bPid == 0)
        {

            kill(memPointer->Client2, SIGUSR1);
        }
    }
    winCondition();
}
void sigHandler(int signal)
{
    disableSigSet();
    if (semop(semId, &p_ops[0], 1) == -1)
    {
        perror("Error in Semaphore Operation (S, p, 0)");
        return;
    }

    if (signal == SIGINT)
    {
        if (memPointer->current == -10)
        {
            if (memPointer->Client2 != bPid)
            {
                sendMessage("\nIl giocatore 1 si è arreso. Hai vinto tu, giocatore 2!\n", 0, 1);
            }
            if (semop(semId, &v_ops[0], 1) == -1)
            {
                perror("Error in Semaphore Operation (Sc, v, 1)");
                return;
            }
            closure();
        }
        else if (memPointer->current == -20)
        {
            sendMessage("\nIl giocatore 2 si è arreso. Hai vinto tu, giocatore 1!\n", 1, 0);
            if (semop(semId, &v_ops[0], 1) == -1)
            {
                perror("Error in Semaphore Operation (Sc, v, 2)");
                return;
            }
            closure();
        }

        memPointer->onGame--;

        if (memPointer->onGame == 1)
        {
            memPointer->current = -1;
            if (semop(semId, &v_ops[0], 1) < 0)
            {
                perror("Error in Semaphore Operation (S, v, 51)");
                return;
            }
            enableSigSet();

            closure();
        }

        if (memPointer->Client1 == memPointer->current || memPointer->current == memPointer->Client2)
        {

            if (semop(semId, &v_ops[0], 1) < 0)
            {
                perror("Error in Semaphore Operation (S, vsig)");
                return;
            }
            if (semop(semId, &p_ops[3], 1) == -1)
            {
                if (errno != EINTR)
                {
                    perror("Error in Semaphore Operation (S, psig)");
                    return;
                }
            }
        }
    }
    else
    {
        memPointer->onGame = 3;
        if (signal == SIGUSR1)
        {
            if (memPointer->Client2 == -10)
            {
                print("Inizio partita contro il computer.\n");
            }
            else if (memPointer->Client2 != -12)
            {
                print("Giocatore 2 individuato.\n");
            }
            else if (memPointer->Client1 != -11)
            {
                print("Giocatore 1 individuato.\n");
            }
        }
        else if (signal == SIGUSR2)
        {
        }
        else if (SIGALRM)
        {

            print("alarm\n");

            if (memPointer->current == memPointer->Client1)
            {
                if (semop(semId, &v_ops[2], 1) < 0)
                {
                    perror("Error in Semaphore Operation (S, prevalrm2)");
                    return;
                }
                sendMessage("\nTempo scaduto. Hai perso.\n", 1, 0);
                kill(memPointer->Client1, SIGALRM);
                if (memPointer->Client2 != bPid)
                {
                    sendMessage("Vittoria per Time Out.\n", 0, 1);
                    kill(memPointer->Client2, SIGALRM);
                }

                if (semop(semId, &v_ops[0], 1) < 0)
                {
                    perror("Error in Semaphore Operation (S, prevalrm01)");
                    return;
                }
                enableSigSet();

                closure();
            }
            else if (memPointer->current == memPointer->Client2)
            {
                if (semop(semId, &v_ops[1], 1) < 0)
                {
                    perror("Error in Semaphore Operation (S, prevalrm1)");
                    return;
                }
                if (memPointer->Client2 != bPid)
                {
                    sendMessage("\nTempo scaduto. Hai perso.\n", 0, 1);
                    kill(memPointer->Client2, SIGALRM);
                }
                sendMessage("Vittoria per Time Out.\n", 1, 0);
                kill(memPointer->Client1, SIGALRM);
                if (semop(semId, &v_ops[0], 1) < 0)
                {
                    perror("Error in Semaphore Operation (S, prevalrm02)");
                    return;
                }
                enableSigSet();

                closure();
            }
        }
    }

    if (semop(semId, &v_ops[0], 1) == -1)
    {
        perror("Error in Semaphore Operation (S, v, 0)");
        closure();
    }
    enableSigSet();
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

    timeOut = atoi(argv[1]);

    if (strlen(argv[2]) != 1 || strlen(argv[3]) != 1)
    {
        printf("Uno o più simboli sono stati inseriti in maniera errata. Ogni giocatore può disporre di un simbolo composto da 1 solo carattere.\n");
        fflush(stdout);
        return 0;
    }
    symbol1 = argv[2][0];
    symbol2 = argv[3][0];
    //--------------------------------------------------------------------------------//

    // Predisposizione Memoria Condivisa, code di Messaggi, semafori e segnali -----------//
    shmId = shmget(ftok("./TriServer.c", 's'), size, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    if (shmId < 0)
    {
        if (errno == EEXIST)
        {
            perror("Shared Memory Already Existing");
            shmId = shmget(ftok("./TriServer.c", 's'), size, S_IRUSR | S_IWUSR);
            quit++;
        }
        else
        {

            perror("Shared Memory creation error");
        }
    }

    memPointer = shmat(shmId, NULL, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    if (memPointer == (void *)-1)
    {
        perror("Shared Memory Attachment Error");
        if (memPointer->onGame == 1)
        {
            print("Partita in corso.\n");
        }
        quit++;
    }

    msgId = msgget(ftok("./TriServer.c", 's'), IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    if (msgId < 0)
    {
        if (errno == EEXIST)
        {
            perror("Message Queue Already present");
            msgId = msgget(ftok("./TriServer.c", 's'), S_IRUSR | S_IWUSR);

            quit++;
        }
        else
        {
            perror("Message Queue creation Error");
        }
    }

    semId = semget(ftok("./TriServer.c", 's'), semNum, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    if (semId < 0)
    {
        if (errno == EEXIST)
        {
            perror("Semaphore Already Existing");
            semId = semget(ftok("./TriServer.c", 's'), semNum, S_IRUSR | S_IWUSR);
            quit++;
        }
        else
        {
            perror("semaphore's ID creation Error");
        }
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
        perror("Server Semaphore value association Error");
    }

    if (quit > 1)
    {
        closure();
        return 0;
    }

    sigfillset(&disabledSigSet);
    sigdelset(&disabledSigSet, SIGINT);
    sigdelset(&disabledSigSet, SIGALRM);
    enableSigSet();
    signal(SIGINT, sigHandler);
    signal(SIGALRM, sigHandler);
    signal(SIGUSR1, sigHandler);
    signal(SIGUSR2, sigHandler);
    //----------------------------------------------------------------------------------------------//

    // corpo del codice ------------------------------------------------------------------//
    disableSigSet();
    if (semop(semId, &p_ops[0], 1) == -1)
    {
        perror("Error in Semaphore Operation (S, p, 1)");
        return 0;
    }

    if (memPointer->onGame >= 1) // una partita è già in corso
    {
        print("Una partita è già in corso.\n");
        if (semop(semId, &v_ops[0], 1) < 0)
        {
            perror("Error in Semaphore Operation (S, v, 1)");
            return 0;
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

    do
    {
        if (memPointer->Client1 == -11)
        {
            print("Ricerca primo giocatore in corso...\n");
        }
        if (memPointer->Client2 == -12)
        {
            print("Ricerca secondo giocatore in corso...\n");
        }
        if (semop(semId, &v_ops[0], 1) < 0)
        {
            perror("Error in Semaphore Operation (S, v, 2)");
            return 0;
        }
        enableSigSet();

        pause();

        disableSigSet();
        if (semop(semId, &p_ops[0], 1) < 0)
        {
            perror("Error in Semaphore Operation (S, p, 2)");
            return 0;
        }
    } while ((memPointer->Client2 == -12 || memPointer->Client1 == -11));

    print("Partita avviata.\n");

    if (memPointer->Client1 != -11 && memPointer->Client2 == -10)
    {
        memPointer->current = memPointer->Client1;
        sendMessage("Inizio partita contro la CPU.\nRegole: ogni giocatore deve scegliere la propria mossa inserendo\nun numero da 1 a 9 in base alla cella che vuole occupare.\nI numeri delle celle sono organizzati come segue:\n\n 1 | 2 | 3\n___|___|___\n 4 | 5 | 6\n___|___|___\n 7 | 8 | 9\n   |   |   \n", 1, 0);
        if (semop(semId, &v_ops[0], 1) == -1)
        {
            perror("Error in Semaphore Operation (S, vbot, 3)");
            return 0;
        }
        enableSigSet();

        bPid = fork();
        if (bPid == 0)
        {
            execl("./TriClient", "./TriClient", "BOT", "#!*-_?", (char *)NULL);
        }
    }
    else if (memPointer->Client1 != -11 && memPointer->Client2 != -12)
    {
        sendMessage("Entrambi i giocatori individuati. Partita avviata.\nRegole: ogni giocatore deve scegliere la propria mossa inserendo\nun numero da 1 a 9 in base alla cella che vuole occupare.\nI numeri delle celle sono organizzati come segue:\n\n 1 | 2 | 3\n___|___|___\n 4 | 5 | 6\n___|___|___\n 7 | 8 | 9\n   |   |   \n\nCominci per secondo.\nAttendi che l'altro giocatore faccia la sua mossa...\n", 0, 1);
        sendMessage("Entrambi i giocatori individuati. Partita avviata.\nRegole: ogni giocatore deve scegliere la propria mossa inserendo\nun numero da 1 a 9 in base alla cella che vuole occupare.\nI numeri delle celle sono organizzati come segue:\n\n 1 | 2 | 3\n___|___|___\n 4 | 5 | 6\n___|___|___\n 7 | 8 | 9\n   |   |   \n", 1, 0);
        memPointer->current = memPointer->Client1;
        if (semop(semId, &v_ops[0], 1) == -1)
        {
            perror("Error in Semaphore Operation (S, v, 3)");
            return 0;
        }
        enableSigSet();
    }
    else
    {
        if (semop(semId, &v_ops[0], 1) == -1)
        {
            perror("Error in Semaphore Operation (S, verr, 3)");
            return 0;
        }
        enableSigSet();
        perror("error during players research");
        closure();
    }

    if (timeOut != 0)
    {
        alarm(40);
    }

    while (1)
    {
        disableSigSet();
        if (semop(semId, &p_ops[3], 1) == -1)
        {
            if (errno != EINTR)
            {
                perror("Error in Semaphore Operation (S, p3, 1)");
                return 0;
            }
        }
        enableSigSet();

        alarm(0);

        if (memPointer->current == memPointer->Client1)
        {
            if (memPointer->move == -1)
            {
                sendMessage("Selezione non valida. Turno perso.\n", 1, 0);
            }
            else
            {
                compileMatrix(symbol1, 0);
            }

            if (semop(semId, &v_ops[2], 1) < 0)
            {
                perror("Error in Semaphore Operation (S, v2, 51)");
                return 0;
            }
        }
        else if (memPointer->current == memPointer->Client2)
        {
            if (memPointer->move == -1)
            {
                sendMessage("Selezione non valida. Turno perso.\n", 0, 1);
            }
            else
            {
                compileMatrix(symbol2, 1);
            }
            if (semop(semId, &v_ops[1], 1) < 0)
            {
                perror("Error in Semaphore Operation (S, v1, 51)");
                return 0;
            }
        }
        // }
        if (timeOut != 0)
        {
            alarm(timeOut);
        }
    }

    closure(); // da togliere

    return 0;
}