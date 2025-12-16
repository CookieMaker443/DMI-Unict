#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <stdbool.h>
#include <semaphore.h>

#define ROW 3
#define COL 5
#define MAX_NUMBER 75

// Struttura condivisa
typedef struct {
    int** scheda;             
    int numero_estratto;       
    int player_bingo;         
    int player_cinquina;       
    bool is_cinquina;          
    bool done;                
    int turn; //turno a chi bisogna assegnarli la scheda
    int destinatario; //destinatario della scheda
    int waiting; //attendiamo che tutti ricevano la scheda e controllano la propria scheda per il numero estratto
    int counter_estrazioni; //serve a evitare di fare uscire le stesse estrazioni

    pthread_mutex_t lock;
    pthread_cond_t dealer, player;
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
    if (n > 1){
        for (int i = 0; i < n; i++){
            int j = rand() % n;
            int t = arr[i];
            arr[i] = arr[j];
            arr[j] = t;
        }
    }
}

int** init_card(){
    int** scheda = malloc(sizeof(int*) * ROW);
    for (int i = 0; i < ROW; i++){
        scheda[i] = calloc(COL, sizeof(int));
    }
    return scheda;
}

void copy_card(int** src, int** dest){
    for (int i = 0; i < ROW; i++){
        for (int j = 0; j < COL; j++){
            dest[i][j] = src[i][j];
        }
    }
}

void print_card(int** scheda){
    for (int i = 0; i < ROW; i++){
        if (i == 0)
            printf("(");
        else
            printf(" (");
        for (int j = 0; j < COL; j++){
            if (j == 0)
                printf("%d", scheda[i][j]);
            else
                printf(", %d", scheda[i][j]);
        }
        printf(")\n");
    }
}

void generate_card(int** scheda){
    int number[MAX_NUMBER];
    for (int i = 0; i < MAX_NUMBER; i++){
        number[i] = i + 1;
    }
    shuffle(number, MAX_NUMBER);
    int k = 0;
    for (int i = 0; i < ROW; i++){
        for (int j = 0; j < COL; j++){
            scheda[i][j] = number[k++];
        }
    }
}

int mark_number(int** scheda, int number){
    int counter_row = 0;
    for (int i = 0; i < ROW; i++){
        int counter_numeriestratti = 0;
        for (int j = 0; j < COL; j++){
            if (scheda[i][j] == number){
                scheda[i][j] = 0; // Assegna zero
            }
            if (scheda[i][j] == 0){
                counter_numeriestratti++;
            }
        }
        if (counter_numeriestratti == COL)
            counter_row++;
    }
    if (counter_row == ROW)
        return 1;  // Bingo
    else if (counter_row > 0)
        return 0;  // Cinquina
    return -1;
}

void free_card(int** scheda){
    for (int i = 0; i < ROW; i++){
        free(scheda[i]);
    }
    free(scheda);
}

shared* init_shared(int nplayers){
    shared* sh = malloc(sizeof(shared));

    sh->scheda = init_card();
    sh->done = sh->is_cinquina = false;
    sh->waiting = sh->counter_estrazioni = 0;
    sh->player_cinquina = sh->player_bingo =  sh->numero_estratto = sh->destinatario  =-1;
    sh->turn = nplayers;

    if(pthread_mutex_init(&sh->lock, NULL) != 0){
        perror("pthread_mutex_init");
        exit(EXIT_FAILURE);
    }

    if(pthread_cond_init(&sh->dealer, NULL) != 0){
        perror("pthread_cond_init");
        exit(EXIT_FAILURE);
    }

    if(pthread_cond_init(&sh->player, NULL) != 0){
        perror("pthread_cond_init");
        exit(EXIT_FAILURE);
    }
    return sh;
}

void shared_destroy(shared* sh){
    pthread_mutex_destroy(&sh->lock);
    pthread_cond_destroy(&sh->dealer);
    pthread_cond_destroy(&sh->player);
    free_card(sh->scheda);
    free(sh);
}

