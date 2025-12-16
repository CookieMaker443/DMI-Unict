#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <semaphore.h>
#include <dirent.h>
#include <sys/stat.h>


#define PATH_CAPACITY 10

typedef struct{
    char pathname[PATH_CAPACITY][PATH_MAX];
    int index_in, index_out;
    int size;
    unsigned ndone;
    bool done;

    pthread_mutex_t lock;
    sem_t full, empty;
}shared_sd;

typedef struct{
    char path[PAHT_MAX];
    unsigned long size;
    
    sem_t read, write;
}shared_ms;

typedef struct{
    pthread_t tid;
    unsigned thread_n;
    int ndir;
    char* pathfile;
    shared_sd* sh;
}thread_dir;

typedef struct{
    pthread_t tid;
    shared_sd* sd;
    shared_ms* sm;
}thread_stat;

shared_sd* init_sharedsd(){
    shared_sd* sh = malloc(sizeof(shared_sd));

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

    if(sem_init(&sh->empty, 0, PATH_CAPACITY) != 0){
        perror("sem_init");
        exit(EXIT_FAILURE);
    }

    return sh;
}

shared_ms* init_sharedms(){
    shared_ms* sh = malloc(sizeof(shared_ms));

    sh->size = 0;

    if(sem_init(&sh->read, 0, 0) != 0){
        perror("sem_init");
        exit(EXIT_FAILURE);
    }

    if(sem_init(&sh->write, 0, 1) != 0){
        perror("sem_init");
        exit(EXIT_FAILURE);
    }

    return sh;
}

void destroy_sd(shared_sd* sh){
    pthread_mutex_destroy(&sh->lock);
    sem_destroy(&sh->full);
    sem_destroy(&sh->empty);
    free(sh);
}

void destroy_ms(shared_ms* sh){
    sem_destroy(&sh->read);
    sem_destroy(&sh->write);
    free(sh);
}

void dir_function(void* arg){
    thread_dir* td = (thread_dir*)arg;

    DIR* dr;
    struct stat statbuf;
    struct dirent* entry;
    char path[PATH_MAX];

    if((dr = opendir(td->pathfile)) == NULL){
        perror("opendir");
        exit(EXIT_FAILURE);
    }

    printf("[D-%u] scansione della cartella '%s'\n", td->thread_n + 1, td->pathfile);

    while((entry = readdir(dr))){
        snprintf(path, PATH_MAX, "%s/%s", td->pathfile, entry->d_name);
    
        if(lstat(path, &statbuf) == -1){
            perror("lstat");
            exit(EXIT_FAILURE);
        }

        if(S_ISREG(statbuf.st_mode)){
            printf("[D-%u] trovato il file '%s' in '%s'\n", td->thread_n +1, entry->d_name, td->pathfile);

            if(sem_wait(&td->sh->empty) != 0){
                perror("sem_wait");
                exit(EXIT_FAILURE);
            }

            if(pthread_mutex_lock(&td->sh->lock) != 0){
                perror("pthread_mutex_lock");
                exit(EXIT_FAILURE);
            }

            td->sh->index_in = (td->sh->index_in + 1) % PATH_CAPACITY;
            td->sh->size++;
            
            strncpy(td->sh->pathname[td->sh->index_in], path, PATH_MAX);

            if(pthread_mutex_unlock(&td->sh->lock) != 0){
                perror("pthread_mutex_unlock");
                exit(EXIT_FAILURE);
            }

            if(sem_post(&td->sh->full) != 0){
                perror("sem_post");
                exit(EXIT_FAILURE);
            }
        }
    }

    if(pthread_mutex_lock(&td->sh->lock) != 0){
        perror("pthread_mutex_lock");
        exit(EXIT_FAILURE);
    }

    td->sh->ndone++;

    if(td->sh->ndone == td->ndir && td->sh->size == 0){
        td->sh->done = true;
    }

    if(pthread_mutex_unlock(&td->sh->lock) != 0){
        perror("pthread_mutex_unlock");
        exit(EXIT_FAILURE);
    }
}

void stat_function(void* arg){
    thread_stat* td = (thread_stat*)arg;
    struct stat statbuf;
    struct dirent* entry;
    char* filepath;

    while(1){
        if(sem_wait(&td->sd->full) != 0){
            perror("sem_wait");
            exit(EXIT_FAILURE);
        }

        if(sem_wait(&td->sm->write) != 0){
            perror("sem_wait");
            exit(EXIT_FAILURE);
        }

        if(pthread_mutex_lock(&td->sd->lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }

        td->sd->index_out = (td->sd->index_out + 1) % PATH_CAPACITY;
        td->sd->size--;

        if(td->sd->done){
            break;
        }

        filepath = td->sd->pathname[td->sd->index_out];

        if(lstat(filepath, &statbuf) == -1){
            perror("lstat");
            exit(EXIT_FAILURE);
        }

        printf("[STAT] il file '%s' ha dimensione %lu byte\n", filepath, statbuf.st_size);

        strncpy(td->sm->path, filepath, PATH_MAX);
        td->sm->size = statbuf.st_size;

        if(pthread_mutex_unlock(&td->sd->lock) != 0){
            perror("pthread_mutex_unlock");
            exit(EXIT_FAILURE);
        }

        if(sem_post(&td->sd->empty) != 0){
            perror("sem_post");
            exit(EXIT_FAILURE);
        }

        if(sem_post(&td->sm->read) != 0){
            perror("sem_post");
            exit(EXIT_FAILURE);
        }
    }
}

int main(int argc, char** argv){
    if(argc < 3){
        fprintf(stderr, "Usage: %s <dir-1> <dir-2> <..> <dir-n>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int ndir = argc - 1;
    shared_sd* sd = init_sharedsd();
    shared_ms* ms = init_sharedms();
    unsigned long total_bytes = 0;

    thread_dir* dr = malloc(sizeof(thread_dir) * ndir);

    for(int i = 0; i < ndir; i++){
        dr[i].ndir = ndir;
        dr[i].pathfile = argv[i + 1];
        dr[i].thread_n = i;
        dr[i].sh = sd;
        if(pthread_create(&dr[i].tid, NULL, (void*)dir_function, &dr[i]) != 0){
            perror("pthread_Create");
            exit(EXIT_FAILURE);
        }
    }

    thread_stat st;
    st.sd = sd;
    st.sm = ms;
    if(pthread_create(&st.tid, NULL, (void*)stat_function, &st) != 0){
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }    

    while(1){
        if(sem_wait(&ms->read) != 0){
            perror("sem_wait");
            exit(EXIT_FAILURE);
        }

        total_bytes += ms->size;

        if(ms->done){
            break;
        }else{
            printf("[MAIN] con il file '%s' il totale parziale è di %lu byte\n", ms->path, total_bytes);
        }

        if(sem_post(&ms->write) != 0){
            perror("sem_post");
            exit(EXIT_FAILURE);
        }
    }

    printf("[MAIN] il totale finale è di %lu bytes\n", total_bytes);

    for(int i = 0; i <ndir; i++){
        if(pthread_join(dr[i].tid, NULL) != 0){
            perror("pthread_join");
            exit(EXIT_FAILURE);
        }
    }

    if(pthread_join(st.tid, NULL) != 0){
        perror("pthread_join");
        exit(EXIT_FAILURE);
    }

    destroy_ms(ms);
    destroy_sd(sd);
    free(dr);
}