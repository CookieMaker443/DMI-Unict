#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <stdbool.h>

#define ROW 3
#define COL 5
#define MAX_NUMBER 75

typedef struct {
    int** scheda;            
    int numero_estratto;     
    int player_id_bingo;     
    int player_id_cinquina;  
    bool is_cinquina;        
    bool done;               
    int turn;                
    int waiting;           
    int counter_extraction;  

    pthread_mutex_t lock;
    pthread_cond_t* cond_p;  //Sbagliato perchè utilizza un array
    pthread_cond_t cond_d;   
} shared;

typedef struct {
    pthread_t tid;
    shared* sh;
    int nplayers;
    int mcards;
} thread_dealer;

typedef struct {
    int mcards;
    pthread_t tid;
    unsigned thread_n;
    shared* sh;
} thread_player;

void shuffle(int* arr, int n) {
    if(n > 1){
        for(int i = 0; i < n; i++){
            int j = rand() % n;
            int t = arr[i];
            arr[i] = arr[j];
            arr[j] = t;
        }
    }
}

int** init_card(){
    int** scheda = malloc(sizeof(int*) * ROW);
    for(int i = 0; i < ROW; i++){
        scheda[i] = calloc(COL, sizeof(int));
    }
    return scheda;
}

void copy_card(int** scheda, int** scheda_copy){
    for(int i = 0; i < ROW; i++){
        for(int j = 0; j < COL; j++){
            scheda_copy[i][j] = scheda[i][j];
        }
    }
}

