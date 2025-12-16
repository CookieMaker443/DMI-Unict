#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>
#include <dirent.h>
#include <linux/limits.h>
#include <sys/stat.h>
#include <unistd.h>
#define QUEUE_CAPACITY 10

typedef struct{
    char path[QUEUE_CAPACITY][PATH_MAX];
    char index_in, index_out;
    unsigned long done;
    unsigned long size;

    pthread_mutex_t lock;
    sem_t full, empty;
}shared_sd;

typedef struct{
    char pathfile[PATH_MAX];
    unsigned long size;
    bool done;

    sem_t sem_r, sem_w; //semafori di lettura e scrittura per il main;
}shared_ms;

typedef struct{
    pthread_t tid;
    unsigned thread_n;
    char* pathdir;

    shared_sd* sd;
    shared_ms* ms;
}thread_data;

shared_sd* init_shared_sd(){
    shared_sd* sd = malloc(sizeof(shared_sd));

    sd->index_in = sd->index_out = sd->size = sd->done = 0;

    if(pthread_mutex_init(&sd->lock) != 0){
        perror("pthread_mutex_init");
        exit(EXIT_FAILURE);
    }

    if(sem_init(&sd->full, 0, 0) != 0){
        perror("sem_init");
        exit(EXIT_FAILURE);
    }

    if(sem_init(&sd->empty, 0, QUEUE_CAPACITY) != 0){
        perror("sem_init");
        exit(EXIT_FAILURE);
    }

    return sd;
}

void shared_sd_destroy(shared_sd* sd){
    pthread_mutex_destroy(&sd->lock);
    sem_destroy(&sd->full);
    sem_destroy(&sd->empty);
    free(sd);
}

shared_ms* init_shared_ms(){
    shared_ms* ms = malloc(sizeof(shared_ms));

    ms->done = 0;

    if(sem_init(&ms->sem_w, 0, 1) != 0){
        perror("sem_init");
        exit(EXIT_FAILURE);
    }

    if(sem_init(&ms->sem_r, 0, 0) != 0){
        perror("sem_init");
        exit(EXIT_FAILURE);
    }

    return ms;
}

void shared_ms_destroy(shared_ms* ms){
    sem_destroy(&ms->sem_w);
    sem_destroy(&ms->sem_r);
    free(ms);
}

void dir_thread(void* arg){
    thread_data* td = (thread_data*)arg;
    DIR* dr;
    struct stat statbuf;
    struct dirent* entry;
    char pathfile[PATH_MAX];

    if((dr = opendir(td->pathdir)) == NULL){
        perror("opendir");
        exit(EXIT_FAILURE);
    }

    printf("[D-%lu] scansione della cartella '%s'\n", td->thread_n, td->pathdir);

    while((entry = readdir(dr))){
        snprintf(pathfile, PATH_MAX, "%s/%s", td->pathdir, entry->d_name); //Costruisce il percorso assoluto del file

        if(lstat(pathfile, &statbuf) == -1){
            perror("lstat");
            exit(EXIT_FAILURE); 
        }

        if(S_ISREG(statbuf.st_mode)){
            printf("[D-%lu] trovato il file '%s' in %s\n", td->thread_n, entry->d_name, td->pathdir);

            if(sem_wait(&td->sd->empty) != 0){
                perror("sem_wait");
                exit(EXIT_FAILURE);
            }

            if(pthread_mutex_lock(&td->sd->lock) != 0){
                perror("pthread_mutex_lock");
                exit(EXIT_FAILURE);
            }

            td->sd->index_in = (td->sd->index_in + 1) % QUEUE_CAPACITY;
            td->sd->size++;

            strncpy(td->sd->path[td->sd->index_in], pathfile, PATH_MAX);

            if(pthread_mutex_unlock(&td->sd->lock) != 0){
                perror("pthread_mutex_unlock");
                exit(EXIT_FAILURE);
            }

            if(sem_post(&td->sd->full) != 0){
                perror("sem_post");
                exit(EXIT_FAILURE);
            }
        }
    }

    if(pthread_mutex_lock(&td->sd->lock) != 0){
        perror("phtread_mutex_lock");
        exit(EXIT_FAILURE);
    }

    td->sd->done++;

    if(pthread_mutex_unlock(&td->sd->lock) != 0){
        perror("pthread_mutex_unlock");
        exit(EXIT_FAILURE);
    }
    
    closedir(dr);
}

