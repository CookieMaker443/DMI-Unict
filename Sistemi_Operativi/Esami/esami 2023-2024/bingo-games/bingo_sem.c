#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>
#include <time.h>

#define ROW 3
#define COL 5
#define MAX_NUMBER 75

typedef struct{
    int** card;
    int current_number;
    int player_id_bingo;
    int player_id_cinquina;
    bool is_cinquina;
    bool done;

    sem_t* reader;
    sem_t write;
}shared;

typedef struct{
    unsigned thread_n;
    int n_card;
    shared* sh;
    pthread_t tid;
}thread_player;

typedef struct{
    int n_card;
    int n_player;
    shared* sh;
    pthread_t tid;
}thread_dealer;

int** init_card(){
    int** card = malloc(sizeof(int*) * ROW);
    for(int i = 0; i < ROW; i++){
        card[i] = calloc(COL, sizeof(int));
    }
    return card;
}

void free_card(int** card){
    for(int i = 0; i < ROW; i++){
        free(card[i]);
    }
    free(card);
}

void shuffle(int* arr, int n ){ //randomizzare i numeri
    if(n > 1){
        for(int i = 0; i < n; i++){
            int j = rand()%n;
            int t = arr[j];
            arr[j] = arr[i];
            arr[i] = t;
        }
    }
}

void generate_card(int** card){ //Serve al dealer
    int numbers[MAX_NUMBER];
    for(int i = 0; i < MAX_NUMBER; i++){
        numbers[i] = i + 1;
    }
    shuffle(numbers, MAX_NUMBER);
    int k = 0;
    for(int i = 0; i < ROW; i++){
        for(int j = 0; j < COL; j++){
            card[i][j] = numbers[k++];
        }
    }
}

void copy_card(int** src, int** dest){ // copia per la memoria condivisa
    for(int i = 0; i < ROW; i++){
        for(int j = 0; j < COL; j++){
            dest[i][j] = src[i][j];
        }
    }
}

 //indica chi il bingo e mette a zero i numeri estratti
int mark_number(int** card, int number){
    int cinquina_count = 0;
    for(int i = 0; i < ROW; i++){
        int count_row = 0;
        for(int j = 0; j< COL; j++){
            if(card[i][j] == number){
                card[i][j] = 0;
            }

            if(card[i][j] == 0){
                count_row++;
            }
        }
        if(count_row == COL){
            cinquina_count++;
        }
    }
    if(cinquina_count == ROW){
        return 1;
    }else if(cinquina_count > 0){
        return 0;
    }
    return -1;
}

void print_card(int** card){
    for(int i = 0; i < ROW; i++){
        if(i == 0){
            printf("(");
        }else{
            printf(" (");
        }
        for(int j = 0; j <COL; j++){
            if(j == 0){
                printf("%d", card[i][j]);
            }else{
                printf(",%d", card[i][j]);
            }
        }
        printf(")\n");
    }
}

