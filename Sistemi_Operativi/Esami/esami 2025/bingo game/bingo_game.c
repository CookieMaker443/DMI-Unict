#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <semaphore.h>

#define ROW 3
#define COL 5
#define MAX_NUMBER 75

typedef struct{
    int** scheda;
    int numero_estratto;
    int player_cinquina;
    int player_bingo;
    bool is_cinquina;
    bool done;

    pthread_mutex_t lock;
    sem_t* read;
    sem_t write;
}shared;

typedef struct{
    pthread_t tid;
    unsigned thread_n;
    int mcards;
    shared* sh;
}thread_player;

typedef struct{
    pthread_t tid;
    shared* sh;
    int mcards;
    int nplayers;
}thread_dealer;

void shuffle(int* arr, int n){
    if(n > 1){
        for(int i = 0; i < n; i++){
            int j = rand()%n;
            int t = arr[j];
            arr[j] = arr[i];
            arr[i] = t;
        }
    }
}

int** init_scheda(){
    int** scheda = malloc(sizeof(int*) * ROW);
    for(int i = 0; i < ROW; i++){
        scheda[i] = calloc(COL, sizeof(int));
    }
    return scheda;
}

void free_scheda(int** scheda){
    for(int i = 0; i < ROW; i++){
        free(scheda[i]);
    }
    free(scheda);
}

void generate_card(int** scheda){
    int number[MAX_NUMBER];
    for(int i = 0; i < MAX_NUMBER; i++){
        number[i] = i + 1;
    }
    shuffle(number, MAX_NUMBER);
    int k = 0; 
    for(int i = 0; i < ROW; i++){
        for(int j = 0; j < COL; j++){
            scheda[i][j] = number[k++];
        }
    }
}

void copy_scheda(int** scheda, int** scheda_copy){
    for(int i = 0; i < ROW; i++){
        for(int j = 0; j < COL; j++){
            scheda_copy[i][j] = scheda[i][j];
        }
    }
}

int mark_number(int** scheda, int number){
    int complate_row = 0;
    for(int i = 0; i < ROW; i++){
        int found_number = 0;
        for(int j = 0; j < COL; j++){
            if(scheda[i][j] == number){
                scheda[i][j] = 0;
            }

            if(scheda[i][j] == 0){
                found_number++;
            }
        }
        if(found_number == COL){
            complate_row++;
        }
    }

    if(complate_row == ROW){
        return 1;
    }else if(complate_row > 0){
        return 0;
    } 
    return -1;
}

void print_card(int** scheda){
    for(int i = 0; i < ROW; i++){
        if(i == 0){
            printf("(");
        }else{
            printf("(");
        }

        for(int j = 0; j < COL; j++){
            if(j == 0){
                printf("%d", scheda[i][j]);
            }else{
                printf(", %d", scheda[i][j]);
            }
        }
        printf(")\n");
    }
}

shared* init_shared(int nplayers){
    shared* sh = malloc(sizeof(shared));

    sh->scheda = init_scheda();
    sh->player_cinquina = sh->player_bingo = -1;
    sh->done = sh->is_cinquina = false;
    sh->numero_estratto = -1;

    if(pthread_mutex_init(&sh->lock, NULL) != 0){
        perror("pthread_mutex_init");
        exit(EXIT_FAILURE);
    }

    sh->read = malloc(sizeof(sem_t) * nplayers);
    for(int i = 0; i < nplayers; i++){
        if(sem_init(&sh->read[i], 0, 0) != 0){
            perror("sem_init");
            exit(EXIT_FAILURE);
        }
    }

    if(sem_init(&sh->write, 0, 0) != 0){
        perror("sem_init");
        exit(EXIT_FAILURE);
    }

    return sh;
}

void shared_destroy(shared* sh, int nplayers){
    pthread_mutex_destroy(&sh->lock);
    sem_destroy(&sh->write);
    for(int i = 0; i < nplayers; i++){
        sem_destroy(&sh->read[i]);
    }
    free(sh->read);
    free_scheda(sh->scheda);
    free(sh);
}

