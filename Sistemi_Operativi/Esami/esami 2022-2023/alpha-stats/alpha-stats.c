#include "./lib-misc.h"
#include <ctype.h> //permette di gestire i caratteri
#include <fcntl.h> //mappare un file in memoria
#include <pthread.h> //gestione thread
#include <semaphore.h> //sincronizzazione dei thread con i semafori
#include <stdbool.h> //aggiunge il tipo bool
#include <stdio.h> 
#include <stdlib.h>
#include <sys/mman.h> //mappare un file in memoria e gestire i file
#include <sys/stat.h> //mappare un file in memoria e gestire i file
#include <unistd.h> 

typedef enum { AL_N, MZ_N, PARENT_N } thread_n; //definzione di tre thread
//il primo indica il thread che gestitsce i caratteri da 'a' a 'l'
//il secondo indica il thread che gestisce i caratteri da 'm' a 'z'.
//il terzo è il thread parent

typedef struct {
    char c; //carattere singolo da elaborare
    unsigned long stats[26]; //array per contare le occorrenze di ciascuna lettera dell'alfabeto inglese
    bool done; 
    sem_t sem[3]; //array di semafori per la sincronizzazione
} shared; //struttura dati condivisa

typedef struct {
    // dati privati
    pthread_t tid; //identificatore del thread
    char thread_n; //identifica uno dei thread definiti da enum

    // struttura dati condivisa
    shared *shared; //puntatore alla struttura dati condivisa
} thread_data;

// inizializza la struttura dati condivisa e restituisce il suo indirizzo
shared *init_shared() {
    int err; //numero di codici di errore 
    shared *sh = malloc(sizeof(shared)); //allocazione memoria condivisa

    // inizializza il campo done a 0
    sh->done = 0;

    // inizializza l'array stats a 0
    memset(sh->stats, 0, sizeof(sh->stats));

    // inizializza il semaforo del thread padre
    if ((err = sem_init(&sh->sem[PARENT_N], 0, 1)) != 0) //sem_init(prende come parametri il semaforo da inizializzare, secondo parametro indica se il semaforo è condiviso tra thread del processo(0) o se è condiviso tra i processi (1), valore del semaforo inizializzato)
        exit_with_err("sem_init", err);

    // inizializza i semafori destinati ai thread al ed mz
    if ((err = sem_init(&sh->sem[AL_N], 0, 0)) != 0)
        exit_with_err("sem_init", err);

    if ((err = sem_init(&sh->sem[MZ_N], 0, 0)) != 0)
        exit_with_err("sem_init", err);

    return sh;
}

void shared_destroy(shared *sh) {
    for (int i = 0; i < 3; i++)
        sem_destroy(&sh->sem[i]); //distruggiamo i semafori

    free(sh); //deallochiamo la memoria della struttura shared
}