void print_card(int** scheda){
    for(int i = 0; i < ROW; i++){
        if(i == 0)
            printf("(");
        else
            printf(" (");
        for(int j = 0; j < COL; j++){
            if(j == 0)
                printf("%d", scheda[i][j]);
            else
                printf(", %d", scheda[i][j]);
        }
        printf(")\n");
    }
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

int mark_number(int** scheda, int number){
    int counter_row = 0;
    for(int i = 0; i < ROW; i++){
        int counter_numeriestratti = 0;
        for(int j = 0; j < COL; j++){
            if(scheda[i][j] == number){
                scheda[i][j] = 0;
            }
            if(scheda[i][j] == 0){
                counter_numeriestratti++;
            }
        }
        if(counter_numeriestratti == COL){
            counter_row++;
        }
    }
    if(counter_row == ROW){
        return 1;  // Bingo
    }else if(counter_row > 0){
        return 0;  
    }
    return -1;
}

void free_card(int** scheda){
    for(int i = 0; i < ROW; i++){
        free(scheda[i]);
    }
    free(scheda);
}

shared* init_shared(int nplayers){
    shared* sh = malloc(sizeof(shared));
    sh->numero_estratto = sh->player_id_bingo = sh->player_id_cinquina = -1;
    sh->turn = nplayers;  
    sh->waiting = sh->counter_extraction = 0;
    sh->done = sh->is_cinquina = false;
    sh->scheda = init_card();
    
    if(pthread_mutex_init(&sh->lock, NULL) != 0){
        perror("pthread_mutex_init");
        exit(EXIT_FAILURE);
    }

    sh->cond_p = malloc(sizeof(pthread_cond_t) * nplayers);
    for(int i = 0; i < nplayers; i++){
        if(pthread_cond_init(&sh->cond_p[i], NULL) != 0){
            perror("pthread_cond_init");
            exit(EXIT_FAILURE);
        }
    }
    if(pthread_cond_init(&sh->cond_d, NULL) != 0){
        perror("pthread_cond_init");
        exit(EXIT_FAILURE);
    }
    return sh;
}

void shared_destroy(shared* sh, int nplayers){
    pthread_mutex_destroy(&sh->lock);
    for(int i = 0; i < nplayers; i++){
        pthread_cond_destroy(&sh->cond_p[i]);
    }
    free(sh->cond_p);
    pthread_cond_destroy(&sh->cond_d);
    free_card(sh->scheda);
    free(sh);
}

void dealer_thread(void* arg){
    thread_dealer* td = (thread_dealer*) arg;
    printf("[D] ci saranno %d giocatori con %d schede ciascuno\n", td->nplayers, td->mcards);

    int card_counter = 1;

    for(int i = 0; i < td->nplayers; i++){
        for(int j = 0; j < td->mcards; j++){
            if(pthread_mutex_lock(&td->sh->lock) != 0){
                perror("pthread_mutex_lock");
                exit(EXIT_FAILURE);
            }
            while(td->sh->turn != td->nplayers){
                if(pthread_cond_wait(&td->sh->cond_d, &td->sh->lock) != 0){
                    perror("pthread_cond_wait");
                    exit(EXIT_FAILURE);
                }
            }
            generate_card(td->sh->scheda);
            printf("[D] genero e distribuisco la scheda n.%d:\n", card_counter++);
            print_card(td->sh->scheda);

            td->sh->turn = i;   
            td->sh->waiting = 1;
            if(pthread_cond_signal(&td->sh->cond_p[i]) != 0){
                perror("pthread_cond_signal");
                exit(EXIT_FAILURE);
            }
            while(td->sh->waiting > 0){
                if(pthread_cond_wait(&td->sh->cond_d, &td->sh->lock) != 0){
                    perror("pthread_cond_wait");
                    exit(EXIT_FAILURE);
                }
            }
            td->sh->turn = td->nplayers;  
            if(pthread_mutex_unlock(&td->sh->lock) != 0){
                perror("pthread_mutex_unlock");
                exit(EXIT_FAILURE);
            }
        }
    }

    printf("[D] Fine della distribuzione delle schede. Inizio estrazione dei numeri.\n");

    int number[MAX_NUMBER];
    for(int i = 0; i < MAX_NUMBER; i++){
        number[i] = i + 1;
    }
    shuffle(number, MAX_NUMBER);
    int k = 0;
    while(!td->sh->done && k < MAX_NUMBER){
        if(pthread_mutex_lock(&td->sh->lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }
        td->sh->numero_estratto = number[k++];
        td->sh->counter_extraction++;
        td->sh->waiting = td->nplayers;
        printf("[D] Numero estratto: %d\n", td->sh->numero_estratto);

        for(int i = 0; i < td->nplayers; i++){
            if(pthread_cond_signal(&td->sh->cond_p[i]) != 0){
                perror("pthread_cond_signal");
                exit(EXIT_FAILURE);
            }
        }
        while(td->sh->waiting > 0){
            if(pthread_cond_wait(&td->sh->cond_d, &td->sh->lock) != 0){
                perror("pthread_cond_wait");
                exit(EXIT_FAILURE);
            }
        }
       
        if(td->sh->player_id_bingo >= 0){
            printf("[D] il giocatore n.%d ha vinto il bingo con la scheda:\n", td->sh->player_id_bingo + 1);
            print_card(td->sh->scheda);
            td->sh->done = true;
        }
        else if(!td->sh->is_cinquina && td->sh->player_id_cinquina >= 0){
            printf("[D] il giocatore n.%d ha vinto la cinquina con la scheda:\n", td->sh->player_id_cinquina + 1);
            print_card(td->sh->scheda);
            td->sh->is_cinquina = true;
        }
        if(pthread_mutex_unlock(&td->sh->lock) != 0){
            perror("pthread_mutex_unlock");
            exit(EXIT_FAILURE);
        }
    }

    if(pthread_mutex_lock(&td->sh->lock) != 0){
        perror("pthread_mutex_lock");
        exit(EXIT_FAILURE);
    }
    td->sh->done = true;
    for (int i = 0; i < td->nplayers; i++){
        if(pthread_cond_signal(&td->sh->cond_p[i]) != 0){
            perror("pthread_cond_signal");
            exit(EXIT_FAILURE);
        }
    }
    if(pthread_mutex_unlock(&td->sh->lock) != 0){
        perror("pthread_mutex_unlock");
        exit(EXIT_FAILURE);
    }
    printf("[D] Fine del gioco.\n");
}

// Thread player
void player_thread(void* arg){
    thread_player* tp = (thread_player*) arg;
    int n = tp->mcards;

    int*** scheda = malloc(sizeof(int**) * n);
    int*** scheda_copy = malloc(sizeof(int**) * n);
    for(int i = 0; i < n; i++){
        scheda[i] = init_card();
        scheda_copy[i] = init_card();
    }

    for(int i = 0; i < n; i++){
        if(pthread_mutex_lock(&tp->sh->lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }
        while(tp->sh->turn != tp->thread_n){
            if(pthread_cond_wait(&tp->sh->cond_p[tp->thread_n], &tp->sh->lock) != 0){
                perror("pthread_cond_wait");
                exit(EXIT_FAILURE);
            }
        }
        copy_card(tp->sh->scheda, scheda[i]);
        copy_card(tp->sh->scheda, scheda_copy[i]);
        tp->sh->waiting--;
        if(pthread_cond_signal(&tp->sh->cond_d) != 0){
            perror("pthread_cond_signal");
            exit(EXIT_FAILURE);
        }
        if(pthread_mutex_unlock(&tp->sh->lock) != 0){
            perror("pthread_mutex_unlock");
            exit(EXIT_FAILURE);
        }
        printf("[P%u] ricevuta scheda:\n", tp->thread_n + 1);
        print_card(scheda[i]);
    }

    int status_game = -1;
    int last_extraction = 0;
    while(1){
        if(pthread_mutex_lock(&tp->sh->lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }
        while(!tp->sh->done && tp->sh->counter_extraction == last_extraction){
            if(pthread_cond_wait(&tp->sh->cond_p[tp->thread_n], &tp->sh->lock) != 0){
                perror("pthread_cond_wait");
                exit(EXIT_FAILURE);
            }
        }
        if(tp->sh->done){
            if(pthread_mutex_unlock(&tp->sh->lock) != 0){
                perror("pthread_mutex_unlock");
                exit(EXIT_FAILURE);
            }
            break;
        }
        last_extraction = tp->sh->counter_extraction;
        int extracted = tp->sh->numero_estratto;
        if(pthread_mutex_unlock(&tp->sh->lock) != 0){
            perror("pthread_mutex_unlock");
            exit(EXIT_FAILURE);
        }
    
        for(int i = 0; i < n; i++){
            int status = mark_number(scheda_copy[i], extracted);
            if(status_game != status){
                if(status == 1){
                    printf("[P%u] card con il Bingo:\n", tp->thread_n + 1);
                    print_card(scheda[i]);
                    if(pthread_mutex_lock(&tp->sh->lock) != 0){
                        perror("pthread_mutex_lock");
                        exit(EXIT_FAILURE);
                    }
                    tp->sh->player_id_bingo = tp->thread_n;
                    copy_card(scheda[i], tp->sh->scheda);
                    if(pthread_mutex_unlock(&tp->sh->lock) != 0){
                        perror("pthread_mutex_unlock");
                        exit(EXIT_FAILURE);
                    }
                    break;
                }
                else if(status == 0 && tp->sh->player_id_cinquina < 0){
                    printf("[P%u] card con la cinquina:\n", tp->thread_n + 1);
                    print_card(scheda[i]);
                    if(pthread_mutex_lock(&tp->sh->lock) != 0){
                        perror("pthread_mutex_lock");
                        exit(EXIT_FAILURE);
                    }
                    tp->sh->player_id_cinquina = tp->thread_n;
                    copy_card(scheda[i], tp->sh->scheda);
                    if(pthread_mutex_unlock(&tp->sh->lock) != 0){
                        perror("pthread_mutex_unlock");
                        exit(EXIT_FAILURE);
                    }
                }
                status_game = status;
            }
        }
        if(pthread_mutex_lock(&tp->sh->lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }
        tp->sh->waiting--;
        if(pthread_cond_signal(&tp->sh->cond_d) != 0){
            perror("pthread_cond_signal");
            exit(EXIT_FAILURE);
        }
        if(pthread_mutex_unlock(&tp->sh->lock) != 0){
            perror("pthread_mutex_unlock");
            exit(EXIT_FAILURE);
        }
    }

    for(int i = 0; i < n; i++){
        free_card(scheda[i]);
        free_card(scheda_copy[i]);
    }
    free(scheda);
    free(scheda_copy);
}

int main(int argc, char** argv){
    if(argc < 3){
        fprintf(stderr, "Usage: %s <n-players> <m-schede>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int nplayers = atoi(argv[1]);
    int mcards = atoi(argv[2]);

    srand(time(NULL));
    shared* sh = init_shared(nplayers);

    thread_player* pl = malloc(sizeof(thread_player) * nplayers);
    for(int i = 0; i < nplayers; i++){
        pl[i].thread_n = i;
        pl[i].mcards = mcards;
        pl[i].sh = sh;
        if(pthread_create(&pl[i].tid, NULL, (void*)player_thread, &pl[i]) != 0){
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }

    thread_dealer* dl = malloc(sizeof(thread_dealer));
    dl->mcards = mcards;
    dl->nplayers = nplayers;
    dl->sh = sh;
    if(pthread_create(&dl->tid, NULL, (void*)dealer_thread, dl) != 0){
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    if(pthread_join(dl->tid, NULL) != 0){
        perror("pthread_join");
        exit(EXIT_FAILURE);
    }

    for(int i = 0; i < nplayers; i++){
        if(pthread_join(pl[i].tid, NULL) != 0){
            perror("pthread_join");
            exit(EXIT_FAILURE);
        }
    }
    
    free(pl);
    shared_destroy(sh, nplayers);
    free(dl);
}