void dealer_function(void* arg){
    thread_dealer* td = (thread_dealer*)arg;

    printf("[D] ci saranno %d giocatori con %d schede ciascuno\n", td->nplayers, td->mcards);

    int schede_assegnate = 1;

    for(int i = 0; i < td->nplayers; i++){
        for(int j = 0; j < td->mcards; j++){
            if(pthread_mutex_lock(&td->sh->lock) != 0){
                perror("pthread_mutex_lock");
                exit(EXIT_FAILURE);
            }
            generate_card(td->sh->scheda);
            printf("[D] genero e distribuisco la scheda n.%d: \n", schede_assegnate++);
            print_card(td->sh->scheda);

            if(pthread_mutex_unlock(&td->sh->lock) != 0){
                perror("pthread_mutex_unlock");
                exit(EXIT_FAILURE);
            }

            if(sem_post(&td->sh->read[i]) != 0){
                perror("sem_post");
                exit(EXIT_FAILURE);
            }

            if(sem_wait(&td->sh->write) != 0){
                perror("sem_wait");
                exit(EXIT_FAILURE);
            }
        }
    }

    printf("[D] fine della distribuzione delle card e inizio di estrazione dei numeri\n");

    int number[MAX_NUMBER];
    for(int i = 0; i < MAX_NUMBER; i++){
        number[i] = i + 1;
    }
    shuffle(number, MAX_NUMBER);
    int k = 0;
    while(!td->sh->done && k < MAX_NUMBER){
        if(pthread_mutex_lock(&td->sh->lock) != 0){
            perror("ptherad_mutex_lock");
            exit(EXIT_FAILURE);
        }

        td->sh->numero_estratto = number[k++];

        if(pthread_mutex_unlock(&td->sh->lock) != 0){
            perror("pthread_mutex_unlock");
            exit(EXIT_FAILURE);
        }

        printf("[D] estrazione del prossimo numero: %d\n", td->sh->numero_estratto);
        

        for(int i = 0; i < td->nplayers; i++){
            if(sem_post(&td->sh->read[i]) != 0){
                perror("sem_post");
            }

            if(sem_wait(&td->sh->write) != 0){
                perror("sem_wait");
                exit(EXIT_FAILURE);
            }

            if(pthread_mutex_lock(&td->sh->lock) != 0){
                perror("pthread_mutex_lock");
                exit(EXIT_FAILURE);
            }

            if(td->sh->player_bingo >= 0){
                printf("[D] il giocatore n.%d ha vinto il Bingo con la scheda\n", td->sh->player_bingo);
                print_card(td->sh->scheda);
                td->sh->done = true;
                if(pthread_mutex_unlock(&td->sh->lock) != 0){
                    perror("pthread_mutex_unlock");
                    exit(EXIT_FAILURE);
                }
                break;
            }else if(td->sh->player_cinquina >= 0 && !td->sh->is_cinquina){
                printf("[D] il giocatore n.%d ha vinto la cinquiuna con la scheda\n", td->sh->player_cinquina);
                print_card(td->sh->scheda);
                td->sh->is_cinquina = true;
            }

            if(pthread_mutex_unlock(&td->sh->lock) != 0){
                perror("pthread_mutex_unlock");
                exit(EXIT_FAILURE);
            }
        }
    }

    printf("[D] fine del gioco\n");
    for(int i = 0; i < td->nplayers; i++){
        if(sem_post(&td->sh->read[i]) != 0){
            perror("sem_post");
            exit(EXIT_FAILURE);
        }
    }    
}

