#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdbool.h>
#include <unistd.h>
#include <time.h>

#define ROW 3
#define COL 5
#define MAX_NUMBER 75

// Struttura condivisa
typedef struct {
    int** scheda;            // Scheda condivisa (anche per comunicare la vincente)
    int current_number;      // Numero estratto attuale
    int player_id_bingo;     // ID del giocatore che ha fatto Bingo
    int player_id_cinquina;  // ID del giocatore che ha fatto cinquina
    bool is_cinquina;        // Flag che indica se la cinquina è già stata vinta
    bool done;               // Flag per terminare il gioco
    int turn;                // Indica a chi tocca (durante la distribuzione); valore = n_player indica che non c’è distribuzione in corso
    int waiting;             // Contatore per le conferme (sia in distribuzione che in estrazione)
    int extraction_event;    // Contatore per distinguere gli eventi di estrazione

    pthread_mutex_t lock;
    pthread_cond_t* cond_p;  // Array di variabili di condizione per i giocatori
    pthread_cond_t cond_d;   // Variabile di condizione per il dealer
} shared;

// Struttura per il thread giocatore
typedef struct {
    int n_card;        // Numero di schede possedute
    unsigned thread_n; // ID del giocatore (0-indexed)
    pthread_t tid;
    shared* sh;
} thread_player;

// Struttura per il thread dealer
typedef struct {
    int n_card;    // Numero di schede per giocatore
    int n_player;  // Numero totale di giocatori
    shared* sh;
    pthread_t tid;
} thread_dealer;

// Funzioni di gestione della scheda
int** init_card(){
    int** scheda = malloc(sizeof(int*) * ROW);
    for(int i = 0; i < ROW; i++){
        scheda[i] = calloc(COL, sizeof(int));
    }
    return scheda;
}

void free_card(int** scheda){
    for(int i = 0; i < ROW; i++){
        free(scheda[i]);
    }
    free(scheda);
}

void shuffle(int* arr, int n){
    if(n > 1){
        for(int i = 0; i < n; i++){
            int j = rand() % n;
            int t = arr[i];
            arr[i] = arr[j];
            arr[j] = t;
        }
    }
}

void generate_card(int** scheda){
    int numbers[MAX_NUMBER];
    for(int i = 0; i < MAX_NUMBER; i++){
        numbers[i] = i + 1;
    }
    shuffle(numbers, MAX_NUMBER);
    int k = 0;
    for(int i = 0; i < ROW; i++){
        for(int j = 0; j < COL; j++){
            scheda[i][j] = numbers[k++];
        }
    }
}

void copy_card(int** src, int** dest){
    for(int i = 0; i < ROW; i++){
        for(int j = 0; j < COL; j++){
            dest[i][j] = src[i][j];
        }
    }
}

int mark_number(int** scheda, int number){
    int cinquina_counter = 0;
    for(int i = 0; i < ROW; i++){
        int row_counter = 0;
        for(int j = 0; j < COL; j++){
            if(scheda[i][j] == number){
                scheda[i][j] = 0;
            }
            if(scheda[i][j] == 0){
                row_counter++;
            }
        }
        if(row_counter == COL){
            cinquina_counter++;
        }
    }
    if(cinquina_counter == ROW)
        return 1;  // Bingo
    else if(cinquina_counter > 0)
        return 0;  // Cinquina
    return -1;
}

void print_card(int** card){
    for(int i = 0; i < ROW; i++){
        if(i == 0)
            printf("(");
        else
            printf(" (");
        for(int j = 0; j < COL; j++){
            if(j == 0)
                printf("%d", card[i][j]);
            else
                printf(",%d", card[i][j]);
        }
        printf(")\n");
    }
}

