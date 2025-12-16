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

#define PATH_NAME 10
#define BUFFER_SIZE 10
#define MAX_LENGTH 1024

typedef struct{
    char path[PATH_NAME][PATH_MAX];
    char index_in, index_out;
    unsigned long done;
    unsigned long size;

    pthread_mutex_t lock;
    sem_t full, empty;
}shared_ds;

typedef struct{
    char path[BUFFER_SIZE][PATH_MAX];
    int occorrenze;
    bool done;

    sem_t read_s, write_s;
}shared_ms;

typedef struct{
    pthread_t tid;
    unsigned thread_n;
    char* pathdir;
    char* word;

    shared_ds* ds;
    shared_ms* ms;
}thread_data;

shared_ds* init_shareds(){
    shared_ds* sh = malloc(sizeof(shared_ds));

    sh->index_in = sh->index_out = sh->size = sh->done = 0;

    if(pthread_mutex_init(&sh->lock, NULL) != 0){
        perror("pthread_mutex_init");
        exit(EXIT_FAILURE);
    }

    if(sem_init(&sh->full, 0, 0) != 0){
        perror("sem_init");
        exit(EXIT_FAILURE);
    }

    if(sem_init(&sh->empty, 0, PATH_NAME) != 0){
        perror("sem_init");
        exit(EXIT_FAILURE);
    }

    return sh;
}

void destroy_shareds(shared_ds* sh){
    pthread_mutex_destroy(&sh->lock);
    sem_destroy(&sh->full);
    sem_destroy(&sh->empty);
    free(sh);
}

shared_ms* init_sharedms(){
    shared_ms* sh = malloc(sizeof(shared_ms));

    sh->done = 0;

    if(sem_init(&sh->read_s, 0, 0) != 0){
        perror("sem_init");
        exit(EXIT_FAILURE);
    }

    if(sem_init(&sh->write_s, 0, 1) != 0){
        perror("sem_init");
        exit(EXIT_FAILURE);
    }

    return sh;
}

void destroy_sharedms(shared_ms* sh){
    sem_destroy(&sh->read_s);
    sem_destroy(&sh->write_s);
    free(sh);
}

void dir_thread(void* arg){
    thread_data* td = (thread_data*)arg;

    DIR* dr;
    char buffer[PATH_MAX];
    struct stat statbuf;
    struct dirent* entry;

    if((dr = opendir(td->pathdir)) == NULL){
        perror("opendir");
        exit(EXIT_FAILURE);
    }

    while((entry = readdir(dr))){
        snprintf(buffer, PATH_MAX, "%s/%s", td->pathdir, entry->d_name);

        if(lstat(buffer, &statbuf) == -1){
            perror("lstat");
            exit(EXIT_FAILURE);
        }

        if(S_ISREG(statbuf.st_mode)){

            if(sem_wait(&td->ds->empty) != 0){
                perror("sem_wait");
                exit(EXIT_FAILURE);
            }

            if(pthread_mutex_lock(&td->ds->lock) != 0){
                perror("pthread_mutex_lock");
                exit(EXIT_FAILURE);
            }

            td->ds->index_in = (td->ds->index_in + 1) % PATH_NAME;
            td->ds->size++;

            strncpy(td->ds->path[td->ds->index_in], buffer, PATH_MAX);

            if(pthread_mutex_unlock(&td->ds->lock) != 0){
                perror("pthread_mutex_unlock");
                exit(EXIT_FAILURE);
            }

            if(sem_post(&td->ds->full) != 0){
                perror("sem_post");
                exit(EXIT_FAILURE);
            }
        }
    }

    if(pthread_mutex_lock(&td->ds->lock) != 0){
        perror("pthread_mutex_lock");
        exit(EXIT_FAILURE);
    }

    td->ds->done = 1;

    if(pthread_mutex_unlock(&td->ds->lock) != 0){
        perror("pthread_mutex_unlock");
        exit(EXIT_FAILURE);
    }
    closedir(dr);
}

