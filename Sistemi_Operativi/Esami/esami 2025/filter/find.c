#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>

#define PATH_SIZE 10
#define BUFFER_SIZE 10


typedef struct{
    char path[PATH_SIZE][PATH_MAX];
    int index_in, index_out;
    unsigned long size;
    unsigned long done;

    pthread_mutex_t lock;
    sem_t full, empty;
}shared_ds;

typedef struct{
    char buffer[BUFFER_SIZE][PATH_MAX];
    int occorrenze[BUFFER_SIZE];
    int index_in, index_out;
    unsigned long size;
    bool done;

    pthread_mutex_t lock;
    sem_t full, empty;
}shared_sm;


typedef struct{
    pthread_t tid;
    unsigned thread_n;
    char* dirname;

    shared_ds* ds;
}dir_thread_data;

typedef struct{
    pthread_t tid;
    unsigned dir_n;
    char* word;
    shared_ds* ds;
    shared_sm* sm;
}sm_thread_data;

shared_ds* init_sharedds(){
    shared_ds* sh = malloc(sizeof(shared_ds));

    sh->size = sh->index_in = sh->index_out = sh->done = 0;

    if(pthread_mutex_init(&sh->lock, NULL) != 0){
        perror("pthread_mutex_init");
        exit(EXIT_FAILURE);
    }

    if(sem_init(&sh->full, 0, 0) != 0){
        perror("sem_init");
        exit(EXIT_FAILURE);
    }

    if(sem_init(&sh->empty, 0, PATH_SIZE) != 0){
        perror("sem_init");
        exit(EXIT_FAILURE);
    }

    return sh;
}

shared_sm* init_sharedsm(){
    shared_sm* sh = malloc(sizeof(shared_sm));

    sh->size = sh->done = sh->index_in = sh->index_out = 0;
    
    if(pthread_mutex_init(&sh->lock, NULL) != 0){
        perror("pthread_mutex_init");
        exit(EXIT_FAILURE);
    }

    if(sem_init(&sh->empty, 0 , BUFFER_SIZE) != 0){
        perror("sem_init");
        exit(EXIT_FAILURE);
    }

    if(sem_init(&sh->full, 0, 0) != 0){
        perror("sem_init");
        exit(EXIT_FAILURE);
    }

    return sh;
}

void shared_destroy_ds(shared_ds* sh){
    pthread_mutex_destroy(&sh->lock);
    sem_destroy(&sh->full);
    sem_destroy(&sh->empty);
    free(sh);
}

void shared_destroy_sm(shared_sm* sh){
    pthread_mutex_destroy(&sh->lock);
    sem_destroy(&sh->full);
    sem_destroy(&sh->empty);
    free(sh);
}

