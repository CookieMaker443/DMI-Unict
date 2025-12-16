#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <stdbool.h>

typedef struct{
    unsigned long* data;
    int size;
    int capacity;
    unsigned total_byte;
    unsigned ndone;
    bool done;

    pthread_mutex_t lock;
    pthread_cond_t write;
}number_set;

typedef struct{
    char* pathfile;
    pthread_t tid;
    unsigned thread_n;
    int ndir;
    number_set* sh;
}thread_dir_i;

typedef struct{
    pthread_t tid;
    unsigned thread_n;
    number_set* sh;
}thread_dir_j;


number_set* init_number_set(){
    number_set* sh = malloc(sizeof(number_set));

    sh->capacity = 10;
    sh->data = malloc(sizeof(unsigned long) * sh->capacity);
    for(int i = 0; i < sh->capacity; i++){
        sh->data[i] = 0;
    }

    sh->size = sh->total_byte = sh->ndone = 0;
    sh->done = false;

    if(pthread_mutex_init(&sh->lock, NULL) != 0){
        perror("Pthread_mutex_init");
        exit(EXIT_FAILURE);
    }

    if(pthread_cond_init(&sh->write, NULL) != 0){
        perror("pthread_cond_init");
        exit(EXIT_FAILURE);
    }

    return sh;
}

void number_set_destroy(number_set* sh){
    pthread_cond_destroy(&sh->write);
    pthread_mutex_destroy(&sh->lock);
    free(sh);
}

void diri_function(void* arg){
    thread_dir_i* td = (thread_dir_i*)arg;

    DIR* dr;
    struct stat statbuf;
    struct dirent* entry;
    char path[PATH_MAX];

    if((dr = opendir(td->pathfile)) == NULL){
        perror("opendir");
        exit(EXIT_FAILURE);
    }

    printf("[DIR-%u] scansione della cartellaa '%s'\n" , td->thread_n +1, td->pathfile);

    while((entry = readdir(dr))){
        snprintf(path, PATH_MAX, "%s/%s", td->pathfile, entry->d_name);

        if(lstat(path, &statbuf) == -1){
            perror("lstat");
            exit(EXIT_FAILURE);
        }

        if(S_ISREG(statbuf.st_mode)){
            if(pthread_mutex_lock(&td->sh->lock) != 0){
                perror("pthread_mutex_lock");
                exit(EXIT_FAILURE);
            }

            if(td->sh->size == td->sh->capacity){
                td->sh->capacity *= 2;
                unsigned long* temp = realloc(td->sh->data, td->sh->capacity * sizeof(unsigned long));
                td->sh->data = temp;
            }

            td->sh->data[td->sh->size] = statbuf.st_size;
            td->sh->size++;
            printf("[DIR-%u] trovato il file '%s' di %lu; l'insieme ha %d elementi\n", td->thread_n + 1, path, statbuf.st_size, td->sh->size);

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
        if(pthread_cond_broadcast(&td->sh->write) != 0){
            perror("Pthread_Cond_broadcast");
            exit(EXIT_FAILURE);
        }
    }

    if(pthread_mutex_unlock(&td->sh->lock) != 0){
        perror("pthread_mutex_unlock");
        exit(EXIT_FAILURE);
    }
}


void dirj_function(void* arg){
    thread_dir_j* td = (thread_dir_j*)arg;

    while(1){
        if(pthread_mutex_lock(&td->sh->lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }

        while(td->sh->size < 2 && !td->sh->done){
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

        int minIndex = 0, maxIndex = 0;
        for(int i = 0; i < td->sh->size; i++){
            if(td->sh->data[i] < td->sh->data[minIndex]){
                minIndex = i;
            }
            if(td->sh->data[i] > td->sh->data[maxIndex]){
                maxIndex = i;
            }
        }

        unsigned long min = td->sh->data[minIndex];
        unsigned long max = td->sh->data[maxIndex];
        unsigned long sum = min + max;
        td->sh->total_byte += sum;

        if(minIndex > maxIndex){
            for(int i = minIndex; i < td->sh->size - 1; i++){
                td->sh->data[i] = td->sh->data[i + 1];
            }
            td->sh->size--;

            for(int i = maxIndex; i < td->sh->size - 1; i++){
                td->sh->data[i] = td->sh->data[i + 1];
            }
            td->sh->size--;
        }else{
            for(int i = maxIndex; i < td->sh->size - 1; i++){
                td->sh->data[i] = td->sh->data[i + 1];
            }
            td->sh->size--;

            for(int i = minIndex; i < td->sh->size - 1; i++){
                td->sh->data[i] = td->sh->data[i + 1];
            }
            td->sh->size--;
        }

        if(td->sh->size == td->sh->capacity){
            td->sh->capacity *= 2;
            unsigned long* temp = realloc(td->sh->data, td->sh->capacity * sizeof(unsigned long));
            td->sh->data = temp;
        }
        td->sh->data[td->sh->size] = sum;
        td->sh->size++;

        printf("[DIR-%u] il minimo (%lu) e il massimo (%lu) sono stati sostituiti da %lu; l'insieme ha adesso %d elementi\n", td->thread_n + 1, min, max, sum, td->sh->size);

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
    thread_dir_i* di = malloc(sizeof(thread_dir_i) * ndir);

    for(int i = 0; i < ndir; i++){
        di[i].pathfile = argv[i + 1];
        di[i].sh = sh;
        di[i].thread_n = i;
        if(pthread_create(&di[i].tid, NULL, (void*)diri_function, &di[i]) != 0){
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }

    thread_dir_j* dj = malloc(sizeof(thread_dir_j) * 2);
    for(int i = 0;  i < 2; i++){
        dj[i].sh = sh;
        dj[i].thread_n = i;
        if(pthread_create(&dj[i].tid, NULL, (void*)dirj_function, &dj[i]) != 0){
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
        if(pthread_join(dj[i].tid, NULL) != 0){
            perror("pthread_join");
            exit(EXIT_FAILURE);
        }
    }

    printf("[MAIN] i thread secondari hanno terminato e il totale finale è di %u byte\n", sh->total_byte);
    number_set_destroy(sh);
    free(di);
    free(dj);
}