#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>

#define PATH_SIZE 10
#define BUFFER_SIZE 10
#define MAX_LENGTH 1024

typedef struct{
    char pathdir[PATH_SIZE][PATH_MAX]; //pathname di capienza prefissata 10
    int index_in, index_out; //indici per inserire ed estrarre gli elementi dal bbuffer
    unsigned long size; //dimensione del buffer per indicare gli elementi inseriti e rimossi
    unsigned long done; //thread che hanno concluso

    pthread_mutex_t lock;
    pthread_cond_t full;
    pthread_cond_t empty;
}shared_ds;

typedef struct{
    char buffer[BUFFER_SIZE][PATH_MAX]; //buffer che contiene il pathname
    int occorrenze[BUFFER_SIZE]; //buffer che contiene le occorrenze della parola
    int index_in, index_out; //indici per inserire e rimuovere elementi dal buffer
    unsigned long size; //dimensione del buffer
    bool done; //flag di fine

    pthread_mutex_t lock;
    pthread_cond_t full; //indica se il buffer è pieno 
    pthread_cond_t empty; //indica se il buffer è vuoto
}shared_sm;

typedef struct{
    char* directory; //contiene le dir da analizzare
    unsigned thread_n; //n thread_dir
    int total_dir; //numero totale di directory
    shared_ds* sh;
    pthread_t tid;
}ds_thread_data;

typedef struct{
    pthread_t tid;
    int ndir; //numero di directory da analizzare
    char* word; //contiene la word da trovare
    shared_ds* ds;
    shared_sm* sm;
}sm_thread_data;

shared_ds* init_sharedds(){
    shared_ds* sh = malloc(sizeof(shared_ds));

    sh->index_in = sh->index_out = sh->size = sh->done = 0;

    if(pthread_mutex_init(&sh->lock, NULL) != 0){
        perror("pthread_mutex_init");
        exit(EXIT_FAILURE);
    }

    if(pthread_cond_init(&sh->empty, NULL) != 0){
        perror("Pthread_cond_init");
        exit(EXIT_FAILURE);
    }

    if(pthread_cond_init(&sh->full, NULL) != 0){
        perror("pthread_cond_init");
        exit(EXIT_FAILURE);
    }

    return sh;
}

shared_sm* init_sharedms(){
    shared_sm* sm = malloc(sizeof(shared_sm));

    sm->index_in = sm->index_out = sm->size = sm->done = 0;

    if(pthread_mutex_init(&sm->lock, NULL) != 0){
        perror("pthread_mutex_init");
        exit(EXIT_FAILURE);
    }

    if(pthread_cond_init(&sm->full, NULL) != 0){
        perror("pthread_cond_init");
        exit(EXIT_FAILURE);
    }

    if(pthread_cond_init(&sm->empty, NULL) != 0){
        perror("pthread_cond_init");
        exit(EXIT_FAILURE);
    }

    return sm;
}

void destroy_sharedds(shared_ds* sh){
    pthread_mutex_destroy(&sh->lock);
    pthread_cond_destroy(&sh->empty);
    pthread_cond_destroy(&sh->full);
    free(sh);
}

void destroy_sharedms(shared_sm* sh){
    pthread_mutex_destroy(&sh->lock);
    pthread_cond_destroy(&sh->empty);
    pthread_cond_destroy(&sh->full);
    free(sh);
}

