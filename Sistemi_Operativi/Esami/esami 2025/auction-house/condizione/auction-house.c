#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>

#define BUFFER_SIZE 100

typedef struct{
    char object_description[BUFFER_SIZE];
    int minium_offer;
    int max_offer;
    int offer;
    bool done;

    int fase; // 0 fase iniziale, 1 fase asta
    int index_asta;
    int success;
    int failed;
    int total_price;

    int bids_submitted;     // contatore per il numero di offerte raccolte per l'asta corrente
    int currentAuction;     // identifica l'asta corrente
    int best_offer;         // Miglior offerta ricevuta
    int best_bidder;        // Il bidder che ha inviato la migliore offerta

    pthread_mutex_t lock;
    pthread_cond_t cond_j, cond_b;
}shared;

typedef struct{
    pthread_t tid;
    shared* sh;
    int nbidders;
    char* auction_file;
}thread_judge;

typedef struct{
    pthread_t tid;
    unsigned thread_n;
    shared* sh;
}thread_bidders;

shared* init_shared(){
    shared* sh = malloc(sizeof(shared));

    sh->best_offer = sh->failed = sh->success = sh->total_price = sh->fase = sh->bids_submitted = sh->currentAuction = 0;
    sh->best_bidder = -1;
    sh->done = false;
    sh->index_asta = 1;

    if(pthread_mutex_init(&sh->lock, NULL) != 0){
        perror("pthread_mutex_init");
        exit(EXIT_FAILURE);
    }

    if(pthread_cond_init(&sh->cond_j, NULL) != 0){
        perror("Pthread_cond_init");
        exit(EXIT_FAILURE);
    }

    if(pthread_cond_init(&sh->cond_b, NULL) != 0){
        perror("pthread_cond_init");
        exit(EXIT_FAILURE);
    }
    
    return sh;
}

void shared_destroy(shared* sh){
    pthread_mutex_destroy(&sh->lock);
    pthread_cond_destroy(&sh->cond_j);
    pthread_cond_destroy(&sh->cond_b);
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

        if(pthread_mutex_lock(&td->sh->lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }

        while(td->sh->fase != 0){
            if(pthread_cond_wait(&td->sh->cond_j, &td->sh->lock) != 0){
                perror("pthraed_cond_wait");
                exit(EXIT_FAILURE);
            }
        }

        if((oggetto = strtok(buffer, ",")) != NULL && (s_min = strtok(NULL, ",")) != NULL && (s_max = strtok(NULL, ",")) != NULL){
            strcpy(td->sh->object_description, oggetto);
            td->sh->minium_offer = atoi(s_min);
            td->sh->max_offer = atoi(s_max);
        }
        printf("[J] lancio asta n.%d per %s con offerta minima di %d EUR e massima di %d EUR\n", td->sh->index_asta, td->sh->object_description, td->sh->minium_offer, td->sh->max_offer);
        
        //inizializzo per la nuova asta 
        td->sh->currentAuction = td->sh->index_asta;
        td->sh->bids_submitted = 0;
        td->sh->best_offer = 0;
        td->sh->best_bidder = -1;
        td->sh->fase = 1;

        if(pthread_cond_broadcast(&td->sh->cond_b) != 0){
            perror("pthread_cond_broadcast");
            exit(EXIT_FAILURE);
        }

        if(pthread_mutex_unlock(&td->sh->lock) != 0){
            perror("pthread_mutex_unlock");
            exit(EXIT_FAILURE);
        }

        if(pthread_mutex_lock(&td->sh->lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }

        while(td->sh->bids_submitted < td ->nbidders){
            if(pthread_cond_wait(&td->sh->cond_j, &td->sh->lock) != 0){
                perror("pthread_cond_wait");
                exit(EXIT_FAILURE);
            }
        }

        if(td->sh->best_offer >= td->sh->minium_offer){
            td->sh->success++;
            td->sh->total_price += td->sh->best_offer;
            printf("[J] l'asta n.%d per %s si è conclusa con offerta valida; il vincitore è B%d che ha offerto %d EUR\n",td->sh->index_asta, td->sh->object_description, td->sh->best_bidder + 1, td->sh->best_offer);
        }else{
            td->sh->failed++;
            printf("[J] l'asta n.%d per %s si è conclusa senza offerta valida, l'oggetto non risulta assegnato\n", td->sh->index_asta, td->sh->object_description);
        }
        //stato prossima asta
        td->sh->fase = 0;
        td->sh->currentAuction = 0;
        td->sh->index_asta++;

        if(pthread_mutex_unlock(&td->sh->lock) != 0){
            perror("pthread_mutex_unlock");
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

    if(pthread_cond_broadcast(&td->sh->cond_b) != 0){
        perror("pthread_cond_broadcast");
        exit(EXIT_FAILURE);
    }

    if(pthread_mutex_unlock(&td->sh->lock) != 0){
        perror("pthread_mutex_unlock");
        exit(EXIT_FAILURE);
    }
}

void bidders_function(void* arg){
    thread_bidders* td = (thread_bidders*)arg;
    int lastAuction = 0; //flag per evitare di inviare più offerte

    while(1){
        if(pthread_mutex_lock(&td->sh->lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }

        while(td->sh->fase != 1 && !td->sh->done){
            if(pthread_cond_wait(&td->sh->cond_b, &td->sh->lock) != 0){
                perror("pthread_Cond_wait");
                exit(EXIT_FAILURE);
            }
        }

        if(td->sh->done){
            if(pthread_mutex_unlock(&td->sh->lock) != 0){
                perror("pthread_mutex_unlock");
                exit(EXIT_FAILURE);
            }
            break;
        }

        if(lastAuction == td->sh->currentAuction){
            if(pthread_mutex_unlock(&td->sh->lock) != 0){
                perror("pthread_mutex_unlock");
                exit(EXIT_FAILURE);
            }
            continue;
        }

        int offer = rand()% td->sh->max_offer + 1;
        td->sh->offer = offer;
        lastAuction = td->sh->currentAuction;
        
        if(offer > td->sh->best_offer){
            td->sh->best_offer = offer;
            td->sh->best_bidder = td->thread_n;
        }
        td->sh->bids_submitted++;

        printf("[B%u] invio offera di %d EUR per asta n.%d\n", td->thread_n + 1, offer, td->sh->index_asta);
        
        if(pthread_cond_signal(&td->sh->cond_j) != 0){
            perror("Pthread_Cond_signal");
            exit(EXIT_FAILURE);
        }

        if(pthread_mutex_unlock(&td->sh->lock) != 0){
            perror("Pthread_mutex_unlock");
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