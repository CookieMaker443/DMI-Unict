#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <linux/limits.h>
#include <sys/stat.h>

#define BUFFER_CAPACITY 10

/* Struttura per il buffer condiviso (simile a 'shared' del professore) */
typedef struct {
    char pathfiles[BUFFER_CAPACITY][PATH_MAX];
    int index_in, index_out;
    unsigned int size;   // numero di elementi attualmente nel buffer
    unsigned int done;   // numero di thread DIR terminati
    pthread_mutex_t lock;
    sem_t empty, full;
} shared;

/* Struttura per il record file (simile a 'stat_pair') */
typedef struct {
    char pathfile[PATH_MAX];
    unsigned long size;
    char done;           // flag di terminazione: 1 se STAT ha terminato
    sem_t sem_w, sem_r;
} stat_pair;

/* Struttura dati per i thread DIR */
typedef struct {
    pthread_t tid;
    unsigned int thread_num;  // numerazione: 1,2,... n
    char *dir_path;
    shared *sh;
} dir_thread_data;

/* Struttura dati per il thread STAT */
typedef struct {
    pthread_t tid;
    unsigned int thread_num;  // numero identificativo (es. n+1)
    shared *sh;
    stat_pair *sp;
    int n_dirs;               // numero totale di thread DIR
} stat_thread_data;

/* Inizializzazione della struttura condivisa */
shared* init_shared() {
    shared *sh = malloc(sizeof(shared));
    if (!sh) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    sh->index_in = 0;
    sh->index_out = 0;
    sh->size = 0;
    sh->done = 0;
    if(pthread_mutex_init(&sh->lock, NULL) != 0) {
        perror("pthread_mutex_init");
        exit(EXIT_FAILURE);
    }
    if(sem_init(&sh->empty, 0, BUFFER_CAPACITY) != 0) {
        perror("sem_init empty");
        exit(EXIT_FAILURE);
    }
    if(sem_init(&sh->full, 0, 0) != 0) {
        perror("sem_init full");
        exit(EXIT_FAILURE);
    }
    return sh;
}

void destroy_shared(shared *sh) {
    pthread_mutex_destroy(&sh->lock);
    sem_destroy(&sh->empty);
    sem_destroy(&sh->full);
    free(sh);
}

/* Inizializzazione del record stat_pair */
stat_pair* init_statp() {
    stat_pair *sp = malloc(sizeof(stat_pair));
    if (!sp) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    sp->done = 0;
    if(sem_init(&sp->sem_w, 0, 1) != 0) {
        perror("sem_init sem_w");
        exit(EXIT_FAILURE);
    }
    if(sem_init(&sp->sem_r, 0, 0) != 0) {
        perror("sem_init sem_r");
        exit(EXIT_FAILURE);
    }
    return sp;
}

void destroy_statp(stat_pair *sp) {
    sem_destroy(&sp->sem_w);
    sem_destroy(&sp->sem_r);
    free(sp);
}

/* Funzione per i thread DIR */
void *dir_thread(void *arg) {
    dir_thread_data *td = (dir_thread_data *) arg;
    DIR *dr;
    struct dirent *entry;
    struct stat statbuf;
    char pathfile[PATH_MAX];

    if ((dr = opendir(td->dir_path)) == NULL) {
        perror("opendir");
        pthread_exit(NULL);
    }
    
    printf("[D-%u] scansione della cartella '%s'\n", td->thread_num, td->dir_path);
    
    while ((entry = readdir(dr)) != NULL) {
        /* Escludo "." e ".." */
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        
        snprintf(pathfile, PATH_MAX, "%s/%s", td->dir_path, entry->d_name);
        if (lstat(pathfile, &statbuf) == -1) {
            perror("lstat");
            continue;
        }
        if (S_ISREG(statbuf.st_mode)) {
            printf("[D-%u] trovato il file '%s' in '%s'\n", td->thread_num, entry->d_name, td->dir_path);
            /* Inserimento nel buffer condiviso */
            sem_wait(&td->sh->empty);
            pthread_mutex_lock(&td->sh->lock);
            
            strncpy(td->sh->pathfiles[td->sh->index_in], pathfile, PATH_MAX);
            td->sh->index_in = (td->sh->index_in + 1) % BUFFER_CAPACITY;
            td->sh->size++;
            
            pthread_mutex_unlock(&td->sh->lock);
            sem_post(&td->sh->full);
        }
    }
    closedir(dr);
    /* Segnalo il completamento di questo thread DIR */
    pthread_mutex_lock(&td->sh->lock);
    td->sh->done++;
    pthread_mutex_unlock(&td->sh->lock);
    
    printf("[D-%u] La cartella '%s' e' stata processata\n", td->thread_num, td->dir_path);
    pthread_exit(NULL);
}