void dir_function(void* arg){
    ds_thread_data* td = (ds_thread_data*)arg;

    DIR* dr;
    char pathfile[PATH_MAX]; //buffer temporaneo che contiene il path da analizzare
    struct stat statbuf; //serve a ottenere informazioni dalla directory es dimensione
    struct dirent* entry;//costruisce il percorso completo di tutti gli elementi presenti nella directory 

    if((dr = opendir(td->directory)) == NULL){
        perror("opendir");
        exit(EXIT_FAILURE);
    }    

    printf("[DIR-%u] scansione della cartella '%s'\n", td->thread_n, td->directory);

    while((entry = readdir(dr))){   
        if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0){ //ignora le directory speciali "." (la directory corrente) e ".." (la directory padre)
            continue;
        }

        snprintf(pathfile, PATH_MAX, "%s/%s", td->directory, entry->d_name);
//formatta una stringa che rappresenta il percorso completo del file, combinando la directory e il nome del file, separandoli con una barra.

        if(lstat(pathfile, &statbuf) == -1){
            perror("lstat");
            exit(EXIT_FAILURE);
        }

        if(S_ISREG(statbuf.st_mode)){
            printf("[DIR-%u] Trovato il file '%s' in '%s'\n", td->thread_n, entry->d_name, td->directory);

            if(pthread_mutex_lock(&td->sh->lock) != 0){
                perror("pthread_mutex_lock");
                exit(EXIT_FAILURE);
            }

            while(td->sh->size == PATH_SIZE){ //FIN QUANDO il buffer è pieno sono in attesa
                if(pthread_cond_wait(&td->sh->empty, &td->sh->lock) != 0){
                    perror("pthread_cond_wait");
                    exit(EXIT_FAILURE);
                }
            }
            
            td->sh->index_in = (td->sh->index_in + 1) % PATH_SIZE;
            td->sh->size++;
            strncpy(td->sh->pathdir[td->sh->index_in], pathfile, PATH_MAX);

            if(pthread_cond_signal(&td->sh->full) != 0){
                perror("pthread_cond_signal");
                exit(EXIT_FAILURE);
            }

            if(pthread_mutex_unlock(&td->sh->lock) != 0){
                perror("pthread_mutex_unlock");
                exit(EXIT_FAILURE);
            }
        }
    }

    if(pthread_mutex_lock(&td->sh->lock) != 0){
        perror("pthread-mutex_lock");
        exit(EXIT_FAILURE);
    }

    td->sh->done++;

    if(td->sh->done == td->total_dir && td->sh->size == 0){ //se dopo aver terminato non ci sono elementi nel buffer sveglia il consumatore
        if(pthread_cond_signal(&td->sh->full) != 0){
            perror("pthread_cond_signal");
            exit(EXIT_FAILURE);
        }
    }

    if(pthread_mutex_unlock(&td->sh->lock) != 0){
        perror("pthread_mutex_unlock");
        exit(EXIT_FAILURE);
    }
    closedir(dr);
}

int counter_occorrenze(const char* pathname, const char* word){
    const char* p = pathname;
    int count = 0;
    while((p = strcasestr(p, word)) != NULL){
        count++;
        p += strlen(word);
    } 
    return count;
}

