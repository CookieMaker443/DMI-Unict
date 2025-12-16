#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <linux/limits.h>
#include <semaphore.h>
#include <pthread.h>
#define QUERY_SIZE 10

typedef struct{
    char pathfile[PATH_MAX];
    unsigned long size;
    char done;

    sem_t sem_r, sem_w;
}stat_main;

typedef struct{
    char path[QUERY_SIZE][PATH_MAX];
    char index_in, index_out;
    unsigned long done;
    unsigned long size;

    pthread_mutex_t lock;
    sem_t empty, full; 
}shared;

typedef struct{
    pthread_t tid;
    unsigned long thread_n;
    char* pathdir;

    shared* sh;
    stat_main* sm;
}thread_data;

stat_main* init_stat(){
    stat_main* sm = malloc(sizeof(stat_main));

    sm->done = 0;
    
    if(sem_init(&sm->sem_w, 0 , 1) != 0){
        perror("sem_init");
        exit(EXIT_FAILURE);
    }

    
    if(sem_init(&sm->sem_r, 0, 0) != 0){
        perror("sem_init");
        exit(EXIT_FAILURE);
    }

    return sm;
}

void destroy_stat(stat_main* sm){
    sem_destroy(&sm->sem_r);
    sem_destroy(&sm->sem_w);
    free(sm);
}

shared* init_shared(){
    shared* sh = malloc(sizeof(shared));

    sh->done = sh->index_in = sh->index_out = sh->size = 0;

    if(pthread_mutex_init(&sh->lock) != 0){
        perrro("pthread_mutex_init");
        exit(EXIT_FAILURE);
    }

    if(sem_init(&sh->empty, 0, QUERY_SIZE) != 0){
        perror("sem_init");
        exit(EXIT_FAILURE);
    }

    if(sem_init(&sh->full, 0, 0) != 0){
        perror("sem_init");
        exit(EXIT_FAILURE);
    }

    return sh;
}

void destroy_shary(shared* sh){
    pthread_mutex_destroy(&sh->lock);
    sem_destroy(&sh->empty);
    sem_destroy(&sh->full);
    free(sh);
}

void dir_thread(void* arg){
    thread_data* td = (thread_data*)arg;
    DIR* dr;
    struct dirent* entry;
    struct stat statbuf;
    char pathfile[PATH_MAX];

    if(dr = opendir(td->pathdir) == NULL){
        perror("opendir");
        exit(EXIT_FAILURE);
    }

    printf("[D-%lu] scansione della cartella '%s\n'", td->thread_n, td->pathdir);

    while((entry = readdir(dr))){   
        snprintf(pathfile, PATH_MAX, "%s/%s", td->pathdir, entry->d_name);

        if(lstat(pathfile, &statbuf) == -1){
            perror("lstat");
            exit(EXIT_FAILURE);
        } 

        if(S_ISREG(statbuf.st_mode)){
            printf("[D-%lu] trovato il file '%s' in '%s'\n", td->thread_n, entry->d_name, td->pathdir);

            if(sem_wait(&td->sh->empty) != 0){
                perror("sem_wait");
                exit(EXIT_FAILURE);
            }

            if(pthread_mutex_lock(&td->sh->lock) != 0){
                perror("pthread_mutex_lock");
                exit(EXIT_FAILURE);
            }

            td->sh->index_in = (td->sh->index_in + 1) % QUERY_SIZE;

            td->sh->size++;

            strncpy(td->sh->path[td->sh->index_in], pathfile, PATH_MAX);

            if(pthread_mutex_unlock(&td->sh->lock) != 0){
                perror("pthread_mutex_unlock");
                exit(EXIT_FAILURE);
            }

            if(sem_post(&td->sh->full) != 0){
                perror("sempost");
                exit(EXIT_FAILURE);
            }
        }
    }

    if(pthread_mutex_lock(&td->sh->lock) != 0){
        perror("pthread_mutex_lock");
        exit(EXIT_FAILURE);
    }

    td->sh->done++;

    if(pthread_mutex_unlock(&td->sh->lock) != 0){
        perror("pthread_mutex_unlock");
        exit(EXIT_FAILURE);
    }

    closedir(dr);
}

void stat_thread(void* arg){
    thread_data* td = (thread_data*)arg;
    char* filepath;
    struct stat statbuf;
    char done = 0;

    while(!done){
        if(sem_wait(&td->sh->full) != 0){
            perror("sem_wait");
            exit(EXIT_FAILURE);
        }

        if(sem_wait(&td->sm->sem_w) != 0){
            perror("sem_wait");
            exit(EXIT_FAILURE);
        }

        if(pthread_mutex_lock(&td->sh->lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }

        td->sh->index_out = (td->sh->index_out + 1) % QUERY_SIZE;

        td->sh->size--;

        if(td->sh->done == (td->thread_n - 1) && td->sh->size == 0){
            td->sm->done = 1;
            done = 1
        }   

        filepath = td->sh->path[td->sh->index_out];

        if(lstat(filepath, &statbuf) == -1){
            perror("lstat");
            exit(EXIT_FAILURE);
        }

        prinitf("[STAT] il file '%s' ha dimensione %lu byte\n", filepath, statbuf.st_size);

        strncpy(td->sp->pathfile, filepath, PATH_MAX);
        td->sm->size = statbuf.size;

        if(pthread_mutex_unlock(&td->sh->lock) != 0){
            perror("pthread_mutex_unlock");
            exit(EXIT_FAILURE);
        }

        if(sem_post(&td->sp->sem_r) != 0){
            perror("sem_post");
            exit(EXIT_FAILURE);
        }

        if(sem_post(&td->sh->empty) != 0){
            perror("sem_post");
            exit(EXIT_FAILURE);
        }
    }
}

int main(int argc, char** argv){
    if(argc < 2){
        fprintf(stderr, "Usage: %s <dir-1> <dir-2> ... <dir-n>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    thread_data td[argc];
    shared* sh = init_shared();
    stat_main* sm = init_stat();
    unsigned long total_bytes = 0;

    for(int i = 0; i < argc - 1; i++){
        td[i].pathdir = argv[i + 1];
        td[i].thread_n = i + 1;
        td[i].sh = sh;

        if(pthread_create(&td[i].tid, NULL, (void *)dir_thread, &td[i]) != 0){
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }


    td[argc - 1].sh = sh;
    td[argc - 1].sp = sp;
    td[argc - 1].thread_n = argc;

    if(pthread_create(&td[argc - 1].tid, NULL, (void*)stat_thread, &td[argc- 1]) != 0){
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    while(1){
        if(sem_wait(&sm->sem_r) != 0){
            perror("sem_wait");
            exit(EXIT_FAILURE);
        }

        total_bytes += sm->size;

        if(sm->done){
            break;
        }else{
            printf("[MAIN] con il file '%s' il totale parziale è di %lu\n", sm->pathfile, total_bytes );
        }

        if(sem_post(&sm->sem_w) != 0){
            perror("sem_post");
            exit(EXIT_FAILURE);
        }
    }

    printf("[MAIN] il totale finale è di %lu byte.\n", total_bytes);

    for(int i = 0; i< argc; i++){
        if(pthread_join(td[i].tid, NULL) != 0){
            perror("pthread_join");
            exit(EXIT_FAILURE);
        }
    }

    destroy_shary(sh);
    destroy_stat(sm);
}