#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#define BUFFER_SIZE 100

typedef struct{
    int index_asta;
    char object_description[BUFFER_SIZE];
    int minimu_offer;
    int maximun_offer;
    int temp_offer;
    int index_bidder;
    int exit;

    pthread_mutex_t lock;
    pthread_cond_t cond_judge, cond_biders;
}shared;

typedef struct{
    pthread_t tid;
    unsigned thread_n;
    shared* sh;
}thread_data;

shared* init_shared(){
    shared* sh = malloc(sizeof(shared));

    sh->exit = sh->index_asta = 0;

    if(pthread_mutex_init(&sh->lock, NULL) != 0){
        perror("Pthread_mutex_init");
        exit(EXIT_FAILURE);
    }

    if(pthread_cond_init(&sh->cond_judge, NULL) != 0){
        perror("pthread_cond_init");
        exit(EXIT_FAILURE);
    }

    if(pthread_cond_init(&sh->cond_biders, NULL) != 0){
        perror("pthread_cond_init");
        exit(EXIT_FAILURE);
    }

    return sh;
}

void shared_destroy(shared* sh){
    pthread_mutex_destroy(&sh->lock);
    pthread_cond_destroy(&sh->cond_judge);
    pthread_cond_destroy(&sh->cond_biders);
    free(sh);
}

