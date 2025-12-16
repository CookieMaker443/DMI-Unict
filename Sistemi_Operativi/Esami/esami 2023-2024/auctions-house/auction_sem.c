#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>

#define BUFFER_SIZE 100

typedef struct{
    char object_description[BUFFER_SIZE];
    int min_offer;
    int max_offer;
    int temp_offer;
    bool done;
    int index_auction;
    int index_bidders;  // Indice del bidder che ha inviato l'offerta (0-based)

    pthread_mutex_t lock;
    sem_t sem_b, sem_j; // sem_b: per notificare ai bidder; sem_j: per notificare al giudice che l'offerta è pronta
} shared;

typedef struct{
    pthread_t tid;
    unsigned thread_n;
    shared* sh;
} thread_data;

int parse_line(char* buffer, char* name, int* min, int* max){
    char* token = strtok(buffer, ",");
    if(token != NULL){
        strcpy(name, token);
        token = strtok(NULL, ",");
        if(token != NULL){
            *min = atoi(token);
            token = strtok(NULL, ",");
            if(token != NULL){
                *max = atoi(token);
                return 0;
            }
        }
    }
    return -1;
}

shared* init_shared(){
    shared* sh = malloc(sizeof(shared));
    if(!sh){
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    sh->done = false;
    sh->index_auction = 0;
    sh->index_bidders = 0;
    
    if(pthread_mutex_init(&sh->lock, NULL) != 0){
        perror("pthread_mutex_init");
        exit(EXIT_FAILURE);
    }

    if(sem_init(&sh->sem_b, 0, 0) != 0){
        perror("sem_init sem_b");
        exit(EXIT_FAILURE);
    }

    if(sem_init(&sh->sem_j, 0, 0) != 0){
        perror("sem_init sem_j");
        exit(EXIT_FAILURE);
    }

    return sh;
}

void shared_destroy(shared* sh){
    pthread_mutex_destroy(&sh->lock);
    sem_destroy(&sh->sem_b);
    sem_destroy(&sh->sem_j);
    free(sh);
}

void bidders_function(void* arg){
    thread_data* td = (thread_data*)arg;

    printf("[B%u] offerente pronto\n", td->thread_n);

    while(1){
        if(sem_wait(&td->sh->sem_b) != 0){
            perror("sem_wait sem_b");
            exit(EXIT_FAILURE);
        }

        // Acquisisce il mutex per controllare il flag e generare l'offerta
        if(pthread_mutex_lock(&td->sh->lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }

        if(td->sh->done){
            if(pthread_mutex_unlock(&td->sh->lock) != 0){
                perror("pthread_mutex_unlock");
                exit(EXIT_FAILURE);
            }
            break;
        }

        int offer = 1 + rand() % td->sh->max_offer;
        td->sh->temp_offer = offer;
        td->sh->index_bidders = td->thread_n - 1; 
        printf("[B%d] invio offerta di %d EUR per asta n. %d\n", td->thread_n, offer, td->sh->index_auction);

        if(pthread_mutex_unlock(&td->sh->lock) != 0){
            perror("pthread_mutex_unlock");
            exit(EXIT_FAILURE);
        }

        if(sem_post(&td->sh->sem_j) != 0){
            perror("sem_post sem_j");
            exit(EXIT_FAILURE);
        }
    }
}

int main(int argc, char** argv){
    if(argc < 3){
        fprintf(stderr, "Usage: %s <file.txt> <num-bidders>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int num_bidders = atoi(argv[2]);
    shared* sh = init_shared();
    thread_data* td = malloc(sizeof(thread_data) * num_bidders);
    
    FILE* f;
    int auction_count = 1;
    char buffer[BUFFER_SIZE];   
    int* offers = malloc(sizeof(int) * num_bidders);
    int* rank = malloc(sizeof(int) * num_bidders);
    int order = 0;

    for(int i = 0; i < num_bidders; i++){
        td[i].sh = sh;
        td[i].thread_n = i + 1;
        if(pthread_create(&td[i].tid, NULL, (void*)bidders_function, &td[i]) != 0){
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }

    if((f = fopen(argv[1], "r")) == NULL){
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    int total_auction = 0, auctions_assigned = 0, auctions_void = 0, total_collected = 0;

    while(fgets(buffer, BUFFER_SIZE, f)){
        if(buffer[strlen(buffer)-1] == '\n'){
            buffer[strlen(buffer)-1] = '\0';
        }

        char name[BUFFER_SIZE];
        int min, max;
        if(parse_line(buffer, name, &min, &max) != 0){
            fprintf(stderr, "[J] Formato della riga non valido\n");
            continue;
        }

        // Imposta i parametri dell'asta nella struttura condivisa
        if(pthread_mutex_lock(&sh->lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }
        strcpy(sh->object_description, name);
        sh->min_offer = min;
        sh->max_offer = max;
        sh->index_auction = auction_count;
        if(pthread_mutex_unlock(&sh->lock) != 0){
            perror("pthread_mutex_unlock");
            exit(EXIT_FAILURE);
        }

        printf("[J] lancio asta n.%d per %s con offerta min %d EUR e offerta max %d EUR\n",auction_count, sh->object_description, sh->min_offer, sh->max_offer);
        
        for(int i = 0; i < num_bidders; i++){
            if(sem_post(&sh->sem_b) != 0){
                perror("sem_post sem_b");
                exit(EXIT_FAILURE);
            }
            if(sem_wait(&sh->sem_j) != 0){
                perror("sem_wait sem_j");
                exit(EXIT_FAILURE);
            }

            if(pthread_mutex_lock(&sh->lock) != 0){
                perror("pthread_mutex_lock");
                exit(EXIT_FAILURE);
            }
            int bidder_index = sh->index_bidders; 
            offers[bidder_index] = sh->temp_offer;
            rank[bidder_index] = order++;
            printf("[J] ricevuta offerta da B%d di %d EUR\n", bidder_index + 1, offers[bidder_index]);
            if(pthread_mutex_unlock(&sh->lock) != 0){
                perror("pthread_mutex_unlock");
                exit(EXIT_FAILURE);
            }
        }

        int winner = -1;
        int best_offer = -1;
        int valid_count = 0;
        for(int i = 0; i < num_bidders; i++){
            if(offers[i] >= sh->min_offer && offers[i] <= sh->max_offer){
                valid_count++;
                if(offers[i] > best_offer){
                    best_offer = offers[i];
                    winner = i;
                } else if(offers[i] == best_offer){
                    if(rank[i] < rank[winner]){
                        winner = i;
                    }
                }
            }
        }
        total_auction++;
        if(winner >= 0){
            auctions_assigned++;
            total_collected += best_offer;
            printf("[J] l'asta n.%d per %s si è conclusa con %d offerte valide su %d, il vincitore è B%d che si aggiudica l'oggetto per %d EUR\n",auction_count, sh->object_description, valid_count, num_bidders, winner + 1, best_offer);
        } else {
            auctions_void++;
            printf("[J] l'asta n.%d per %s si è conclusa senza alcuna offerta valida\n",auction_count, sh->object_description);
        }
        auction_count++;

        // Resetta l'indicatore di asta
        if(pthread_mutex_lock(&sh->lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }
        sh->index_auction = 0;
        if(pthread_mutex_unlock(&sh->lock) != 0){
            perror("pthread_mutex_unlock");
            exit(EXIT_FAILURE);
        }
    }
    fclose(f);

    printf("[J] sono state svolte %d aste di cui %d andate assegnate e %d andate a vuoto; il totale raccolto è di %d EUR\n",total_auction, auctions_assigned, auctions_void, total_collected);

    if(pthread_mutex_lock(&sh->lock) != 0){
        perror("pthread_mutex_lock");
        exit(EXIT_FAILURE);
    }
    sh->done = true;
    if(pthread_mutex_unlock(&sh->lock) != 0){
        perror("pthread_mutex_unlock");
        exit(EXIT_FAILURE);
    }
    for(int i = 0 ; i < num_bidders; i++){
        if(sem_post(&sh->sem_b) != 0){
            perror("sem_post sem_b");
            exit(EXIT_FAILURE);
        }
    }

    for(int i = 0; i < num_bidders; i++){
        if(pthread_join(td[i].tid, NULL) != 0){
            perror("pthread_join");
            exit(EXIT_FAILURE);
        }
    }

    free(offers);
    free(rank);
    shared_destroy(sh);
    free(td);
}