//void* arg = puntatore alla struttura thread_data del thread chiamante.
void stats(void *arg) { 
    int err; //numero di codici di errore
    thread_data *td = (thread_data *)arg; //viene fatto per accedere ai dati specifici del thread

    while (1) {
        if ((err = sem_wait(&td->shared->sem[td->thread_n])) != 0) //viene eseguita una wait sul semaforo sem[AL_N] o sem(MZ_N)
            exit_with_err("sem_wait", err); 

        if (td->shared->done) //CONTROLLA il flag done se è 1 esce dal ciclo
            break;
 
        td->shared->stats[td->shared->c - 'a']++; //viene aumentato il contatore della lettere corrente

        if ((err = sem_post(&td->shared->sem[PARENT_N])) != 0) //sblocco il semaforo, seganalando al thread principale che il thread ha completato il suo lavoro
            exit_with_err("sem_post", err); 
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: %s <file.txt>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int err, fd; //file description del file di input
    thread_data td[2]; //contiene i dati specifici per i due thread
    shared *sh = init_shared(); //inizializzazione struttura condivisa
    char *map; //puntatore alla mappatura in memoria del file
    struct stat s_file; //struttura che contiene informazioni sul file

    // inizializzo le strutture dati per i thread
    for (int i = 0; i < 2; i++) { 
        td[i].shared = sh; 
        td[i].thread_n = i; //indica il tipo di thread 0 (AL) 1 (MZ)
    }

    // creazione thread

    //Creazione thread AL
    //int pthread_create(pthread_t *thread, pthread_attr_t *attr, void *(*start_routine)/(void *arg), void *arg);
    //primo elemento il thread da inizializzare, il secondo elemento NULL serve a indicare che vengono utilizzati gli attributi predefiniti
     if ((err = pthread_create(&td[AL_N].tid, NULL, (void *)stats, (void *)&td[AL_N])) != 0)
        exit_with_err("pthread_create", err);

    //Creazione thread MZ
    if ((err = pthread_create(&td[MZ_N].tid, NULL, (void *)stats, (void *)&td[MZ_N])) != 0)
        exit_with_err("pthread_create", err);

    // apro il file
    if ((fd = open(argv[1], O_RDONLY)) == -1)
        exit_with_err("open", err);

    // eseguo lo stat del file per conoscere le sua dimensione
    if ((err = fstat(fd, &s_file)) == -1)
        exit_with_err("fstat", err);

    // mappo il file in memoria in sola lettura
    if ((map = mmap(NULL, s_file.st_size, PROT_READ, MAP_SHARED, fd, 0)) == MAP_FAILED) 
        exit_with_err("mmap", err);

    // chiudo il file (non più necessario dopo aver effettuato la mappatura in memoria)
    if ((err = close(fd)) == -1)
        exit_with_err("close", err);

    // leggo il file carattere per carattere
    int i = 0;

    while (i < s_file.st_size) { //scorre il contenuto del file mappatto e processa ogni carattere alfabetico
        if ((map[i] >= 'a' && map[i] <= 'z') || (map[i] >= 'A' && map[i] <= 'Z')) {
            // aspetto che il thread lettore abbia completato
            if ((err = sem_wait(&sh->sem[PARENT_N])) != 0)
                exit_with_err("sem_wait", err);

            // inserisco il carattere nella struttura dati condivisa
            sh->c = tolower(map[i]); //Converto in minuscolo i caratteri 

            // sveglio il thread che deve gestire il carattere
            if (map[i] <= 'l') {
                if ((err = sem_post(&sh->sem[AL_N])) != 0)
                    exit_with_err("sem_post", err);
            } else {
                if ((err = sem_post(&sh->sem[MZ_N])) != 0)
                    exit_with_err("sem_post", err);
            }
        }

        i++; // incremento l'indice per l'array map
    }

    // aspetto che il thread lettore abbia completato
    if ((err = sem_wait(&sh->sem[PARENT_N])) != 0)
        exit_with_err("sem_wait", err);

    // stampo le statistiche
    printf("Statistiche sul file: %s\n", argv[1]);

    for (int i = 0; i < 26; i++)
        printf("%c: %lu\t", i + 'a', sh->stats[i]);

    printf("\n");

    // notifico ai thread la fine dei lavori
    sh->done = 1;
    if ((err = sem_post(&sh->sem[AL_N])) != 0)
        exit_with_err("sem_post", err);
    if ((err = sem_post(&sh->sem[MZ_N])) != 0)
        exit_with_err("sem_post", err);

    // attendo l'uscita dei thread
    for (int i = 0; i < 2; i++)
        if ((err = pthread_join(td[i].tid, NULL)) != 0)
            exit_with_err("pthread_join", err);

    // distruggo la struttura dati condivisa
    shared_destroy(sh);

    // rilascio la mappatura del file in memoria
    if ((err = munmap(map, s_file.st_size)) == -1)
        exit_with_err("munmap", err);

    exit(EXIT_SUCCESS);
}