void search_thread(void* arg){
    thread_data* td = (thread_data*)arg;
    char* pathfile;
    bool done = 0;
    FILE* f;
    int counter;
    char line[MAX_LENGTH];

    while(!done){
        if(sem_wait(&td->ds->full) != 0){
            perror("sem_wait");
            exit(EXIT_FAILURE);
        }
        pthread_mutex_destroy(&sh->lock);
        if(sem_wait(&td->ms->write_s) != 0){
            perror("sem_wait");
            exit(EXIT_FAILURE);
        }

        if(pthread_mutex_lock(&td->ds->lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }

        td->ds->index_out = (td->ds->index_out - 1) % PATH_NAME;
        td->ds->size--;

        if(td->ds->done == (td->thread_n - 1) && td->ds->size == 0){
            td->ms->done = 1;
            done = 1;
        }

        pathfile = td->ds->path[td->ds->index_out];

        if((f = fopen(pathfile, "r")) == NULL){
            perror("fopen");
            exit(EXIT_FAILURE);
        }
        
        counter = 0;
        while(fgets(pathfile, MAX_LENGTH, f)){
            char* p = line;
            while((p = strcasecmp(p, td->word)) != NULL){
                counter++;
                p += strlen(td->word);
            }
        }
        fclose(f);

        if(counter > 0){
            strncpy(td->ms->path, pathfile, PATH_MAX);
            td->ms->occorrenze = counter;
        }else{
            printf("[SEARCH] il file '%s' non contiene occorrenze\n", pathfile);
        }

        if(pthread_mutex_unlock(&td->ds->lock) != 0){
            perror("pthread_mutex_unlock");
            exit(EXIT_FAILURE);
        }

        if(sem_post(&td->ms->read_s) != 0){
            perror("sem_post");
            exit(EXIT_FAILURE);
        }

        if(sem_post(&td->ds->empty) != 0){
            perror("sem_post");
            exit(EXIT_FAILURE);
        }
    }
}

int main(int argc, char** argv){
    if(argc < 3){
        fprintf(stderr, "Usage: %s <word> <dir-1> <dir-2> ... <dir-n>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char* word = argv[1];
    int num_dir = argc - 2;
    shared_ds* ds = init_shareds();
    shared_ms* ms  = init_sharedms();
    int occorrenze = 0;
    thread_data* dir_td = malloc(sizeof(thread_data) * num_dir);
    for(unsigned i = 0; i < argc - 1; i++){
        dir_td[i].ds = ds;
        dir_td[i].pathdir = argv[i + 2];
        dir_td[i].thread_n = i + 1;

        if(pthread_create(&dir_td[i].tid, NULL, (void*)dir_thread, &dir_td[i]) != 0){
            perror("pthread_Create");
            exit(EXIT_FAILURE);
        }
    }

    thread_data search_td;
    search_td.word = word;
    search_td.ms = ms;

    if(pthread_create(&search_td.tid, NULL, (void*)search_thread, &search_td) != 0){
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }


    while(1){
        if(sem_wait(&ms->read_s) != 0){
            perror("sem_wait");
            exit(EXIT_FAILURE);
        }

        occorrenze += ms->occorrenze;

        if(ms->done){
            break;
        }else{
            printf("[MAIN] con il file %s il totale parziale è di %d occorenze\n", ms->path, occorrenze);
        }

        if(sem_post(&ms->write_s) != 0){
            perror("sem_post");
            exit(EXIT_FAILURE);
        }
    }

    printf("[MAIN] il totale finale è di %d occorrenze\n", occorrenze);

    for(unsigned i = 0; i < num_dir; i++){
        if(pthread_join(dir_td[i].tid, NULL) != 0){
            perror("pthread_join");
            exit(EXIT_FAILURE);
        }
    }

    if(pthread_join(search_td.tid, NULL) != 0){
        perror("pthread_join");
        exit(EXIT_FAILURE);
    }

    destroy_sharedms(ms);
    destroy_shareds(ds);
    free(dir_td);
}