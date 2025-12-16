#include <dirent.h> //gestione della directory
#include <linux/limits.h> //fornisce costanti (PATH_MAX: che fornisce la lunghezza massima di un percorso)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/msg.h> //gestione coda dei messaggi
#include <sys/sem.h> //gestione semafori
#include <sys/shm.h> //gestione memoria condivisa
#include <sys/stat.h> //gestisce i dati restituiti dalla funziones stat()
#include <sys/types.h> //gestioni dei tipi di dati utilizzati nel codice sorgente del sistema
#include <sys/wait.h> //gestione processi figli
#include <unistd.h> //funzioni base di sistema (Fork, exec ecc)
#define SHM_SIZE sizeof(shm_data) //dimensione segmento di memoria condivisa 
#define MSG_SIZE sizeof(msg) - sizeof(long) //dimensione coda dei messaggi
//Il valore è ottenuto sottraendo la dimensione del campo long type dal totale

enum { S_SCANNER, S_STATER };//semafori

typedef struct {
    unsigned id; // identificativo del processo Scaner
    char path[PATH_MAX]; //buffer che contiene il percorso del file (pathname)
    char done; //completamento del processo Scanner nella mememoria condivisa
} shm_data; //memoria condivisa
 
typedef struct {
    long type; //tipo del messaggio
    unsigned id; //identificatore del processo Scanner 
    unsigned long value; //valore calcolato dal processo Starter (spazio occupato in blocchi)
    char done; //completamento da parte del processo nella coda dei messaggi
} msg;
//operazioni semafori
int WAIT(int sem_id, int sem_num) { 
    struct sembuf ops[1] = {{sem_num, -1, 0}};
    return semop(sem_id, ops, 1);
}
int SIGNAL(int sem_id, int sem_num) {
    struct sembuf ops[1] = {{sem_num, +1, 0}};
    return semop(sem_id, ops, 1);
}

int init_shm() { //inizializzazione memoriaa condivisa
    int shm_des;

    if ((shm_des = shmget(IPC_PRIVATE, SHM_SIZE, IPC_CREAT | 0600)) == -1) {
        perror("shmget");
        exit(1);
    }

    return shm_des;
}

int init_sem() { //inizializzazione dei semafori
    int sem_des;

    if ((sem_des = semget(IPC_PRIVATE, 2, IPC_CREAT | 0600)) == -1) {
        perror("semget");
        exit(1);
    }

    if (semctl(sem_des, S_SCANNER, SETVAL, 1) == -1) {
        perror("semctl SETVAL S_SCANNER");
        exit(1);
    }

    if (semctl(sem_des, S_STATER, SETVAL, 0) == -1) {
        perror("semctl SETVAL S_STATER");
        exit(1);
    }

    return sem_des;
}

int init_queue() { //inizializzazione della coda dei messaggi
    int queue;

    if ((queue = msgget(IPC_PRIVATE, IPC_CREAT | 0600)) == -1) {
        perror("msgget");
        exit(1);
    }

    return queue;
}

void scanner(char id, int shm_des, int sem_des, char *path, char base) {
    DIR *d; //puntatore a una struttura dir (directory aperta), DIR è un tipo che rappresenta un flusso di directory.
    struct dirent *dirent; //struttura che rappresenta una voce di una directory ( un file o sottodirectory)
    shm_data *data; //memoria condivisa

    if ((data = (shm_data *)shmat(shm_des, NULL, 0)) == (shm_data *)-1) {
        perror("shmat");
        exit(1);
    }
//opendir è una funzione utilizzata per aprire una directory in modo tale che possa essere letta con readdir()
//se viene trovata restituisce un puntatore a DIR object. Questo oggetto descrive la directory e viene utilizzato nelle operazioni successive sulla directory
//DIR *opendir(const char *dirname): Il parametro che è prende è la directory da aprire   
    if ((d = opendir(path)) == NULL) {
        perror("opendir");
        exit(1);
    }
//readdir() è una funzione che legge una voce da una directory
//struct dirent *readdir(DIR *dir): restituisce un puntatore a una struttura dirent che descrive
// la voce di directory successiva nel flusso di directory associato a DIR
    while ((dirent = readdir(d))) { //ciclo che legge le voci della directory corrente
        if (!strcmp(dirent->d_name, ".") || !strcmp(dirent->d_name, "..")) //salta le vpco speciali come il . (directory corrente) e ..(directory genitore)
            continue;
        else if (dirent->d_type == DT_REG) { //controlla se il tipo del file della voce della directory è  DT_REG: è un file normale.
            WAIT(sem_des, S_SCANNER);
            sprintf(data->path, "%s/%s", path, dirent->d_name); //funzione che formatta e memorizza i dati (la stringa)  in un array o in una struttura dati
            data->done = 0;
            data->id = id;
            SIGNAL(sem_des, S_STATER);
        } else if (dirent->d_type == DT_DIR) { //controlla se  il tipo del file della voce della directory è DT_DIR: è una directory.
            char tmp[PATH_MAX]; //variabile temporanea che conterra il pathname individuato (ovvero quello di una directory)
            sprintf(tmp, "%s/%s", path, dirent->d_name);
            scanner(id, shm_des, sem_des, tmp, 0); //richiama ricorsivamente la funzione scanner per esplorare la sottodirectory per questo viene messo il valore 0 al parametro char base
        }
    }

    closedir(d); //chiude il flusso di directory
// se base è 1 (quindi la directory della radice è stata scansionata) 
    if (base) {
        WAIT(sem_des, S_SCANNER);
        data->done = 1;
        SIGNAL(sem_des, S_STATER);
        exit(0);
    }
}

