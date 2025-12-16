#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <dirent.h>
#include <sys/stat.h>
#include <pthread.h>
#include <unistd.h>

#define PATH_SIZE 10

typedef struct{
    char pathname[PATH_SIZE][PATH_MAX];
    int index_in, index_out;
    int size;
    unsigned ndone;
    bool done;

    pthread_mutex_t lock;
    pthread_cond_t full, empty;
}shared_ds;

typedef struct{
    char pathname[PATH_MAX];
    unsigned long size;
    int fase;

    pthread_cond_t read, write;
}shared_ms;

typedef struct{
    pthread_t tid;
    unsigned thread_n;
    char* pathfile;
    int ndir;
    shared_ds* sd;
}thread_dir;

typedef struct{
    pthread_t tid;
    shared_ds* sd;
    shared_ms* ms;
}thread_stat;

shared_ds* init_sharedds(){
    shared_ds* sh = malloc(sizeof(shared_ds));

    sh->index_in = sh->index_out = sh->size = sh->ndone = 0;
    sh->done = false;

    if(pthread_mutex_init(&sh->lock, NULL) != 0){
        perror("ptherad_mutex_init");
        exit(EXIT_FAILURE);
    }

    if(pthread_cond_init(&sh->full, NULL) != 0){
        perror("pthread_cond_init");
        exit(EXIT_FAILURE);
    }

    if(pthread_cond_init(&sh->empty, NULL) != 0){
        perror("pthread_cond_init");
        exit(EXIT_FAILURE);
    }

    return sh;
}

shared_ms* init_sharedms(){
    shared_ms* sh = malloc(sizeof(shared_ms));
    
    sh->size = sh->fase = 0;

    if(pthread_cond_init(&sh->read, NULL) != 0){
        perror("pthread_cond_init");
        exit(EXIT_FAILURE);
    }

    if(pthread_cond_init(&sh->write, NULL) != 0){
        perror("pthread_cond_init");
        exit(EXIT_FAILURE);
    }

    return sh;
}

void sharedds_destroy(shared_ds* sh){
    pthread_mutex_destroy(&sh->lock);
    pthread_cond_destroy(&sh->full);
    pthread_cond_destroy(&sh->empty);
    free(sh);
}

void sharedms_destroy(shared_ms* sh){
    pthread_cond_destroy(&sh->read);
    pthread_cond_destroy(&sh->write);
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

    printf("[D-%u] scansione della cartella '%s'\n", td->thread_n + 1, td->pathfile);

    while((entry = readdir(dr))){
        snprintf(path, PATH_MAX, "%s/%s", td->pathfile, entry->d_name);

        if(lstat(path, &statbuf) == -1){
            perror("lstat");
            exit(EXIT_FAILURE);
        }

        if(S_ISREG(statbuf.st_mode)){
            if(pthread_mutex_lock(&td->sd->lock) != 0){
                perror("pthread_mutex_lock");
                exit(EXIT_FAILURE);
            }

            while(td->sd->size == PATH_SIZE){
                if(pthread_cond_wait(&td->sd->empty, &td->sd->lock) != 0){
                    perror("pthread_cond_wait");
                    exit(EXIT_FAILURE);
                }
            }

            td->sd->index_in = (td->sd->index_in + 1) % PATH_SIZE;
            td->sd->size++;
            strncpy(td->sd->pathname[td->sd->index_in], path, PATH_MAX );

            if(pthread_cond_signal(&td->sd->full) != 0){
                perror("pthread_cond_signal");
                exit(EXIT_FAILURE);
            }

            if(pthread_mutex_unlock(&td->sd->lock) != 0){
                perror("pthread_mutex_unlock");
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

    if(td->sd->ndone == td->ndir){
        td->sd->done = true;

        if(pthread_cond_broadcast(&td->sd->full) != 0){
            perror("pthread_cond_signal");
            exit(EXIT_FAILURE);
        }
    }

    if(pthread_mutex_unlock(&td->sd->lock) != 0){
        perror("pthread_mutex_unlock");
        exit(EXIT_FAILURE);
    }
}

void stat_function(void* arg){
    thread_stat* td = (thread_stat*)arg;

    char* filepath;
    struct stat statbuf;
    struct dirent* entry;

    while(1){
        if(pthread_mutex_lock(&td->sd->lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }

        while(td->sd->size == 0 && !td->sd->done){
            if(pthread_cond_wait(&td->sd->full, &td->sd->lock) != 0){
                perror("pthread_cond_wait");
                exit(EXIT_FAILURE);
            }   
        }

        while(td->ms->fase != 0 && !td->sd->done){
            if(pthread_cond_wait(&td->ms->write, &td->sd->lock) != 0){
                perror("pthread_cond_wait");
                exit(EXIT_FAILURE);
            }
        }

        if(td->sd->done){
            if(pthread_mutex_unlock(&td->sd->lock) != 0){
                perror("pthread_mutex_unlock");
                exit(EXIT_FAILURE);
            }
            break;
        }

        td->sd->index_out = (td->sd->index_out + 1) % PATH_SIZE;
        td->sd->size--;

        filepath = td->sd->pathname[td->sd->index_out];

        if(lstat(filepath, &statbuf) == -1){
            perror("lstat");
            exit(EXIT_FAILURE);
        }

        printf("[STAT] il file '%s' ha dimensione %lu byte\n", filepath, statbuf.st_size);

        strncpy(td->ms->pathname, filepath, PAHT_MAX);
        td->ms->size = statbuf.st_size;
        td->ms->fase = 1;

        if(pthread_cond_signal(&td->ms->read) != 0){
            perror("pthread_cond_signal");
            exit(EXIT_FAILURE);
        }

        if(pthread_mutex_unlock(&td->sd->lock) != 0){
            perror("pthread_mutex_unlock");
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
    shared_ds* sd = init_sharedds();
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

    unsigned long total_bytes = 0;

    while(1){
        if(pthread_mutex_lock(&sd->lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }

        while(ms->fase != 1 && !sd->done){
            if(pthread_cond_wait(&ms->read, &sd->lock) != 0){
                perror("Pthread_cond_wait");
                exit(EXIT_FAILURE);
            }
        }

        total_bytes += ms->size;
        ms->fase = 0;

        if(sd->done){
            if(pthread_mutex_unlock(&sd->lock) != 0){
                perror("pthread_mutex_unlock");
                exit(EXIT_FAILURE);
            }
            break;
        }else{
            printf("[MAIN] con il file '%s' il totale parziale è di %lu byte\n", ms->pathname, total_bytes);
        }


        if(pthread_cond_signal(&ms->write) != 0){
            perror("pthread_cond_Signal");
            exit(EXIT_FAILURE);
        }

        if(pthread_mutex_unlock(&sd->lock) != 0){
            perror("Pthread_mutex_unlock");
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

    sharedms_destroy(ms);
    sharedds_destroy(sd);
    free(dr);
}