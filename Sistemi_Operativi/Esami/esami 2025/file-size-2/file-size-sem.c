#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdbool.h>
#include <semaphore.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

typedef struct{
    unsigned long* data_bytes;
    int size;
    int capacity;
    unsigned ndone;
    bool done;
    unsigned long total_bytes;

    pthread_mutex_t lock;
    sem_t read, write;
}number_set;

typedef struct{
    char* pathfile;
    pthread_t tid;
    unsigned thread_n;
    int ndir;
    number_set* sh;
}thread_dir;

typedef struct{
    pthread_t tid;
    unsigned thread_n;
    number_set* sh;
}thread_add;

number_set* init_number_set(){
    number_set* sh = malloc(sizeof(number_set));

    sh->ndone = sh->size = sh->total_bytes = 0;
    sh->done = false;
    sh->capacity = 10;
    sh->data_bytes = malloc(sizeof(unsigned long) * sh->capacity);

    for(int i = 0; i < sh->capacity; i++){
        sh->data_bytes[i] = 0;
    }

    if(pthread_mutex_init(&sh->lock, NULL) != 0){
        perror("pthread_mutex_lock");
        exit(EXIT_FAILURE);
    }

    if(sem_init(&sh->read, 0, 1) != 0){
        perror("sem_init");
        exit(EXIT_FAILURE);
    }

    if(sem_init(&sh->write, 0, 0) != 0){
        perror("sem_init");
        exit(EXIT_FAILURE);
    }
    
    return sh;
}

void destroy_number_set(number_set* sh){
    pthread_mutex_destroy(&sh->lock);
    sem_destroy(&sh->read);
    sem_destroy(&sh->write);
    free(sh->data_bytes);
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

    printf("[DIR-%u] scansione della cartella '%s'\n", td->thread_n +1, td->pathfile);

    while((entry = readdir(dr))){
        snprintf(path, PATH_MAX, "%s/%s", td->pathfile, entry->d_name);

        if(lstat(path, &statbuf) == -1){
            perror("lstat");
            exit(EXIT_FAILURE);
        }

        if(S_ISREG(statbuf.st_mode)){
            if(sem_wait(&td->sh->read) != 0){
                perror("sem_wait");
                exit(EXIT_FAILURE);
            }

            if(pthread_mutex_lock(&td->sh->lock) != 0){
                perror("pthread_mutex_lock");
                exit(EXIT_FAILURE);
            }

            if(td->sh->size == td->sh->capacity){
                td->sh->capacity *= 2;
                unsigned long* temp = realloc(td->sh->data_bytes, td->sh->capacity * sizeof(unsigned long));
                td->sh->data_bytes = temp;
            }

            td->sh->data_bytes[td->sh->size] = statbuf.st_size;
            td->sh->size++;
            printf("[DIR-%u] trovato il file '%s' di %lu byte; l'insieme ha adesso %d elementi\n", td->thread_n + 1, path, statbuf.st_size, td->sh->size);

            if(pthread_mutex_unlock(&td->sh->lock) != 0){
                perror("pthread_mutex_unlock");
                exit(EXIT_FAILURE);
            }

            if(sem_post(&td->sh->write) != 0){
                perror("sem_post");
                exit(EXIT_FAILURE);
            }
        }
    }
    closedir(dr);

    if(pthread_mutex_lock(&td->sh->lock) != 0){
        perror("pthread_mutex_lock");
        exit(EXIT_FAILURE);
    }

    td->sh->ndone++;

    if(td->sh->ndone == td->ndir){
        td->sh->done = true;
    }

    if(pthread_mutex_unlock(&td->sh->lock) != 0){
        perror("pthread_mutex_unlock");
        exit(EXIT_FAILURE);
    }

    if(sem_post(&td->sh->write) != 0){
        perror("sem_post");
        exit(EXIT_FAILURE);
    }
}