void stater(int shm_des, int sem_des, int queue, unsigned n) { // legge i percorsi dei file regolari dalla memoria condivisa.
    struct stat statbuf; //variabile usata per memorizzare le informazioni sui file forniti dalla funzione stat
    shm_data *data; //memoria condivisa
    msg m; //messaggio
    m.type = 1; //indichiamo il tipo di messaggio, in questo caso standard
    m.done = 0; 
    unsigned done_counter = 0; //counter che tiene traccia del numero di processi scaner completati

    if ((data = shmat(shm_des, NULL, 0)) == (shm_data *)-1) {
        perror("shmat");
        exit(1);
    }

    while (1) {
        WAIT(sem_des, S_STATER);

        if (data->done) { //controlla se il flag done è impostato, indicando che un processo scanner ha completato il lavoro
            done_counter++; //aumenta il contatore dei processi finiti

            if (done_counter == n) //controllo se il contatore è uguale al numero dei processi
                break;
            else { //altrimenti risveglia il processo scanner
                SIGNAL(sem_des, S_SCANNER);
                continue;
            }
        }

        if (stat(data->path, &statbuf) == -1) { //ottiene informazioni sullo stato di un file specificato e le inserisce nell'area di memoria indicata dall'argomento buf .
            perror("stat");
            exit(1);
        }

        m.id = data->id;
        m.value = statbuf.st_blocks;
        SIGNAL(sem_des, S_SCANNER);

        if (msgsnd(queue, &m, MSG_SIZE, 0) == -1) { //invia i messaggi alla coda dei messaggi
            perror("msgsnd");
            exit(1);
        }
    }

    m.done = 1;

    if (msgsnd(queue, &m, MSG_SIZE, 0) == -1) { //invia il messaggio alla coda dei messaggi
        perror("msgsnd");
        exit(1);
    }

    exit(0);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s [path-1] [path-2] [...]\n", argv[0]);
        exit(1);
    }

    int shm_des = init_shm();
    int sem_des = init_sem();
    int queue = init_queue();
    msg m;
//Array per accumulare il numero totale di blocchi su disco per ciascun percorso fornito come input.
    unsigned long blocks[argc - 1]; 

    // Stater
    if (!fork())
        stater(shm_des, sem_des, queue, argc - 1); //(argc - 1 come parametri)

    // Scanners
    for (int i = 1; i < argc; i++) //ciclo che scrorre gli argomenti
        if (!fork())
            scanner(i - 1, shm_des, sem_des, argv[i], 1); 

    for (int i = 0; i < argc - 1; i++) //inizializza l'array blocks con il valore 0 per ogni percorso
        blocks[i] = 0;

    while (1) { 
        if (msgrcv(queue, &m, MSG_SIZE, 0, 0) == -1) {
            perror("msgrcv");
            exit(1);
        }

        if (m.done)
            break;

        blocks[m.id] += m.value; //Accumula i blocchi occupati per il percorso identificato da m.id
    }

    for (int i = 0; i < argc - 1; i++)
        printf("%ld %s\n", blocks[i], argv[i + 1]);

    shmctl(shm_des, IPC_RMID, 0); //libera la memoria condivisa+
    semctl(sem_des, 0, IPC_RMID, 0); //elimina i semafori
    msgctl(queue, IPC_RMID, 0); //Rimuove la coda dei messaggi
}