void dir_function(void* arg){
    dir_thread_data* td = (dir_thread_data*)arg;

    DIR* dr;
    char pathfile[PATH_MAX];
    struct stat stat_buf;
    struct dirent* entry; //costruisce il percorso completo di tutti gli elementi presenti nella directory 

    if((dr = opendir(td->dirname)) == NULL){
        perror("opendir");
        exit(EXIT_FAILURE);
    }

    printf("[DIR-%u] scansione della cartella '%s'\n", td->thread_n + 1, td->dirname);

    while((entry = readdir(dr))){
        if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0){
            continue;
        }

        snprintf(pathfile, PATH_MAX, "%s/%s", td->dirname, entry->d_name);
        //formatta una stringa che rappresenta il percorso completo del file, combinando la directory e il nome del file, separandoli con una barra.
        
        if(lstat(pathfile, &stat_buf) == -1){
            perror("lstat");
            exit(EXIT_FAILURE);
        }

        if(S_ISREG(stat_buf.st_mode)){
            printf("[DIR-%u] trovato il file '%s' in '%s'\n", td->thread_n + 1, entry->d_name, td->dirname);

            if(sem_wait(&td->ds->empty) != 0){
                perror("sem_wait");
                exit(EXIT_FAILURE);
            }

            if(pthread_mutex_lock(&td->ds->lock) != 0){
                perror("pthread_mutex_lock");
                exit(EXIT_FAILURE);
            }

            td->ds->index_in = (td->ds->index_in + 1) % PATH_SIZE;
            td->ds->size++;

            strncpy(td->ds->path[td->ds->index_in], pathfile, PATH_MAX);

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

    td->ds->done++;

    if(pthread_mutex_unlock(&td->ds->lock) != 0){
        perror("pthread_mutex_unlock");
        exit(EXIT_FAILURE);
    }
    
    closedir(dr);
}

int count_occorrenze(const char* buffer, const char* word){
    int count = 0;
    const char* p = buffer;
    while((p = strcasestr(p, word)) != NULL){ //case-insensitive per questo uso stracasestr
        count++;
        p += strlen(word);
    }
    return count;
}

int search_word_file(const char* filepath, const char* word){
    FILE* f;
    if((f = fopen(filepath, "r")) == NULL){ 
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    char buffer[1024];
    int total = 0;
    while(fgets(buffer, sizeof(buffer), f)){
        total += count_occorrenze(buffer, word);
    }
    fclose(f);
    return total;
}

void search_function(void* arg){
    sm_thread_data* td = (sm_thread_data*)arg;

    char buffer[PATH_MAX];
    int occorrenze;
    bool done = 0;

    while(1){
        if(sem_wait(&td->ds->full) != 0){
            perror("sem_wait");
            exit(EXIT_FAILURE);
        }

        if(pthread_mutex_lock(&td->ds->lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }

        strncpy(buffer, td->ds->path[td->ds->index_out], PATH_MAX);
        td->ds->index_out = (td->ds->index_out + 1) % PATH_SIZE;
        td->ds->size--;

        if(td->ds->done == td->dir_n && td->ds->size == 0){
            done = 1;
        }

        if(pthread_mutex_unlock(&td->ds->lock) != 0){
            perror("pthread_mutex_unlock");
            exit(EXIT_FAILURE);
        }
        
        if(sem_post(&td->ds->empty) != 0){
            perror("sem_Post");
            exit(EXIT_FAILURE);
        }

        occorrenze = search_word_file(buffer, td->word);
        printf("[SEARCH] nel file '%s' sono state trovate %d occorrenze\n", buffer, occorrenze);

        if(occorrenze > 0){
            if(sem_wait(&td->sm->empty) != 0){
                perror("sem_wait");
                exit(EXIT_FAILURE);
            }

            if(pthread_mutex_lock(&td->sm->lock) != 0){
                perror("pthread_mutex_lock");
                exit(EXIT_FAILURE);
            }

            strncpy(td->sm->buffer[td->sm->index_in], buffer, PATH_MAX);
            td->sm->occorrenze[td->sm->index_in] = occorrenze;
            td->sm->index_in = (td->sm->index_in + 1) % BUFFER_SIZE;
            td->sm->size++;

            if(pthread_mutex_unlock(&td->sm->lock) != 0){
                perror("pthread_mutex_unlock");
                exit(EXIT_FAILURE);
            }

            if(sem_post(&td->sm->full) != 0){
                perror("sem_post");
                exit(EXIT_FAILURE);
            }
        }

        if(done){
            if(sem_wait(&td->sm->empty) != 0){
                perror("sem_wait");
                exit(EXIT_FAILURE);
            }

            if(pthread_mutex_lock(&td->sm->lock) != 0){
                perror("pthread_mutex_lock");
                exit(EXIT_FAILURE);
            }

            td->sm->done = true;

            if(pthread_mutex_unlock(&td->sm->lock) != 0){
                perror("pthread_mutex_unlock");
                exit(EXIT_FAILURE);
            }
            if(sem_post(&td->sm->full) != 0){
                perror("sem_post");
                exit(EXIT_FAILURE);
            }
            break;
        }
    }
}

int main(int argc, char** argv){
    if(argc < 3){
        fprintf(stderr, "Usage: %s <word> <dir-1> <dir-2> <...> <dir-n>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char* word = argv[1];
    int n_dirs = argc - 2;
    shared_ds* ds = init_sharedds();
    shared_sm* sm = init_sharedsm();

    dir_thread_data* td_dir = malloc(sizeof(dir_thread_data) * n_dirs);
    for(int i = 0; i < n_dirs; i++){
        td_dir[i].dirname = argv[i + 2];
        td_dir[i].ds = ds;
        td_dir[i].thread_n = i;
        if(pthread_create(&td_dir[i].tid, NULL, (void*)dir_function, &td_dir[i]) != 0){
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }

    sm_thread_data* td_sm = malloc(sizeof(sm_thread_data));
    td_sm->word = word;
    td_sm->sm = sm;
    td_sm->ds = ds;
    td_sm->dir_n = n_dirs;
    if(pthread_create(&td_sm->tid, NULL, (void*)search_function, td_sm) != 0){
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    int occorrenze = 0;
    while(1){
        if(sem_wait(&sm->full) != 0){
            perror("sem_wait");
            exit(EXIT_FAILURE);
        }

        if(pthread_mutex_lock(&sm->lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }

        if(sm->done && sm->size == 0){
            if(pthread_mutex_unlock(&sm->lock) != 0){
                perror("pthread_mutex_unlock");
                exit(EXIT_FAILURE);
            }
            break;
        }

        char current_path[PATH_MAX];
        strncpy(current_path, sm->buffer[sm->index_out], PATH_MAX);
        int current_count = sm->occorrenze[sm->index_out];
        sm->index_out = (sm->index_out + 1) % BUFFER_SIZE;
        sm->size--;

        if(pthread_mutex_unlock(&sm->lock) != 0){
            perror("pthread_mutex_unlock");
            exit(EXIT_FAILURE);
        }

        if(sem_post(&sm->empty) != 0){
            perror("sem_post");
            exit(EXIT_FAILURE);
        }

        occorrenze += current_count;
        printf("[MAIN] con il file '%s' il totale parziale è di %d occorrenze\n", current_path, occorrenze);

    }

    printf("[MAIN] il totale finale di occorrenze è %d\n", occorrenze);

    for(int i = 0; i < n_dirs; i++){
        if(pthread_join(td_dir[i].tid, NULL) != 0){
            perror("pthread_join");
            exit(EXIT_FAILURE);
        }
    }

    if(pthread_join(td_sm->tid, NULL) != 0){
        perror("pthread_join");
        exit(EXIT_FAILURE);
    }


    shared_destroy_ds(ds);
    shared_destroy_sm(sm);
    free(td_dir);
    free(td_sm);
}
