#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>

#define BUFFER_SIZE 100

typedef struct{
    char object_description[BUFFER_SIZE];
    int minimum_offer;
    int maximum_offer;
    int offer;
    bool done;

    int index_asta;
    int success;
    int failed;
    int total_price;

    int best_offer;
    int best_bidders;

    pthread_mutex_t lock;
    sem_t sem_j, sem_b;
}shared;

typedef struct{
    pthread_t tid;
    char* auction_file;
    int nbidders;
    shared* sh;
}thread_judge;

typedef struct{
    pthread_t tid;
    unsigned thread_n;
    shared* sh;
}thread_bidders;

shared* init_shared(){
    shared* sh = malloc(sizeof(shared));
    
    sh->total_price = sh->success = sh->failed = sh->best_offer = 0;
    sh->best_bidders = -1;
    sh->index_asta = 1;
    sh->done = false;

    if(pthread_mutex_init(&sh->lock, NULL) != 0){
        perror("pthread_mutex_init");
        exit(EXIT_FAILURE);
    } 

    if(sem_init(&sh->sem_j, 0, 1) != 0){
        perror("sem_init");
        exit(EXIT_FAILURE);
    }

    if(sem_init(&sh->sem_b, 0, 0) != 0){
        perror("sem_init");
        exit(EXIT_FAILURE);
    }

    return sh;
}

void shared_destroy(shared* sh){
    pthread_mutex_destroy(&sh->lock);
    sem_destroy(&sh->sem_j);
    sem_destroy(&sh->sem_b);
    free(sh);
}

void judge_function(void* arg){
    thread_judge* td = (thread_judge*)arg;

    FILE* f;
    char buffer[BUFFER_SIZE];
    char* oggetto, *s_min, *s_max;

    if((f = fopen(td->auction_file, "r")) == NULL){
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    while(fgets(buffer, BUFFER_SIZE, f)){
        if(buffer[strlen(buffer) - 1] == '\n'){
            buffer[strlen(buffer) - 1] = '\0';
        }

        if(sem_wait(&td->sh->sem_j) != 0){
            perror("sem_wait");
            exit(EXIT_FAILURE);
        }

        if(pthread_mutex_lock(&td->sh->lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }

        if((oggetto = strtok(buffer, ",")) != NULL && (s_min = strtok(NULL, ",")) != NULL && (s_max = strtok(NULL, ",")) != NULL){
            strcpy(td->sh->object_description, oggetto);
            td->sh->maximum_offer = atoi(s_max);
            td->sh->minimum_offer = atoi(s_min);
        }

        printf("[J] lancio asta n.%d per %s con offerta minima di %d EUR e massima di %d EUR\n", td->sh->index_asta, td->sh->object_description, td->sh->minimum_offer, td->sh->maximum_offer);

        td->sh->best_bidders = -1;
        td->sh->best_offer = 0;
        
        if(pthread_mutex_unlock(&td->sh->lock) != 0){
            perror("pthread_mutex_unlock");
            exit(EXIT_FAILURE);
        }

        for(int i = 0; i < td->nbidders; i++){
            if(sem_post(&td->sh->sem_b) != 0){
                perror("sem_post");
                exit(EXIT_FAILURE);
            }
        }

        for(int i = 0; i < td->nbidders; i++){
            if(sem_wait(&td->sh->sem_j) != 0){
                perror("sem_wait");
                exit(EXIT_FAILURE);
            }
        }

        if(pthread_mutex_lock(&td->sh->lock) != 0){
            perror("pthred_mutex_lock");
            exit(EXIT_FAILURE);
        }

        if(td->sh->best_offer >= td->sh->minimum_offer){
            td->sh->success++;
            td->sh->total_price += td->sh->best_offer;
            printf("[J] l'asta n.%d per %s si è conclusa con offerta valida; il vincitore è B%d che ha offerto %d EUR\n",td->sh->index_asta, td->sh->object_description, td->sh->best_bidders + 1, td->sh->best_offer);
        }else{
            td->sh->failed++;
            printf("[J] l'asta n.%d per %s si è conclusa senza offerta valida, l'oggetto non risulta assegnato\n", td->sh->index_asta, td->sh->object_description);
        }

        td->sh->index_asta++;

        if(pthread_mutex_unlock(&td->sh->lock) != 0){
            perror("pthread_mutex_unlock");
            exit(EXIT_FAILURE);
        }

        if(sem_post(&td->sh->sem_j) != 0){
            perror("sem_post");
            exit(EXIT_FAILURE);
        }
    }
    fclose(f);

    printf("[J] sono state svolte %d aste di cui %d andate assegnate e %d andate a vuoto; il totale raccolte è di %d EUR\n", td->sh->index_asta, td->sh->success, td->sh->failed, td->sh->total_price);

    if(pthread_mutex_lock(&td->sh->lock) != 0){
        perror("pthread_mutex_lock");
        exit(EXIT_FAILURE);
    }

    td->sh->done = true;

    if(pthread_mutex_unlock(&td->sh->lock) != 0){
        perror("pthread_mutex_unlock");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < td->nbidders; i++) {
        if(sem_post(&td->sh->sem_b) != 0){
            perror("sem_post");
            exit(EXIT_FAILURE);
        }
    }
    
}

void bidders_function(void* arg){
    thread_bidders* td = (thread_bidders*)arg;

    while(1){
        if(sem_wait(&td->sh->sem_b) != 0){
            perror("sem_wait");
            exit(EXIT_FAILURE);
        }

        if(pthread_mutex_lock(&td->sh->lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }

        if(td->sh->done){
            if(pthread_mutex_unlock(&td->sh->lock) != 0){
                perror("ptherad_mutex_unlock");
                exit(EXIT_FAILURE);
            }
            break;
        }

        int offer = 1 + rand()% td->sh->maximum_offer;
        td->sh->offer = offer;

        if(offer > td->sh->best_offer){
            td->sh->best_offer = offer;
            td->sh->best_bidders = td->thread_n;
        }
        
        printf("[B%u] invio offera di %d EUR per asta n.%d\n", td->thread_n + 1, offer, td->sh->index_asta);

        if(pthread_mutex_unlock(&td->sh->lock) != 0){
            perror("pthread_mutex_unlock");
            exit(EXIT_FAILURE);
        }

        if(sem_post(&td->sh->sem_j) != 0){
            perror("sem_post");
            exit(EXIT_FAILURE);
        }
    }
}

int main(int argc, char** argv){
    if(argc != 3){
        fprintf(stderr, "Usage %s <auction-file> <num-bidders>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int nbidders = atoi(argv[2]);
    shared* sh = init_shared();

    thread_judge jd;
    jd.nbidders = nbidders;
    jd.sh = sh;
    jd.auction_file = argv[1];
    if(pthread_create(&jd.tid, NULL, (void*)judge_function, &jd) != 0){
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    thread_bidders* bd = malloc(sizeof(thread_bidders) * nbidders);
    for(int i = 0; i < nbidders; i++){
        bd[i].thread_n = i;
        bd[i].sh = sh;

        if(pthread_create(&bd[i].tid, NULL, (void*)bidders_function, &bd[i]) != 0){
            perror("pthread_Create");
            exit(EXIT_FAILURE);
        } 
    }

    for(int i = 0; i < nbidders; i++){
        if(pthread_join(bd[i].tid, NULL) != 0){
            perror("pthread_join");
            exit(EXIT_FAILURE);
        }
    }

    if(pthread_join(jd.tid, NULL) != 0){
        perror("pthread_join");
        exit(EXIT_FAILURE);
    }

    shared_destroy(sh);
    free(bd);
}