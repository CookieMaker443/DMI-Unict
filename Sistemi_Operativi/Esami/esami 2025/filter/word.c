#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <semaphore.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>

#define PATH_SIZE 10
#define BUFFER_SIZE 10
#define MAX_LENGTH 1024

typedef struct{
    char path[PATH_SIZE][PATH_MAX];
    int index_in, index_out;
    int size;
    unsigned ndone;
    bool done;

    pthread_mutex_t lock;
    sem_t full, empty;
}shared_ds;

typedef struct{
    char pathname[BUFFER_SIZE][PATH_MAX];
    int occorrenze;
    int index_in, index_out;
    int size;
    bool done;
    
    pthread_mutex_t lock;
    sem_t full, empty;
}shared_ms;

typedef struct{
    char* pathfile;
    pthread_t tid;
    unsigned thread_n;
    int ndir;
    shared_ds* sd;
}thread_dir;

typedef struct{
    pthread_t tid;
    char* word;
    shared_ds* sd;
    shared_ms* ms;
}thread_search;

shared_ds* init_sharedds(){
    shared_ds* sh = malloc(sizeof(shared_ds));

    sh->index_in = sh->index_out = sh->size = sh->ndone = 0;
    sh->done = false;

    if(pthread_mutex_init(&sh->lock, NULL) != 0){
        perror("pthread_mutex_init");
        exit(EXIT_FAILURE);
    }
    
    if(sem_init(&sh->full, 0, 0) != 0){
        perror("sem_init");
        exit(EXIT_FAILURE);
    }

    if(sem_init(&sh->empty, 0 , PATH_SIZE) != 0){
        perror("sem_init");
        exit(EXIT_FAILURE);
    }

    return sh;
}

shared_ms* init_sharedms(){
    shared_ms* sh = malloc(sizeof(shared_ms));

    sh->index_in = sh->index_out = sh->size = 0;
    sh->done = false;

    if(pthread_mutex_init(&sh->lock, NULL) != 0){
        perror("pthread_mutex_init");
        exit(EXIT_FAILURE);
    }

    if(sem_init(&sh->full, 0, 0) != 0){
        perror("sem_init");
        exit(EXIT_FAILURE);
    }

    if(sem_init(&sh->empty, 0, BUFFER_SIZE) != 0){
        perror("sem_init");
        exit(EXIT_FAILURE);
    }

    return sh;
}

void destroy_sharedds(shared_ds* sh){
    pthread_mutex_destroy(&sh->lock);
    sem_destroy(&sh->full);
    sem_destroy(&sh->empty);
    free(sh);
}

void destroy_sharedms(shared_ms* sh){
    sem_destroy(&sh->full);
    sem_destroy(&sh->empty);
    free(sh);
}

void dir_function(void* arg){
    thread_dir* td = (thread_dir*)arg;

    DIR* dr;
    char path[PATH_MAX];
    struct stat statbuf;
    struct dirent* entry;

    if((dr = opendir(td->pathfile)) == NULL){
        perror("opendir");
        exit(EXIT_FAILURE);
    }

    while((entry = readdir(dr))){
        snprintf(path, PATH_MAX, "%s/%s", td->pathfile, entry->d_name);

        if(lstat(path, &statbuf) == -1){
            perror("lstat");
            exit(EXIT_FAILURE);
        }

        if(S_ISREG(statbuf.st_mode)){
            if(sem_wait(&td->sd->empty) != 0){
                perror("sem_wait");
                exit(EXIT_FAILURE);
            }

            if(pthread_mutex_lock(&td->sd->lock) != 0){
                perror("pthread_mutex_lokc");ù
                exit(EXIT_FAILURE);
            }

            td->sd->index_in = (td->sd->index_in + 1) % PATH_SIZE;
            td->sd->size++;
            strncpy(td->sd->path[td->sd->index_in], path, PATH_MAX);

            if(pthread_mutex_unlock(&td->sd->lock) != 0){
                perror("Pthread_mutex_unlock");
                exit(EXIT_FAILURE);
            }

            if(sem_post(&td->sd->full) != 0){
                perror("sem_post");
                exit(EXIT_FAILURE);
            }
        }
    }
    closedir(dr);

    if(pthread_mutex_lock(&td->sd->lock) != 0){
        perror("pthread_mutex_lock");
        exit(EXIT_FAILURE);
    }

    td->sd->ndone++;

    if(td->sd->ndone == td->ndir && td->sd->size == 0){
        td->sd->done = true;
        if(sem_post(&td->sd->full) != 0){
            perror("sem_post");
            exit(EXIT_FAILURE);
        }
    }

    if(pthread_mutex_unlock(&td->sd->lock) != 0){
        perror("pthread_mutex_lock");
        exit(EXIT_FAILURE);
    }
}

int counter_occorrenze(const char* pathname, const char* word){
    int counter = 0;
    const char* p = pathname;
    while((p = strcasestr(p, word)) != NULL){
        counter++;
        p += strlen(word);
    }
    return counter;
}