/* Funzione per il thread STAT */
void *stat_thread(void *arg) {
    stat_thread_data *td = (stat_thread_data *) arg;
    char pathfile[PATH_MAX];
    struct stat statbuf;
    int finish = 0;
    
    while (!finish) {
        sem_wait(&td->sh->full);
        pthread_mutex_lock(&td->sh->lock);
        
        /* Estrazione del pathname dal buffer */
        strncpy(pathfile, td->sh->pathfiles[td->sh->index_out], PATH_MAX);
        td->sh->index_out = (td->sh->index_out + 1) % BUFFER_CAPACITY;
        td->sh->size--;
        
        /* Condizione di terminazione: se tutti i thread DIR hanno terminato e il buffer è vuoto */
        if (td->sh->done == td->n_dirs && td->sh->size == 0)
            finish = 1;
        
        pthread_mutex_unlock(&td->sh->lock);
        sem_post(&td->sh->empty);
        
        if (lstat(pathfile, &statbuf) == -1) {
            perror("lstat");
            continue;
        }
        printf("[STAT] il file '%s' ha dimensione %lu byte\n", pathfile, statbuf.st_size);
        
        /* Comunicazione al thread MAIN tramite stat_pair */
        sem_wait(&td->sp->sem_w);
        strncpy(td->sp->pathfile, pathfile, PATH_MAX);
        td->sp->size = statbuf.st_size;
        td->sp->done = finish;
        sem_post(&td->sp->sem_r);
    }
    pthread_exit(NULL);
}

/* Funzione main */
int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <dir-1> <dir-2> ... <dir-n>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    int n_dirs = argc - 1;
    /* Verifica che ogni argomento sia una directory */
    for (int i = 0; i < n_dirs; i++) {
        struct stat sb;
        if (stat(argv[i + 1], &sb) < 0 || !S_ISDIR(sb.st_mode)) {
            fprintf(stderr, "Errore: '%s' non e' una cartella valida\n", argv[i + 1]);
            exit(EXIT_FAILURE);
        }
    }
    
    shared *sh = init_shared();
    stat_pair *sp = init_statp();
    
    /* Creazione dei thread DIR */
    dir_thread_data *dir_td = malloc(sizeof(dir_thread_data) * n_dirs);
    if (!dir_td) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < n_dirs; i++) {
        dir_td[i].dir_path = argv[i + 1];
        dir_td[i].thread_num = i + 1;
        dir_td[i].sh = sh;
        if (pthread_create(&dir_td[i].tid, NULL, dir_thread, &dir_td[i]) != 0) {
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }
    
    /* Creazione del thread STAT */
    stat_thread_data td_stat;
    td_stat.n_dirs = n_dirs;
    td_stat.sh = sh;
    td_stat.sp = sp;
    td_stat.thread_num = n_dirs + 1; // ad es. thread STAT identificato come "n+1"
    if (pthread_create(&td_stat.tid, NULL, stat_thread, &td_stat) != 0) {
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }
    
    unsigned long total_bytes = 0;
    /* Ciclo di raccolta dei risultati da parte del MAIN */
    while (1) {
        sem_wait(&sp->sem_r);
        total_bytes += sp->size;
        if (sp->done)
            break;
        else
            printf("[MAIN] con il file '%s' il totale parziale è di %lu byte\n", sp->pathfile, total_bytes);
        sem_post(&sp->sem_w);
    }
    printf("[MAIN] il totale finale è di %lu byte\n", total_bytes);
    
    /* Attesa del termine dei thread */
    for (int i = 0; i < n_dirs; i++) {
        if (pthread_join(dir_td[i].tid, NULL) != 0) {
            perror("pthread_join");
            exit(EXIT_FAILURE);
        }
    }
    if (pthread_join(td_stat.tid, NULL) != 0) {
        perror("pthread_join");
        exit(EXIT_FAILURE);
    }
    
    destroy_shared(sh);
    destroy_statp(sp);
    free(dir_td);
    
    return 0;
}