int parse_line(char* line, char* name, int* min, int* max){
    char* token = strtok(line, ",");
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

void bidder_thread(void* arg){
    thread_data* td = (thread_data*)arg;

    printf("[B%d] offerente pronto\n", td->thread_n);

    while(1){
        if(pthread_mutex_lock(&td->sh->lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }

        while(td->sh->index_asta == 0 && td->sh->exit == 0){
            if(pthread_cond_wait(&td->sh->cond_biders, &td->sh->lock) != 0){
                perror("Pthread_cond_wait");
                exit(EXIT_FAILURE);
            }
        }

        if(td->sh->exit){
            if(pthread_mutex_unlock(&td->sh->lock) != 0){
                perror("pthread_mutex_unlock");
                exit(EXIT_FAILURE);
            }
            break;
        }

        int offer = 1 + rand() % td->sh->maximun_offer;
        td->sh->temp_offer = offer;
        td->sh->index_bidder = td->thread_n - 1;
        
        if(pthread_cond_signal(&td->sh->cond_judge) != 0){
            perror("pthread_cond_signal");
            exit(EXIT_FAILURE);
        }
        if(pthread_mutex_unlock(&td->sh->lock) != 0){
            perror("pthread_mutex_unlock");
            exit(EXIT_FAILURE);
        }
    }
}


int main(int argc, char** argv){
    if(argc < 3){
        fprintf(stderr, "Usage: %s <auction_file> <num_biders>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int num_biders = atoi(argv[2]);
    shared* sh = init_shared();
    thread_data* td = malloc(sizeof(thread_data) * num_biders);

    for(int i = 0; i < num_biders; i++){
        td[i].thread_n = i + 1;
        td[i].sh = sh;

        if(pthread_create(&td[i].tid, NULL, (void*)bidder_thread, &td[i]) != 0){
            perror("pthread_Create");
            exit(EXIT_FAILURE);
        }
    }

    FILE* f;
    char buffer[BUFFER_SIZE];
    int auction_count = 1;
    int* offerte = malloc(sizeof(int) * num_biders);
    int* ranking = malloc(sizeof(int) * num_biders);
    int order = 0;

    if((f = fopen(argv[1], "r")) == NULL){
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    int total_auction, auctions_assigned, auction_void, total_collected = 0;

    while(fgets(buffer, BUFFER_SIZE, f)){
        char name[BUFFER_SIZE];
        int min, max;
        if(parse_line(buffer, name, &min, &max) != 0){
            fprintf(stderr, "Formato della riga non valida");
            continue;
        }

        if(pthread_mutex_lock(&td->sh->lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }

        strcpy(td->sh->object_description, name);
        td->sh->minimu_offer = min;
        td->sh->maximun_offer = max;
        td->sh->index_asta = auction_count;

        if(pthread_cond_broadcast(&td->sh->cond_biders) != 0){
            perror("Pthread_cond_broadcast");
            exit(EXIT_FAILURE);
        }
        if(pthread_mutex_unlock(&td->sh->lock) != 0){
            perror("Pthread_mutex_unlock");
            exit(EXIT_FAILURE);
        }

        printf("[J] lancio asta n.%d per %s con offerta min %d EUR e offerta max %d EUR\n", auction_count, td->sh->object_description, td->sh->minimu_offer, td->sh->maximun_offer);
    
        for(int i = 0; i < num_biders; i++){
            if(pthread_mutex_lock(&td->sh->lock) != 0){
                perror("pthread_mutex_lock");
                exit(EXIT_FAILURE);
            }

            if(pthread_cond_wait(&td->sh->cond_judge, &td->sh->lock) != 0){
                perror("pthread_cond_wait");
                exit(EXIT_FAILURE);
            }

            int bidder_index = td->sh->index_bidder;
            offerte[bidder_index] = td->sh->temp_offer;
            ranking[bidder_index] = order++;
            printf("[J] ricevuta offerta da B%d di %d EUR\n", bidder_index + 1, offerte[bidder_index]);
            if(pthread_mutex_unlock(&td->sh->lock) != 0){
                perror("pthread_mutex_unlock");
                exit(EXIT_FAILURE);
            }
        }

        int winner = -1;
        int best_offer = -1;
        int valid_count = 0;
        for(int i = 0; i < num_biders; i++){
            if(offerte[i] >= td->sh->minimu_offer && offerte[i] <= td->sh->maximun_offer){
                valid_count++;
                if(offerte[i] > best_offer){
                    best_offer = offerte[i];
                    winner = i;
                }else if(offerte[i] == best_offer){
                    if(ranking[i] < ranking[winner]){
                        winner = i;
                    }
                }
            }
        }

        total_auction++;
        if(winner >= 0){
            auctions_assigned++;
            total_collected += best_offer;
            printf("[J] l'asta n.%d per %s si è conclusa con %d offerte valide su %d, il vincitore è B%d che si aggiudica l'oggetto per %d EUR\n",
            auction_count, td->sh->object_description, valid_count, num_biders, winner + 1, best_offer);
        }else{
            auction_void++;
            printf("[J] l'asta n.%d per %s si è conclusa senza alcuna offerta valida\n", auction_count, td->sh->object_description);
        }
        auction_count++;

        if(pthread_mutex_lock(&td->sh->lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }
        td->sh->index_asta = 0;

        if(pthread_mutex_unlock(&td->sh->lock) != 0){
            perror("pthread_mutex_unlock");
            exit(EXIT_FAILURE);
        }
    }

    fclose(f);

    printf("[J] sono state svolte %d aste di cui %d andate assegnate e %d andate a vuoto; il totale raccolto è %d EUR\n", total_auction, auctions_assigned, auction_void, total_collected);

    if(pthread_mutex_lock(&td->sh->lock) != 0){
        perror("pthread_mutex_lock");
        exit(EXIT_FAILURE);
    }

    td->sh->exit = 1;

    if(pthread_cond_broadcast(&td->sh->cond_biders) != 0){
        perror("pthread_cond_broadcast");
        exit(EXIT_FAILURE);
    }

    if(pthread_mutex_unlock(&td->sh->lock) != 0){
        perror("pthread_mutex_unlock");
        exit(EXIT_FAILURE);
    }

    for(int i = 0; i < num_biders; i++){
        if(pthread_join(td[i].tid, NULL) != 0){
            perror("Pthread_join");
            exit(EXIT_FAILURE);
        }
    }

    free(offerte);
    free(ranking);
    shared_destroy(sh);
    free(td);
}