void dealer_function(void* arg){
    thread_dealer* td = (thread_dealer*)arg;

    printf("D: ci saranno %d giocatori con %d card ciascuno\n", td->nplayers, td->mcards);
    int counter_schede = 1;
    for(int i = 0; i < td->nplayers; i++){
        for(int j = 0; j < td->mcards; j++){
            if(pthread_mutex_lock(&td->sh->lock) != 0){
                perror("pthread_mutex_lock");
                exit(EXIT_FAILURE);
            }

            while(td->sh->turn != td->nplayers){
                if(pthread_cond_wait(&td->sh->dealer, &td->sh->lock) != 0){
                    perror("pthread_cond_wait");
                    exit(EXIT_FAILURE);
                }
            }

            generate_card(td->sh->scheda);
            printf("D: genero e distribuisco la card n.%d:\n", counter_schede++);
            print_card(td->sh->scheda);
            td->sh->destinatario = i;
            td->sh->turn = i;
            td->sh->waiting = 1;
            if(pthread_cond_broadcast(&td->sh->player) != 0){
                perror("pthread_cond_broadcast");
                exit(EXIT_FAILURE);
            }

            while(td->sh->waiting > 0){
                if(pthread_cond_wait(&td->sh->dealer, &td->sh->lock) != 0){
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

    printf("D fine della distribuzione delle schede e inzio di estrazione dei numeri\n");

    int number[MAX_NUMBER];
    for(int i = 0; i < MAX_NUMBER; i++){
        number[i] = i + 1;
    }
    shuffle(number, MAX_NUMBER);
    int k = 0;
    while(k < MAX_NUMBER && !td->sh->done){
        if(pthread_mutex_lock(&td->sh->lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }

        td->sh->numero_estratto = number[k++];
        td->sh->counter_estrazioni++;
        td->sh->waiting = td->nplayers;
        printf("D: numero estratto: %d\n", td->sh->numero_estratto);
        
        if(pthread_cond_broadcast(&td->sh->player) != 0){
            perror("pthread_cond_signal");
            exit(EXIT_FAILURE);
        }

        while(td->sh->waiting > 0){
            if(pthread_cond_wait(&td->sh->dealer, &td->sh->lock) != 0){
                perror("pthread_cond_wait");
                exit(EXIT_FAILURE);
            }
        }

        if(td->sh->player_bingo >= 0){
            printf("D: il giocatore n.%d ha vinto il bingo con la scheda:\n", td->sh->player_bingo);
            print_card(td->sh->scheda);
            td->sh->done = true;
        }
        else if(!td->sh->is_cinquina && td->sh->player_cinquina >= 0){
            printf("D: il giocatore n.%d ha vinto la cinquina con la scheda:\n", td->sh->player_cinquina);
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

    for(int i = 0; i < td->nplayers; i++){
        if(pthread_cond_signal(&td->sh->player) != 0){
            perror("pthread_cond_signal");
            exit(EXIT_FAILURE);
        }
    }

    if(pthread_mutex_unlock(&td->sh->lock) != 0){
        perror("pthread_mutex_unlock");
        exit(EXIT_FAILURE);
    }
    printf("D: fine del gioco\n");
}

void player_function(void* arg){
    thread_player* td = (thread_player*)arg;

    int*** scheda = malloc(sizeof(int**) * td->mcards);
    int*** scheda_copy = malloc(sizeof(int**) * td->mcards);

    for(int i = 0; i < td->mcards; i++){
        scheda[i] = init_card();
        scheda_copy[i] = init_card();
    }

    for(int i = 0; i < td->mcards; i++){
        if(pthread_mutex_lock(&td->sh->lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }

        while(td->sh->destinatario != td->thread_n){
            if(pthread_cond_wait(&td->sh->player, &td->sh->lock) != 0){
                perror("pthread_cond_wait");
                exit(EXIT_FAILURE);
            }
        }

        copy_card(td->sh->scheda, scheda[i]);
        copy_card(td->sh->scheda, scheda_copy[i]);
        td->sh->waiting--;
        
        if(pthread_cond_signal(&td->sh->dealer) != 0){
            perror("pthread_cond_signal");
            exit(EXIT_FAILURE);
        }
        
        if(pthread_mutex_unlock(&td->sh->lock) != 0){
            perror("pthread_mutex_unlock");
            exit(EXIT_FAILURE);
        }

        printf("P%u: ricevuta scheda:\n", td->thread_n +1);
        print_card(scheda[i]);
    }

    int status_game = -1;
    int ultima_estrazione = 0;
    while(1){
        if(pthread_mutex_lock(&td->sh->lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        } 

        while(!td->sh->done && td->sh->counter_estrazioni == ultima_estrazione){
            if(pthread_cond_wait(&td->sh->player, &td->sh->lock) != 0){
                perror("pthread_cond_wait");
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

        ultima_estrazione = td->sh->counter_estrazioni;
        for(int i = 0; i < td->mcards; i++){
            int status = mark_number(scheda_copy[i], td->sh->numero_estratto);
            if(status != status_game){
                if(status == 1){
                    printf("[P%d] scheda con il Bingo:\n", td->thread_n + 1);
                    print_card(scheda[i]);
                    td->sh->player_bingo = td->thread_n;
                    copy_card(scheda[i], td->sh->scheda);
                    break;
                } else if(status == 0 && td->sh->player_cinquina < 0){
                    printf("[P%d] scheda con la cinquina:\n", td->thread_n + 1);
                    print_card(scheda[i]);
                    td->sh->player_cinquina = td->thread_n;
                    copy_card(scheda[i], td->sh->scheda);
                }
                status_game = status;
            }
        }
        
        td->sh->waiting--;
        if(pthread_cond_signal(&td->sh->dealer) != 0){
            perror("pthread_cond_signal");
            exit(EXIT_FAILURE);
        }

        if(pthread_mutex_unlock(&td->sh->lock) != 0){
            perror("pthread_mutex_Unlock");
            exit(EXIT_FAILURE);
        }
    }
    for (int i = 0; i < td->mcards; i++){
        free_card(scheda[i]);
        free_card(scheda_copy[i]);
    }
    free(scheda);
    free(scheda_copy);
}

int main(int argc, char** argv){
    if(argc != 3){
        fprintf(stderr, "Usage: %s <n> <m>\n", argv[0]);
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

    shared_destroy(sh);
    free(pl);
    free(dl);
}