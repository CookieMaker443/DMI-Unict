#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h> // struttura di accesso alla comunicazione interprocesso
#include <sys/sem.h> //libreria che manipola i semafori
#include <sys/shm.h> // gestisce la memoria condivisa
#include <sys/wait.h> //gestitsce i processi 
#include <time.h>
#include <unistd.h> //gestisce i processi 
#define SHM_SIZE sizeof(shm_data)
//enum è una variabile enumerativa che assume valori scelti dall'utente e vengono rappresentati con delle costanti ognuna corrisponde a un valore intero
enum { S_P1, S_P2, S_J, S_S }; //rappresentano i 4 semafori per i processi p1, p2, giudice e tabellone
enum { CARTA, FORBICE, SASSO }; //le tre mosse possibili da fare
char *mosse[3] = {"carta", "forbice", "sasso"}; //array  che serve a visualizzare la scelta della mossa

typedef struct {
    char mossa_p1;
    char mossa_p2; //le mosse dei due processi
    char vincitore; //il vincitore
    char done; //indica la fine del gioco
} shm_data;

int WAIT(int sem_id, int sem_num) {
    struct sembuf ops[1] = {{sem_num, - 1, 0}};
    return semop(sem_id, ops, 1);
}
int SIGNAL(int sem_id, int sem_num) {
    struct sembuf ops[1] = {{sem_num, + 1, 0}};
    return semop(sem_id, ops, 1);
}

int init_shm() { //funziona che inizializza un segmento di memoria condivisa e resituitisce il suo descrittore
    int shm_des; //id del segmento di memoria condivisa 
//shmget è una funzione usata per creatre (o ottenre) un segmento di memoria condivisa
//IPC_PRIVATE indica che voglio creare un segmento senza un identificatore specifico
//SHM_SIZE p la dimensione del segmento di memoria condivisa
//IPC_CREAT | 0600 specifica che il segmento deve essere creato se non esiste e già e assegna permessi di lettura e scrittura al proprietario 0600
    if ((shm_des = shmget(IPC_PRIVATE, SHM_SIZE, IPC_CREAT | 0600)) == -1) {
        perror("shm_des"); //stampa un messaggio di errore 
        exit(1); //termian il programma  
    }

    return shm_des; //restituisce l'id
}

int init_sem() { //inizializza un insieme di semafori e imposta i valori necessarri per la sincronizzazione dei processi 
    int sem_des;

//semget viene utilizzato per creare un nuovo set di semafori
//IPC_PRIVATE indica che vogliamo creare un nuovo insieme
// 4 indica il numero di semafori
// IPC_CREAT | 0600 crea il set dei semafori con i permessi di lettura e scrittura
//se semget == -1 quindi sem_des è -1 da errore
    if ((sem_des = semget(IPC_PRIVATE, 4, IPC_CREAT | 0600)) == -1) {
        perror("semget");
        exit(1);
    }

//semctl viene utilizzato per impostare il valore del semaforo e prende come parametri insieme dei semafori, il primo semaforo e il valore del semaforo iniziale
//SETVAL all'interno di semctl, è un valore che serve come comando a specificare l'operazione di impostazione del valore iniziale del semaforo indicato.
//controllo se il valore del semaforo è -1 se se lo è restituisce errore
//vengono impostati a 1 per consentire l'accesso ai processi immediato
    if (semctl(sem_des, S_P1, SETVAL, 1) == -1) {
        perror("semctl SETVAL S_P1");
        exit(1);
    }

//semctl funziona che imposta il valore del semaforo in questo caso prende come parametro l'insieme dei semafori, il semaforo p2 che ci interessa e il valore del semaforo da impostare
//controllo se il valore del semaforo è -1 
    if (semctl(sem_des, S_P2, SETVAL, 1) == -1) {
        perror("semctl SETVAL S_P2");
        exit(1);
    }
//semctl in questo caso i valori dei semafori vengono impostati a zero quindi bloccati perchè verranno incrementi nel caso di vittoria o pareggio 
    if (semctl(sem_des, S_J, SETVAL, 0) == -1) {
        perror("semctl SETVAL S_J");
        exit(1);
    }

    if (semctl(sem_des, S_S, SETVAL, 0) == -1) {
        perror("semctl SETVAL S_T");
        exit(1);
    }

    return sem_des;
}

//la funzione prende come parametro id del giocatore s_p1 o s_p2
//id del segmento di memoria condivisa e l'id del set di semafori 
void player_child(char id, int shm_des, int sem_des) {
    //per una mossa casuale utilizzo stand(time(NULL));
    srand(time(NULL));
    shm_data *data; //dichiariamo shm_data per accederre alla memoria condivisa
//shmat() è una funzione che collega il segmento di memoria condivisa al processo
    if ((data = (shm_data *)shmat(shm_des, NULL, 0)) == (shm_data *)-1) {
        perror("shmat");
        exit(1);
    }
//serve a forzare l'uscita questo ciclo
    while (1) {
        //controllo se l'id è quello del giocatore 1
        if (id == S_P1) {
            //se lo è chiamo una wait
            WAIT(sem_des, S_P1);
            //controllo se il campo di memoria condivisa del giocatore 1 ha finito
            if (data->done)
                exit(0); //il processo termina 
            //altrimenti imposta la mossa del primo processo in maniera casuale
            data->mossa_p1 = rand() % 3;
            printf("P1: mossa '%s'\n", mosse[data->mossa_p1]);
        } else {
        //se l'id è quello del 2 chiama la wait sul giocatore 2
            WAIT(sem_des, S_P2);
        //Controlla se ha finito
            if (data->done)
                exit(0);
        //imposta la mossa del primo processo
            data->mossa_p2 = rand() % 3;
            printf("P2: mossa '%s'\n", mosse[data->mossa_p2]);
        }
    //chiamiamo una signal per entrambi i processi sul semaforo giudice per segnale che le mosse sono pronte e uscire dal ciclo
        SIGNAL(sem_des, S_J);
    }
}