shared* init_shared(int n_player){
    shared* sh = malloc(sizeof(shared));

    sh->card = init_card();
    sh->player_id_cinquina = sh->player_id_bingo = -1;
    sh->current_number = -1;
    sh->done = false;
    sh->is_cinquina = false;

    sh->reader = malloc(sizeof(sem_t) * n_player);
    for(int i = 0; i < n_player; i++){
        if(sem_init(&sh->reader[i], 0, 0) != 0){
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


void shared_destroy(shared* sh, int n_player){
    for(int i = 0; i < n_player; i++){
        sem_destroy(&sh->reader[i]);
    }
    free(sh->reader);
    free_card(sh->card);
    sem_destroy(&sh->write);
    free(sh);
}

void dealer_thread(void* arg){
    thread_dealer* td = (thread_dealer*)arg;

    printf("[D] ci saranno %d giocatori con %d schede ciascuno\n", td->n_player, td->n_card);

    int card_counter = 1;

    for(int i = 0; i < td->n_player; i++){
        for(int j = 0; j < td->n_card; j++){
            generate_card(td->sh->card);
            printf("[D] genero e distribuisco la scheda n.%d ", card_counter++);
            print_card(td->sh->card);
            if(sem_post(&td->sh->reader[i]) != 0){
                perror("sem_post");
                exit(EXIT_FAILURE);
            }
            if(sem_wait(&td->sh->write) != 0){
                perror("sem_wait");
                exit(EXIT_FAILURE);
            }
        }
    }

    printf("[D] fine della distribuzione delle schede e inizia l'estrazione dei numeri\n");

    int number[MAX_NUMBER];
    for(int i = 0; i < MAX_NUMBER; i++){
        number[i] = i + 1;
    }
    shuffle(number, MAX_NUMBER);
    int k = 0;
    while(!td->sh->done && k < MAX_NUMBER){
        td->sh->current_number = number[k++];
        printf("[D] numero estratto: %d\n", td->sh->current_number);

        for(int i = 0; i < td->n_player; i++){
            if(sem_post(&td->sh->reader[i]) != 0){
                perror("sem_post");
                exit(EXIT_FAILURE);
            }

            if(sem_wait(&td->sh->write) != 0){
                perror("sem_wait");
                exit(EXIT_FAILURE);
            }

            if(td->sh->player_id_bingo >= 0){
                printf("[D] il giocatore n.%d ha vinto il bingo con la scheda\n", td->sh->player_id_bingo + 1);
                print_card(td->sh->card);
                td->sh->done = true;
                break;
            }else if(!td->sh->is_cinquina && td->sh->player_id_cinquina >= 0){
                printf("[D] il giocatore n.%d ha vinto la cinquina con la scheda\n", td->sh->player_id_bingo + 1);
                print_card(td->sh->card);
                td->sh->is_cinquina = true;
            }
        }
    }
    printf("[D] Fine del gioco\n");

    for(int i = 0; i < td->n_player; i++){
        if(sem_post(&td->sh->reader[i]) != 0){
            perror("sem_post");
            exit(EXIT_FAILURE);
        }
    }
}

void player_thread(void* arg){
    thread_player* td = (thread_player*)arg;

    int*** card = malloc(sizeof(int**) * td->n_card);
    int*** card_copy = malloc(sizeof(int**) * td->n_card);

    for(int i = 0; i < td->n_card; i++){
        card[i] = init_card();
        card_copy[i] = init_card();
    }

    for(int i = 0; i < td->n_card; i++){
        if(sem_wait(&td->sh->reader[td->thread_n]) != 0){
            perror("sem_wait");
            exit(EXIT_FAILURE);
        }

        copy_card(td->sh->card, card[i]);
        copy_card(td->sh->card, card_copy[i]);

        if(sem_post(&td->sh->write) != 0){
            perror("sem_post");
            exit(EXIT_FAILURE);
        }

        printf("[P%d] ricevuta scheda ", td->thread_n + 1);
        print_card(card[i]);
    }

    int status_game = -1;
    while(!td->sh->done){
        if(sem_wait(&td->sh->reader[td->thread_n]) != 0){
            perror("sem_wait");
            exit(EXIT_FAILURE);
        }

        if(td->sh->done){
            break;
        }

        for(int i = 0; i < td->n_card; i++){
            int status = mark_number(card_copy[i], td->sh->current_number);
            if(status != status_game){
                if(status == 1){
                    printf("[P%d] scheda con il Bingo\n", td->thread_n + 1);
                    print_card(card[i]);
                    td->sh->player_id_bingo = td->thread_n;
                    copy_card(card[i], td->sh->card);
                    break;
                }else if(status == 0 && !td->sh->player_id_cinquina){
                    printf("[P%d] scheda con la cinquina\n", td->thread_n +1);
                    print_card(card[i]);
                    td->sh->player_id_cinquina = td->thread_n;
                    copy_card(card[i], td->sh->card);
                }
                status_game = status;
            }
        }

        if(sem_post(&td->sh->write) != 0){
            perror("sem_post");
            exit(EXIT_FAILURE);
        }
    }

    for(int i = 0; i < td->n_card; i++){
        free_card(card[i]);
        free_card(card_copy[i]);
    }
    free(card);
    free(card_copy);
}

int main(int argc, char** argv){
    if(argc != 3){
        fprintf(stderr, "Usage: %s <n> <m>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int n_player = atoi(argv[1]);
    int m_card = atoi(argv[2]);

    srand(time(NULL));
    shared* sh = init_shared(n_player);

    thread_player* pl = malloc(sizeof(thread_player) * n_player);
    for(int i = 0; i < n_player; i++){
        pl[i].thread_n = i;
        pl[i].n_card = m_card;
        pl[i].sh = sh;

        if(pthread_create(&pl[i].tid, NULL, (void*)player_thread, &pl[i]) != 0){
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }

    thread_dealer* dealer = malloc(sizeof(thread_dealer));

    dealer->n_player = n_player;
    dealer->n_card = m_card;
    dealer->sh = sh;

    if(pthread_create(&dealer->tid, NULL, (void*)dealer_thread, dealer) != 0){
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    if(pthread_join(dealer->tid, NULL) != 0){
        perror("pthread_join");
        exit(EXIT_FAILURE);
    }

    for(int i = 0; i < n_player; i++){
        if(pthread_join(pl[i].tid, NULL) != 0){
            perror("pthread_join");
            exit(EXIT_FAILURE);
        }
    }
    free(pl);
    shared_destroy(sh, n_player);
    free(dealer);
}