int search_word(const char* path, const char* word){
    FILE* f;
    char buffer[MAX_LENGTH];
    int totale = 0;

    if((f = fopen(path, "r")) == NULL){
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    while(fgets(buffer, MAX_LENGTH, f)){
        totale += counter_occorrenze(buffer, word);
    }
    fclose(f);
    return totale;
}

void search_function(void* arg){
    thread_search* td = (thread_search*)arg;
    char buffer[MAX_LENGTH];
    int occorrenze = 0;

    while(1){
        if(sem_wait(&td->sd->full) != 0){
            perror("sem_wait");
            exit(EXIT_FAILURE);
        }

        if(pthread_mutex_lock(&td->sd->lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }

        if(td->sd->done && td->sd->size == 0){
            if(pthread_mutex_unlock(&td->sd->lock) != 0){
                perror("Pthraed-Mutex_unlock");
                exit(EXIT_FAILURE);
            }
            break;
        }

        td->sd->index_out = (td->sd->index_out + 1) % PATH_SIZE;
        td->sd->size--;
        strncpy(buffer, td->sd->path[td->sd->index_out], PATH_MAX);

        if(pthread_mutex_unlock(&td->sd->lock) != 0){
            perror("pthread_mutex_unlock");
            exit(EXIT_FAILURE);
        }

        if(sem_post(&td->sd->empty) != 0){
            perror("sem_post");
            exit(EXIT_FAILURE);
        }

        occorrenze = search_word(buffer, td->word);
        printf("[SEARCH] nel file '%s' sono state trovate %d occorrenze\n", buffer , occorrenze);

        if(occorrenze > 0){
            if(sem_wait(&td->ms->empty) != 0){
                perror("sem_wait");
                exit(EXIT_FAILURE);
            }

            if(pthread_mutex_lock(&td->ms->lock) != 0){
                perror("pthread_mutex_lock");
                exit(EXIT_FAILURE);
            }

            td->ms->index_in = (td->ms->index_in + 1) % BUFFER_SIZE;
            td->ms->size++;
            strncpy(td->ms->pathname[td->ms->index_in], buffer, PATH_MAX);
            td->ms->occorrenze = occorrenze;

            if(pthread_mutex_unlock(&td->ms->lock) != 0){
                perror("pthread_mutex_unlock");
                exit(EXIT_FAILURE);
            }

            if(sem_post(&td->ms->full) != 0){
                perror("sem_post");
                exit(EXIT_FAILURE);
            }
        }
    }
}

int main(int argc, char** argv){
    if(argc < 4){
        fprintf(stderr, "Usage: %s <word> <dir-1> <dir-2> <...> <dir-n>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    int ndir = argc - 2;
    shared_ds* sd = init_sharedds();
    shared_ms* ms = init_sharedms();
    thread_dir* td = malloc(sizeof(thread_dir) * ndir);

    for(int i = 0; i < ndir; i++){
        td[i].ndir = ndir;
        td[i].pathfile = argv[i + 2];
        td[i].sd = sd;
        td[i].thread_n = i;
        if(pthread_create(&td[i].tid, NULL, (void*)dir_function, &td[i]) != 0){
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }

    thread_search sr;
    sr.ms = ms;
    sr.sd = sd;
    sr.word = argv[1];
    if(pthread_create(&sr.tid, NULL, (void*)search_function, &sr) != 0){
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    int occorrenze = 0;
    char path[PATH_MAX];
    while(1){
        if(sem_wait(&ms->full) != 0){
            perror("sem_wait");
            exit(EXIT_FAILURE);
        }

        if(pthread_mutex_lock(&ms->lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }

        if(ms->size == 0 && sd->done){
            if(pthread_mutex_unlock(&ms->lock) != 0){
                perror("pthread_mutex_unlock");
                exit(EXIT_FAILURE);
            }
            break;
        }


        ms->index_out = (ms->index_out + 1) % BUFFER_SIZE;
        ms->size--;
        occorrenze += ms->occorrenze;
        strncpy(path, ms->pathname[ms->index_out], PATH_MAX);
        printf("[MAIN] con il file '%s' il totale parziale è di %d occorrenze\n", path, occorrenze);

        if(pthread_mutex_unlock(&ms->lock) != 0){
            perror("pthread_mutex_Unlock");
            exit(EXIT_FAILURE);
        }

        if(sem_post(&ms->empty) != 0){
            perror("sem_post");
            exit(EXIT_FAILURE);
        }
    }
    
    printf("[MAIN] il totale finale è di %d occorrenze\n", occorenze);

    for(int i = 0; i < ndir; i++){
        if(pthread_join(td[i].tid, NULL) != 0){
            perror("pthread_join");
            exit(EXIT_FAILURE);
        }
    }

    if(pthread_join(sr.tid, NULL) != 0){
        perror("pthread_join");
        exit(EXIT_FAILURE);
    }

    free(td);
    destroy_sharedds(sd);
    destroy_sharedms(ms);
}