void player_function(void* arg){
    thread_player* td = (thread_player*)arg;

    int*** scheda = malloc(sizeof(int**) * td->mcards);
    int*** scheda_copy = malloc(sizeof(int**) *  td->mcards);

    for(int i = 0; i < td->mcards; i++){
        scheda[i] = init_scheda();
        scheda_copy[i] = init_scheda();
    }

    for(int i = 0; i < td->mcards; i++){
        if(sem_wait(&td->sh->read[td->thread_n]) != 0){
            perror("sem_Wait");
            exit(EXIT_FAILURE);
        }

        if(pthread_mutex_lock(&td->sh->lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }

        copy_scheda(td->sh->scheda, scheda[i]);
        copy_scheda(td->sh->scheda, scheda_copy[i]);

        if(pthread_mutex_unlock(&td->sh->lock) != 0){
            perror("pthread_mutex_unlock");
            exit(EXIT_FAILURE);
        }

        if(sem_post(&td->sh->write) != 0){
            perror("sem_post");
            exit(EXIT_FAILURE);
        }

        printf("[P%u] ricevuta card \n", td->thread_n + 1);
        print_card(scheda[i]);
    }

    int status_game = -1;
    while(!td->sh->done){
        if(sem_wait(&td->sh->read[td->thread_n]) != 0){
            perror("sem_wait");
            exit(EXIT_FAILURE);
        }

        if(td->sh->done){
            break;
        }

        for(int i = 0; i < td->mcards; i++){
            int status = mark_number(scheda_copy[i], td->sh->numero_estratto);
            if(status != status_game){
                if(status == 1){
                    printf("[P%u] card con il Bingo:\n", td->thread_n + 1);
                    print_card(scheda[i]);
                    if(pthread_mutex_lock(&td->sh->lock) != 0){
                        perror("pthread_mutex_lock");
                        exit(EXIT_FAILURE);
                    }
                    td->sh->player_bingo = td->thread_n;
                    copy_scheda(scheda[i], td->sh->scheda);
                    if(pthread_mutex_unlock(&td->sh->lock) != 0){
                        perror("pthread_mutex_unlock");
                        exit(EXIT_FAILURE);
                    }
                    break;
                }else if(status == 0 && !td->sh->is_cinquina){
                    printf("[P%d] card con cinquina:\n", td->thread_n +1);
                    print_card(scheda[i]);
                    if(pthread_mutex_lock(&td->sh->lock) != 0){
                        perror("pthread_mutex_lock");
                        exit(EXIT_FAILURE);
                    }

                    td->sh->player_cinquina = td->thread_n;
                    copy_scheda(scheda[i], td->sh->scheda);
                    if(pthread_mutex_unlock(&td->sh->lock) != 0){
                        perror("pthread_mutex_unlock");
                        exit(EXIT_FAILURE);
                    }
                }
                status_game = status;
            }
        }
        if(sem_post(&td->sh->write) != 0){
            perror("sem_post");
            exit(EXIT_FAILURE);
        }
    }

    for(int i = 0; i < td->mcards; i++){
        free_scheda(scheda[i]);
        free_scheda(scheda_copy[i]);
    }
    free(scheda);
    free(scheda_copy);
}   

int main(int argc, char** argv){
    if(argc < 3){
        fprintf(stderr, "Usage: %s <n-player> <m-cards>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    srand(time(NULL));
    int nplayers = atoi(argv[1]);
    int mcards = atoi(argv[2]);
    shared* sh = init_shared(nplayers);
    thread_player* pl = malloc(sizeof(thread_player) * nplayers);

    for(int i = 0; i < nplayers; i++){
        pl[i].sh = sh;
        pl[i].mcards = mcards;
        pl[i].thread_n = i;
        if(pthread_create(&pl[i].tid, NULL, (void*)player_function, &pl[i]) != 0){
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }

    thread_dealer* dl = malloc(sizeof(thread_dealer));
    dl->mcards = mcards;
    dl->sh = sh;
    dl->nplayers = nplayers;
    if(pthread_create(&dl->tid, NULL, (void*)dealer_function, dl) != 0){
        perror("pthread_Create");
        exit(EXIT_FAILURE);
    }

    if(pthread_join(dl->tid, NULL) != 0){
        perror("pthread_join");
        exit(EXIT_FAILURE);
    }

    for(int i = 0; i < nplayers; i++){
        if(pthread_join(pl[i].tid, NULL) != 0){
            perror("pthread_Join");
            exit(EXIT_FAILURE);
        }
    }

    shared_destroy(sh, nplayers);
    free(pl);
    free(dl);
}