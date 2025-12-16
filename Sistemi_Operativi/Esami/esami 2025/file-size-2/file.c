#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <dirent.h>


typedef struct{
    unsigned long* data;
    int size;
    int capacity;
    unsigned long total_bytes;
    unsigned ndone;
    bool done;

    pthread_mutex_t lock;
    pthread_cond_t write;
}number_set;

typedef struct{
    char* directory;
    pthread_t tid;
    unsigned thread_n;
    number_set* sh;
    unsigned ndir;
}thread_dir;

typedef struct{
    pthread_t tid;
    unsigned thread_n;
    number_set* sh;
}thread_add;

number_set* init_number_set(){
    number_set* sh = malloc(sizeof(number_set));

    sh->capacity = 10;
    sh->size = sh->ndone = sh->total_bytes = 0;
    sh->done = false;
    sh->data = malloc(sizeof(unsigned long) * sh->capacity);
    for(int i = 0; i < sh->capacity; i++){
        sh->data[i] = 0;
    }

    if(pthread_mutex_init(&sh->lock, NULL) != 0){
        perror("pthread_mutex_init");
        exit(EXIT_FAILURE);
    }

    if(pthread_cond_init(&sh->write, NULL) != 0){
        perror("pthread_Cond_inti");
        exit(EXIT_FAILURE);
    }

    return sh;
}

void dir_function(void* arg){
    thread_dir* td = (thread_dir*)arg;

    DIR* dr;
    char path[PATH_MAX];
    struct stat statbuf;
    struct dirent* entry;

    if((dr = opendir(td->directory)) == NULL){
        perror("opendir");
        exit(EXIT_FAILURE);
    }

    printf("[DIR-%u] scansione della cartella '%s'\n", td->thread_n + 1);

    while((entry = readdir(dr))){
        snprintf(path, PATH_MAX, "%s/%s", td->directory, entry->d_name);

        if(lstat(path, &statbuf) == -1){
            perror("lstat");
            exit(EXIT_FAILURE);
        }

        if(S_ISREG(statbuf.st_mode)){
            if(pthread_mutex_lock(&td->sh->lock) != 0){
                perror("pthread_mutex_lock");
                exit(EXIT_FAILURE);
            }

            if(td->sh->capacity == td->sh->size){
                td->sh->capacity *= 2;
                unsigned long* temp = realloc(td->sh->data, sizeof(unsigned long) * td->sh->capacity);
                td->sh->data = temp;
            }

            td->sh->data[td->sh->size] = statbuf.st_size;
            td->sh->size++;
            printf("[DIR-%u] trovato il file '%s' di %llu byte; l'insieme ha adesso %d elementi\n", td->thread_n + 1, entry->d_name, statbuf.st_size, td->sh->size);

            if(pthread_cond_signal(&td->sh->write) != 0){
                perror("pthread_cond_signal");
                exit(EXIT_FAILURE);
            }

            if(pthread_mutex_unlock(&td->sh->lock) != 0){
                perror("pthread_mutex_unlock");
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
        if(pthread_cond_signal(&td->sh->write) != 0){
            perror("ptrhead_cond_signal");
            exit(EXIT_FAILURE);
        }
    }

    if(pthread_mutex_unlock(&td->sh->lock) != 0){
        perror("pthread_mutex_unlock");
        exit(EXIT_FAILURE);
    }
}

void add_function(void* arg){
    thread_dir* td = (thread_dir*)arg;

    while(1){
        if(pthread_mutex_lock(&td->sh->lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }

        while(!td->sh->done && td->sh->size < 2){
            if(pthread_cond_wait(&td->sh->write, &td->sh->lock) != 0){
                perror("pthread_cond_wait");
                exit(EXIT_FAILURE);
            }
        }

        if(td->sh->done && td->sh->size < 2){
            if(pthread_mutex_unlock(&td->sh->lock) != 0){
                perror("pthread_mutex_unlock");
                exit(EXIT_FAILURE);
            }
            break;
        }

        int minIndex = 0;
        int maxIndex = 0;

        for(int i = 0; i < td->sh->size; i++){
            if(td->sh->data[minIndex] > td->sh->data[i]){
                minIndex = i;
            }
            if(td->sh->data[i] > td->sh->data[maxIndex]){
                maxIndex = i;
            }
        }

        unsigned long min = td->sh->data[minIndex];
        unsigned long max = td->sh->data[maxIndex];
        unsigned long sum = min + max;
        td->sh->total_bytes += sum;

        if(minIndex > maxIndex){
            for(int i = minIndex; i < td->sh->size - 1; i++){
                td->sh->data[i] = td->sh->data[i + 1];
            }
            td->sh->size --;

            for(int i = maxIndex; i < td->sh->size - 1; i++){
                td->sh->data[i] = td->sh->data[i + 1];
            }
            td->sh->size --;
        }else{
            for(int i = maxIndex; i < td->sh->size - 1; i++){
                td->sh->data[i] = td->sh->data[i + 1];
            }
            td->sh->size --;

            for(int i = minIndex; i < td->sh->size - 1; i++){
                td->sh->data[i] = td->sh->data[i + 1];
            }
            td->sh->size --;
        }

        if(td->sh->size == td->sh->capacity){
            td->sh->capacity *= 2;
            unsigned long* temp = realloc(td->sh->data, sizeof(unsigned long) * td->sh->capacity);
            td->sh->data = temp;
        }

        td->sh->data[td->sh->size] = sum;
        td->sh->size++;

        printf("[ADD-%u] il minimo (%llu) e il massimo (%llu) sono stati sostituiti da %lld; l'insieme ha adesso %d elementi\n", td->thread_n + 1,  min, max, sum, td->sh->size);

        if(pthread_mutex_unlock(&td->sh->lock) != 0){
            perror("pthread_mutex_unlock");
            exit(EXIT_FAILURE);
        }
    }
}

int main(int argc, char** argv){
    if(argc < 3){
        fprintf(stderr, "Usage: %s <dir-1> <dir-2> <...> <dir-n>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int ndir = argc - 1;
    number_set* sh = init_number_set();
    thread_dir* di = malloc(sizeof(thread_dir) * ndir);

    for(int i = 0; i < ndir; i++){
        di[i].pathfile = argv[i + 1];
        di[i].sh = sh;
        di[i].thread_n = i;
        if(pthread_create(&di[i].tid, NULL, (void*)dir_function, &di[i]) != 0){
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }

    thread_add* add = malloc(sizeof(thread_add) * 2);
    for(int i = 0;  i < 2; i++){
        add[i].sh = sh;
        add[i].thread_n = i;
        if(pthread_create(&add[i].tid, NULL, (void*)add_function, &add[i]) != 0){
            perror("Pthread_create");
            exit(EXIT_FAILURE);
        }
    }

    for(int i = 0; i < ndir; i++){
        if(pthread_join(di[i].tid, NULL) != 0){
            perror("Pthread_join");
            exit(EXIT_FAILURE);
        } 
    }

    for(int i = 0; i < 2; i++){
        if(pthread_join(add[i].tid, NULL) != 0){
            perror("pthread_join");
            exit(EXIT_FAILURE);
        }
    }

    printf("[MAIN] i thread secondari hanno terminato e il totale finale è di %u byte\n", sh->total_byte);
    number_set_destroy(sh);
    free(di);
    free(add);
}