// Inizializza la struttura condivisa
shared* init_shared(int n_player){
    shared* sh = malloc(sizeof(shared));
    sh->turn = n_player;  // Inizialmente nessun giocatore sta ricevendo una scheda
    sh->scheda = init_card();
    sh->player_id_bingo = sh->player_id_cinquina = -1;
    sh->done = sh->is_cinquina = false;
    sh->current_number = -1;
    sh->waiting = 0;
    sh->extraction_event = 0;

    sh->cond_p = malloc(sizeof(pthread_cond_t) * n_player);
    for(int i = 0; i < n_player; i++){
        if(pthread_cond_init(&sh->cond_p[i], NULL) != 0){
            perror("pthread_cond_init");
            exit(EXIT_FAILURE);
        }
    }
    if(pthread_cond_init(&sh->cond_d, NULL) != 0){
        perror("pthread_cond_init");
        exit(EXIT_FAILURE);
    }
    if(pthread_mutex_init(&sh->lock, NULL) != 0){
        perror("pthread_mutex_init");
        exit(EXIT_FAILURE);
    }
    return sh;
}

void shared_destroy(shared* sh, int n_player){
    for(int i = 0; i < n_player; i++){
        pthread_cond_destroy(&sh->cond_p[i]);
    }
    free(sh->cond_p);
    free_card(sh->scheda);
    pthread_cond_destroy(&sh->cond_d);
    pthread_mutex_destroy(&sh->lock);
    free(sh);
}