void stat_thread(void* arg){
    thread_data* td = (thread_data*)arg;
    struct stat statbuf;
    char* filepath;
    bool done = 0;

    while(!done){
        if(sem_wait(&td->sd->full) != 0){
            perror("sem_wait");
            exit(EXIT_FAILURE);
        }

        if(sem_wait(&td->ms->sem_w) != 0){
            perror("sem_Wait");
            exit(EXIT_FAILURE);
        }

        if(pthread_mutex_lock(&td->sd->lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }

        td->sd->index_out = (td->sd->index_out + 1) % QUEUE_CAPACITY;
        td->sd->size--;

        if(td->sd->done == (td->thread_n - 1) && td->sd->size == 0){
            td->ms->done = 1;
            done = 1;
        }

        filepath = td->sd->path[td->sd->index_out];

        if(lstat(filepath, &statbuf) == -1){
            perror("lstat");
            exit(EXIT_FAILURE);
        }
        
        printf("[STAT] il file '%s' ha dimensione %lu byte\n", filepath, statbuf.st_size);
        
        strncpy(td->ms->pathfile, filepath, PATH_MAX);
        td->ms->size = statbuf.st_size;

        if(pthread_mutex_unlock(&td->sd->lock) != 0){
            perror("pthread_mutex_unlock");
            exit(EXIT_FAILURE);
        }

        if(sem_post(&td->ms->sem_r) != 0 ){
            perror("sem_post");
            exit(EXIT_FAILURE);
        }   

        if(sem_post(&td->sd->empty) != 0){
            perror("sem_post");
            exit(EXIT_FAILURE);
        }
    }
}

int main(int argc, char** argv){
    if(argc < 2){
        fprintf(stderr, "Usage: %s <dir-1> <dir-2> <...> <dir-n>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    thread_data td[argc];
    shared_sd* sd = init_shared_sd();
    shared_ms* ms = init_shared_ms();
    unsigned long total_bytes = 0;

    for(unsigned i = 0; i < argc - 1; i++){
        td[i].sd = sd;
        td[i].pathdir = argv[i + 1];
        td[i].thread_n = i + 1;

        if(pthread_create(&td[i].tid, NULL, (void*)dir_thread, &td[i]) != 0){
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }

    td[argc - 1].sd = sd;
    td[argc - 1].ms = ms;
    td[argc - 1].thread_n = argc;

    if(pthread_create(&td[argc - 1].tid, NULL, (void*)stat_thread, &td[argc - 1]) != 0){
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    while(1){
        if(sem_wait(&ms->sem_r) != 0){
            perror("sem_wait");
            exit(EXIT_FAILURE);
        }

        total_bytes += ms->size;

        if(ms->done){
            break;
        }else{
            printf("[MAIN] con il file %s il totale parziale è %lu bytes\n", ms->pathfile, total_bytes);
        }

        if(sem_post(&ms->sem_w) != 0){
            perror("sem_post");
            exit(EXIT_FAILURE);
        }
    }

    printf("[MAIN] il totale finale è di %lu bytes.\n", total_bytes);

    for(unsigned i = 0; i < argc; i++){
        if(pthread_join(td[i].tid, NULL) != 0){
            perror("pthread_join");
            exit(EXIT_FAILURE);
        }
    }

    shared_ms_destroy(ms);
    shared_sd_destroy(sd);
}