void add_function(void* arg){
    thread_add* td = (thread_add*)arg;
    
    while(1){
        if(sem_wait(&td->sh->write) != 0){
            perror("sem_wait");
            exit(EXIT_FAILURE);
        }

        if(pthread_mutex_lock(&td->sh->lock) != 0){
            perror("Pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }

        if(td->sh->done && td->sh->size < 2){
            if(pthread_mutex_unlock(&td->sh->lock) != 0){
                perror("Pthread_mutex_unlock");
                exit(EXIT_FAILURE);
            }
            break;
        }

        if(td->sh->size >= 2){
            int minIndex = 0, maxIndex = 0;
            for(int i = 0; i < td->sh->size; i++){
                if(td->sh->data_bytes[i] < td->sh->data_bytes[minIndex]){
                    minIndex = i;
                }
                if(td->sh->data_bytes[i] > td->sh->data_bytes[maxIndex]){
                    maxIndex = i;
                }
            }

            unsigned long min = td->sh->data_bytes[minIndex];
            unsigned long max = td->sh->data_bytes[maxIndex];
            unsigned long sum = max + min;
            td->sh->total_bytes += sum;

            if(minIndex > maxIndex){
                for(int i = minIndex; i < td->sh->size-1; i++){
                    td->sh->data_bytes[i] = td->sh->data_bytes[i + 1];
                }
                td->sh->size--;

                for(int i = maxIndex; i < td->sh->size - 1; i++){
                    td->sh->data_bytes[i] = td->sh->data_bytes[i + 1];
                }
                td->sh->size--;
            }else{
                for(int i = maxIndex; i < td->sh->size-1; i++){
                    td->sh->data_bytes[i] = td->sh->data_bytes[i + 1];
                }
                td->sh->size--;

                for(int i = minIndex; i < td->sh->size - 1; i++){
                    td->sh->data_bytes[i] = td->sh->data_bytes[i + 1];
                }
                td->sh->size--;
            }

            if(td->sh->size == td->sh->capacity){
                td->sh->capacity *= 2;
                unsigned long* temp = realloc(td->sh->data_bytes, sizeof(unsigned long) * td->sh->capacity);
                td->sh->data_bytes = temp;
            }

            td->sh->data_bytes[td->sh->size] = sum;
            td->sh->size++;

            printf("[ADD-%u] il minimo (%lu) e il massimo (%lu) sono stati sostituiti da %lu; l'insieme ha adesso %d elementi\n", td->thread_n + 1, min, max, sum, td->sh->size);
        
        }

        if(pthread_mutex_unlock(&td->sh->lock) != 0){
            perror("pthread_mutex_unlock");
            exit(EXIT_FAILURE);
        }

        if(sem_post(&td->sh->read) != 0){
            perror("sem_post");
            exit(EXIT_FAILURE);
        }
    }
}


int main(int argc, char** argv){
    if(argc < 3){
        fprintf(stderr, "Usage: %s <dir-1> <dir-2> ... <dir-n>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int ndir = argc - 1;
    number_set* sh = init_number_set();

    thread_dir* dr = malloc(sizeof(thread_dir) * ndir);
    for(int i = 0; i < ndir; i++){
        dr[i].ndir = ndir;
        dr[i].sh = sh;
        dr[i].thread_n = i;
        dr[i].pathfile = argv[i + 1];
        if(pthread_create(&dr[i].tid, NULL, (void*)dir_function, &dr[i]) != 0){
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }

    thread_add* ad = malloc(sizeof(thread_add) * 2);
    for(int i = 0; i < 2; i++){
        ad[i].sh = sh;
        ad[i].thread_n = i;
        if(pthread_create(&ad[i].tid, NULL, (void*)add_function, &ad[i]) != 0){
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }
    
    for(int i = 0; i < ndir; i++){
        if(pthread_join(dr[i].tid, NULL) != 0){
            perror("pthread_join");
            exit(EXIT_FAILURE);
        }
    }

    for(int i = 0; i < 2; i++){
        if(pthread_join(ad[i].tid, NULL) != 0){
            perror("pthread_join");
            exit(EXIT_FAILURE);
        }
    }

    printf("[MAIN] i thread secondari hanno terminato e il totale finale è di %lu byte\n", sh->total_bytes);
    free(ad);
    free(dr);
    destroy_number_set(sh);
}