// Thread dealer: distribuisce le schede e poi estrae i numeri
void* dealer_thread(void* arg){
    thread_dealer* td = (thread_dealer*) arg;

    printf("[D] ci saranno %d giocatori con %d schede ciascuno\n", td->n_player, td->n_card);

    int card_counter = 1;
    // Fase di distribuzione
    for (int i = 0; i < td->n_player; i++){
        for (int j = 0; j < td->n_card; j++){
            if (pthread_mutex_lock(&td->sh->lock) != 0){
                perror("pthread_mutex_lock");
                exit(EXIT_FAILURE);
            }
            // Attende che la precedente distribuzione sia terminata (turn == n_player)
            while (td->sh->turn != td->n_player) {
                if (pthread_cond_wait(&td->sh->cond_d, &td->sh->lock) != 0){
                    perror("pthread_cond_wait");
                    exit(EXIT_FAILURE);
                }
            }
            generate_card(td->sh->scheda);
            printf("[D] genero e distribuisco la scheda n.%d:\n", card_counter++);
            print_card(td->sh->scheda);

            td->sh->turn = i;   // Indica al giocatore i che deve ricevere la scheda
            td->sh->waiting = 1;

            if (pthread_cond_signal(&td->sh->cond_p[i]) != 0){
                perror("pthread_cond_signal");
                exit(EXIT_FAILURE);
            }
            while(td->sh->waiting > 0){
                if (pthread_cond_wait(&td->sh->cond_d, &td->sh->lock) != 0){
                    perror("pthread_cond_wait");
                    exit(EXIT_FAILURE);
                }
            }
            td->sh->turn = td->n_player;  // Ripristina il valore per la prossima distribuzione
            if(pthread_mutex_unlock(&td->sh->lock) != 0){
                perror("pthread_mutex_unlock");
                exit(EXIT_FAILURE);
            }
        }
    }

    printf("[D] Fine della distribuzione delle schede. Inizio estrazione dei numeri.\n");

    int number[MAX_NUMBER];
    for (int i = 0; i < MAX_NUMBER; i++){
        number[i] = i + 1;
    }
    shuffle(number, MAX_NUMBER);
    int k = 0;
    // Fase di estrazione
    while(!td->sh->done && k < MAX_NUMBER){
        if(pthread_mutex_lock(&td->sh->lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }
        td->sh->current_number = number[k++];
        td->sh->extraction_event++;  // Nuovo evento di estrazione
        td->sh->waiting = td->n_player;
        printf("[D] Numero estratto: %d\n", td->sh->current_number);

        // Notifica tutti i giocatori dell'evento di estrazione
        for (int i = 0; i < td->n_player; i++){
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
        // Verifica eventuali vincitori
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
    // Notifica a tutti la terminazione
    if(pthread_mutex_lock(&td->sh->lock) != 0){
        perror("pthread_mutex_lock");
        exit(EXIT_FAILURE);
    }
    td->sh->done = true;
    for (int i = 0; i < td->n_player; i++){
        pthread_cond_signal(&td->sh->cond_p[i]);
    }
    if(pthread_mutex_unlock(&td->sh->lock) != 0){
        perror("pthread_mutex_unlock");
        exit(EXIT_FAILURE);
    }
    printf("[D] Fine del gioco.\n");
    return NULL;
}

// Thread giocatore
void* player_thread(void* arg){
    thread_player* tp = (thread_player*) arg;
    int n = tp->n_card;

    // Allocazione delle schede locali: copia originale e copia per il marking
    int*** card = malloc(sizeof(int**) * n);
    int*** card_copy = malloc(sizeof(int**) * n);
    for (int i = 0; i < n; i++){
        card[i] = init_card();
        card_copy[i] = init_card();
    }

    // Fase di distribuzione: ricezione di n schede
    for (int i = 0; i < n; i++){
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
        copy_card(tp->sh->scheda, card[i]);
        copy_card(tp->sh->scheda, card_copy[i]);
        tp->sh->waiting--;
        if(pthread_cond_signal(&tp->sh->cond_d) != 0){
            perror("pthread_cond_signal");
            exit(EXIT_FAILURE);
        }
        if(pthread_mutex_unlock(&tp->sh->lock) != 0){
            perror("pthread_mutex_unlock");
            exit(EXIT_FAILURE);
        }
        printf("[P%d] ricevuta scheda:\n", tp->thread_n + 1);
        print_card(card[i]);
    }

    int status_game = -1;
    int last_extraction = 0;
    // Fase di estrazione: il giocatore elabora ogni nuovo numero estratto
    while(1){
        if(pthread_mutex_lock(&tp->sh->lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }
        while(!tp->sh->done && tp->sh->extraction_event == last_extraction){
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
        last_extraction = tp->sh->extraction_event;
        int extracted = tp->sh->current_number;
        if(pthread_mutex_unlock(&tp->sh->lock) != 0){
            perror("pthread_mutex_unlock");
            exit(EXIT_FAILURE);
        }
        // Elaborazione del numero estratto su ogni scheda
        for (int i = 0; i < n; i++){
            int status = mark_number(card_copy[i], extracted);
            if(status != status_game){
                if(status == 1){
                    printf("[P%d] scheda con il Bingo:\n", tp->thread_n + 1);
                    print_card(card[i]);
                    if(pthread_mutex_lock(&tp->sh->lock) != 0){
                        perror("pthread_mutex_lock");
                        exit(EXIT_FAILURE);
                    }
                    tp->sh->player_id_bingo = tp->thread_n;
                    copy_card(card[i], tp->sh->scheda);
                    if(pthread_mutex_unlock(&tp->sh->lock) != 0){
                        perror("pthread_mutex_unlock");
                        exit(EXIT_FAILURE);
                    }
                    break;
                } else if(status == 0 && tp->sh->player_id_cinquina < 0){
                    printf("[P%d] scheda con la cinquina:\n", tp->thread_n + 1);
                    print_card(card[i]);
                    if(pthread_mutex_lock(&tp->sh->lock) != 0){
                        perror("pthread_mutex_lock");
                        exit(EXIT_FAILURE);
                    }
                    tp->sh->player_id_cinquina = tp->thread_n;
                    copy_card(card[i], tp->sh->scheda);
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

    for (int i = 0; i < n; i++){
        free_card(card[i]);
        free_card(card_copy[i]);
    }
    free(card);
    free(card_copy);
    return NULL;
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
    for (int i = 0; i < n_player; i++){
        pl[i].thread_n = i;
        pl[i].n_card = m_card;
        pl[i].sh = sh;
        if(pthread_create(&pl[i].tid, NULL, player_thread, &pl[i]) != 0){
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }

    thread_dealer* dealer = malloc(sizeof(thread_dealer));
    dealer->n_player = n_player;
    dealer->n_card = m_card;
    dealer->sh = sh;
    if(pthread_create(&dealer->tid, NULL, dealer_thread, dealer) != 0){
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    if(pthread_join(dealer->tid, NULL) != 0){
        perror("pthread_join");
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < n_player; i++){
        if(pthread_join(pl[i].tid, NULL) != 0){
            perror("pthread_join");
            exit(EXIT_FAILURE);
        }
    }
    free(pl);
    shared_destroy(sh, n_player);
    free(dealer);
}