char whowins(char mossa1, char mossa2) {
    //se le mosse sono uguali pareggio return 0
    if (mossa1 == mossa2)
        return 0;

    //se il player 1 vince return 1 altrimenti return 2
    if ((mossa1 == CARTA && mossa2 == SASSO) ||
        (mossa1 == FORBICE && mossa2 == CARTA) ||
        (mossa1 == SASSO && mossa2 == FORBICE))
        return 1;

    return 2;
}

void judge_child(int shm_des, int sem_des, int n_partite) {
    shm_data *data; //dichiariamo shm_data per accederre alla memoria condivisa
    unsigned match = 0; //indica il match vinto 
    char winner;

//shmat() è una funzione che collega il segmento di memoria condivisa al processo
//controllo se il dato == -1 e mi da errore 
    if ((data = (shm_data*)shmat(shm_des, NULL, 0)) == (shm_data *)-1) {
        perror("shmat");
        exit(1);
    }

    while (1) {
//Doppio wait perchè il giudice aspetta le mosse di entrambi i giocatori
        WAIT(sem_des, S_J); 
        WAIT(sem_des, S_J);
//impostiamo il valore del vincitore chiamando la funzione whowins
        winner = whowins(data->mossa_p1, data->mossa_p2);
//se il valore è 0 vuol dire che c'è stato un paraggio
        if (!winner) {
//viene scritto e vengono risvegliati entrambi i semafori
            printf("G: partita n.%d patta e quindi da ripetere\n", match + 1);
            SIGNAL(sem_des, S_P1);
            SIGNAL(sem_des, S_P2);
            continue;
        }
//altrimenti impostiamo nella memoria condivisa il vincitore 
        data->vincitore = winner;
        match++; //incremento il numero dei match
        printf("G: partita n.%d vinta da P%d\n", match, winner);
//aumento il semaforo del tabellone e poi controllo se il numero di match è uguale al numero di partite per concludere 
        SIGNAL(sem_des, S_S);

        if (match == n_partite)
            break;
    }
//exit per finire
    exit(0);
}

void scoreboard_child(int shm_des, int sem_des, int n_partite) {
    shm_data *data; //dichiariamo shm_data per accederre alla memoria condivisa
    unsigned score[2] = {0, 0};  //indica il numero di partite vinte di entrambi i giocatoiri

//shmat() è una funzione che collega il segmento di memoria condivisa al processo
//controllo se il dato == -1 e mi da errore 
    if ((data = (shm_data *)shmat(shm_des, NULL, 0)) == (shm_data *)-1) {
        perror("shmat");
        exit(1);
    }
//ciclo per contare il numero di partite
    for (int i = 0; i < n_partite - 1; i++) {
        WAIT(sem_des, S_S); //decremento il semaforo del tabbellone per iniziare la partita

        score[data->vincitore - 1]++; //incremento lo score del vincitore 
        //indico i punteggi
        printf("T: classifica temporanea: P1: %d P2: %d\n", score[0], score[1]);
        //aumento i semafori di entrambi i processi per continuare le partite
        SIGNAL(sem_des, S_P1);
        SIGNAL(sem_des, S_P2);
    }
//decremento il semaforo del tabellone 
    WAIT(sem_des, S_S);
    score[data->vincitore - 1]++; //incremento il vincitore
    printf("T: classifica finale: P1: %d P2: %d\n", score[0], score[1]); //indico i punteggi
    //controllo se lo score del processo 1 è maggiore del processo 2 
    if (score[0] > score[1])
        printf("T: vincitore del torneo: P1"); 
    else
        printf("T: vincitore del torneo: P2");

    data->done = 1; //indica che il gioco è terminato 
    SIGNAL(sem_des, S_P1); //riattivo i semafori dei due processi consentendo a loro di terminare 
    SIGNAL(sem_des, S_P2);
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <numero-partite>\n", argv[0]);
        exit(1);
    }
    //inizializzazione delle variabili condivise
    int shm_des = init_shm();
    int sem_des = init_sem();
//crea un nuovo processo per il player 1
    if (!fork())
        player_child(S_P1, shm_des, sem_des);

    // Utile a settare un seme diverso per i due player
    sleep(1);
//crea un nuovo processo per il player 2 chiamando la funzione player_child
    if (!fork())
        player_child(S_P2, shm_des, sem_des);
//processo per il giudice, atoi viene utilizzato per convertire una stringa in un numero
    if (!fork())
        judge_child(shm_des, sem_des, atoi(argv[1]));

    if (!fork())
        scoreboard_child(shm_des, sem_des, atoi(argv[1]));
//Aspetta che tutti i 4 processi figli (giocatori, giudice e tabellone) terminino.
    for (int i = 0; i < 4; i++)
        wait(NULL); //Blocca il processo principale finché un processo figlio termina.
                    //Si esegue 4 volte perché sono stati creati 4 processi figli.
    shmctl(shm_des, IPC_RMID, NULL);
    semctl(sem_des, 0, IPC_RMID);
}