int search_file_word(const char* path, const char* word){
    FILE* f;

    if((f = fopen(path, "r")) == NULL){
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    char buffer[MAX_LENGTH];
    int totale = 0;
    while(fgets(buffer, MAX_LENGTH, f)){
        totale += counter_occorrenze(buffer, word);
    }
    fclose(f);
    return totale;  
}

void search_function(void* arg){
    sm_thread_data* td = (sm_thread_data*)arg;

    char buffer[PATH_MAX];
    int occorrenze;
    bool done = 0;

    while(1){
        if(pthread_mutex_lock(&td->ds->lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }

//Se il buffer è vuoto e non tutti i thread DIR hanno terminato, attende che si riempì
        while(td->ds->size == 0 && td->ds->done < td->ndir){
            if(pthread_cond_wait(&td->ds->full, &td->ds->lock) != 0){
                perror("pthread_cond_wait");
                exit(EXIT_FAILURE);
            }
        }

        strncpy(buffer, td->ds->pathdir[td->ds->index_out], PATH_MAX);
        td->ds->index_out = (td->ds->index_out + 1) % PATH_SIZE;
        td->ds->size--;

        if(td->ds->done == td->ndir && td->ds->size == 0){ //se il numero di thread che ha finito è uguale al numero di directory
            done = 1;
            if(pthread_mutex_unlock(&td->ds->lock) != 0){
                perror("pthread_mutex_unlock");
                exit(EXIT_FAILURE);
            } 
            break;
        }

        if(pthread_cond_signal(&td->ds->empty) != 0){
            perror("pthread_cond_signal");
            exit(EXIT_FAILURE);
        }

        if(pthread_mutex_unlock(&td->ds->lock) != 0){
            perror("pthread_mutex_unlock");
            exit(EXIT_FAILURE);
        }

        occorrenze = search_file_word(buffer, td->word);
        printf("[SEARCH] nel file '%s' sono state trovate %d occorrenze\n", buffer , occorrenze);
        
        if(occorrenze > 0){
            if(pthread_mutex_lock(&td->sm->lock) != 0){
                perror("pthread_mutex_lock");
                exit(EXIT_FAILURE);
            }

            while(td->sm->size == BUFFER_SIZE){
                if(pthread_cond_wait(&td->sm->emtpy, &td->sm->lock) != 0){
                    perror("Pthread_cond_wait");
                    exit(EXIT_FAILURE);
                }
            }

            strncpy(td->sm->buffer[td->sm->index_in], buffer, PATH_MAX);
            td->sm->occorrenze[td->sm->index_in] = occorrenze;
            td->sm->index_in = (td->sm->index_in + 1) % BUFFER_SIZE;
            td->sm->size++;

            if(pthread_cond_signal(&td->sm->full) != 0){
                perror("pthread_cond_signal");
                exit(EXIT_FAILURE);
            }

            if(pthread_mutex_unlock(&td->sm->lock) != 0){
                perror("pthread_mutex_unlock");
                exit(EXIT_FAILURE);
            }
        }

        if(done){
            if(pthread_mutex_lock(&td->sm->lock) != 0){
                perror("pthread_mutex_lock");
                exit(EXIT_FAILURE);
            }

            td->sm->done = 1;

            if(pthread_cond_signal(&td->sm->full) != 0){
                perror("pthread_cond_signal");
                exit(EXIT_FAILURE);
            }

            if(pthread_mutex_unlock(&td->sm->lock) != 0){
                perror("pthread_mutex_unlock");
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
    int ndir = argc - 2;
    shared_ds* ds = init_sharedds();
    shared_sm* sm = init_sharedms();

    ds_thread_data* tds = malloc(sizeof(ds_thread_data) * ndir);

    for(int i = 0; i < ndir; i++){
        tds[i].directory = argv[i + 2];
        tds[i].sh = ds;
        tds[i].thread_n = i;
        tds[i].total_dir = ndir;
        if(pthread_create(&tds[i].tid, NULL, (void*)dir_function, &tds[i]) != 0){
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }

    sm_thread_data* tdsm = malloc(sizeof(sm_thread_data));
    tdsm->ds = ds;
    tdsm->sm = sm;
    tdsm->ndir = ndir;
    tdsm->word = word;
    if(pthread_create(&tdsm->tid, NULL, (void*)search_function, tdsm) != 0){
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    int occorenze = 0;
    while(1){
        if(pthread_mutex_lock(&sm->lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }

        while(tdsm->sm->size == 0 && !sm->done){
            if(pthread_cond_wait(&tdsm->sm->full, &tdsm->sm->lock) != 0){
                perror("phtread_cond_wait");
                exit(EXIT_FAILURE);
            }
        }

        char current_path[PATH_MAX];
        strncpy(current_path, sm->buffer[sm->index_out], PATH_MAX);
        int current_count = sm->occorrenze[sm->index_out]; //VALORE occorrenza
        sm->index_out = (sm->index_out + 1) % BUFFER_SIZE;
        sm->size--;

        if(sm->size == 0 && sm->done){
            if(pthread_mutex_unlock(&sm->lock) != 0){
                perror("pthread_mutex_unlock");
                exit(EXIT_FAILURE);
            }
            break;
        }        

        if(pthread_cond_signal(&sm->empty) != 0){
            perror("pthread_cond_signal");
            exit(EXIT_FAILURE);
        }

        if(pthread_mutex_unlock(&sm->lock) != 0){
            perror("phtread_mutex_unlock");
            exit(EXIT_FAILURE);
        }

        occorenze += current_count;
        printf("[MAIN] con il file '%s' contiene %d il totale parziale è di %d occorrenze\n", current_path, occorenze);
    }

    printf("[MAIN] il totale finale è di %d occorrenze\n", occorenze);

    for(int i = 0; i < ndir; i++){
        if(pthread_join(tds[i].tid, NULL) != 0){
            perror("pthread_join");
            exit(EXIT_FAILURE);
        }
    }

    if(pthread_join(tdsm->tid, NULL) != 0){
        perror("pthread_join");
        exit(EXIT_FAILURE);
    }

    free(tds);
    free(tdsm);
    destroy_sharedds(ds);
    destroy_sharedms(sm);
}