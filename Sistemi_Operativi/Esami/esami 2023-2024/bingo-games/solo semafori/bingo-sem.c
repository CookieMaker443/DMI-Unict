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

#define CARD_COLS 5
#define CARD_ROWS 3
#define MAX_NUMBERS 75

// Struttura dati condivisa
typedef struct {
    int **card;               // Matrice della carta condivisa
    int current_number;       // Numero attualmente estratto
    int player_id_bingo;      // ID del giocatore che ha fatto Bingo
    int player_id_quintet;    // ID del giocatore che ha fatto cinquina
    bool is_quintet;          // Flag per cinquina già vinta
    sem_t *read_sems;         // Array di semafori per notificare ai giocatori
    sem_t write_sem;          // Semaforo per il dealer
    bool done;                // Flag per terminare il gioco
} shared_data_t;

// Struttura dati per i giocatori
typedef struct {
    int id;
    int n_cards;
    shared_data_t *sh;
    pthread_t thread_id;
} player_data_t;

// Struttura dati per il dealer
typedef struct {
    int n_players;
    int n_cards;
    shared_data_t *sh;
    pthread_t thread_id;
} dealer_data_t;

// Funzioni di utilità per la gestione della carta

// Inizializza una matrice per la carta
int **initialize_card() {
    int **card = (int **)malloc(sizeof(int *) * CARD_ROWS);
    for (int i = 0; i < CARD_ROWS; i++) {
        card[i] = calloc(CARD_COLS, sizeof(int));
    }
    return card;
}

void free_card(int **card) {
    for (int i = 0; i < CARD_ROWS; i++) {
        free(card[i]);
    }
    free(card);
}

// Mescola casualmente un array
void shuffle(int *array, int n) {
    if (n > 1) {
        for (int i = 0; i < n; i++) {
            int j = rand() % n;
            int temp = array[i];
            array[i] = array[j];
            array[j] = temp;
        }
    }
}

// Genera una carta con numeri unici
void generate_card(int **card) {
    int numbers[MAX_NUMBERS];
    for (int i = 0; i < MAX_NUMBERS; i++) {
        numbers[i] = i + 1;
    }
    shuffle(numbers, MAX_NUMBERS);
    int k = 0;
    for (int i = 0; i < CARD_ROWS; i++) {
        for (int j = 0; j < CARD_COLS; j++) {
            card[i][j] = numbers[k++];
        }
    }
}

// Copia il contenuto di una carta in un’altra
void copy_card(int **src, int **dest) {
    for (int i = 0; i < CARD_ROWS; i++) {
        for (int j = 0; j < CARD_COLS; j++) {
            dest[i][j] = src[i][j];
        }
    }
}

// Marca il numero estratto sulla carta; restituisce 1 per Bingo, 0 per cinquina, -1 altrimenti.
int mark_number_on_card(int **card, int number) {
    int quintet_count = 0;
    for (int i = 0; i < CARD_ROWS; i++) {
        int row_marked = 0;
        for (int j = 0; j < CARD_COLS; j++) {
            if (card[i][j] == number) {
                card[i][j] = 0;
            }
            if (card[i][j] == 0) {
                row_marked++;
            }
        }
        if (row_marked == CARD_COLS) {
            quintet_count++;
        }
    }
    if (quintet_count == CARD_ROWS)
        return 1; // Bingo
    else if (quintet_count > 0)
        return 0; // Cinquina
    else
        return -1;
}

// Stampa una carta
void print_card(int **card) {
    for (int i = 0; i < CARD_ROWS; i++) {
        if (i == 0)
            printf("(");
        else
            printf(" (");
        for (int j = 0; j < CARD_COLS; j++) {
            if (j == 0)
                printf("%d", card[i][j]);
            else
                printf(",%d", card[i][j]);
        }
        printf(")\n");
    }
}

// Funzioni per l'inizializzazione e distruzione della struttura condivisa

shared_data_t *init_shared_data(int n_players) {
    shared_data_t *sd = malloc(sizeof(shared_data_t));
    sd->done = false;
    sd->current_number = -1;
    sd->player_id_bingo = -1;
    sd->player_id_quintet = -1;
    sd->is_quintet = false;
    sd->card = initialize_card();
    sd->read_sems = malloc(sizeof(sem_t) * n_players);
    for (int i = 0; i < n_players; i++) {
        if (sem_init(&sd->read_sems[i], 0, 0) != 0) {
            perror("sem_init read_sems");
            exit(EXIT_FAILURE);
        }
    }
    if (sem_init(&sd->write_sem, 0, 0) != 0) {
        perror("sem_init write_sem");
        exit(EXIT_FAILURE);
    }
    return sd;
}

void destroy_shared_data(shared_data_t *sd, int n_players) {
    free_card(sd->card);
    for (int i = 0; i < n_players; i++) {
        sem_destroy(&sd->read_sems[i]);
    }
    free(sd->read_sems);
    sem_destroy(&sd->write_sem);
    free(sd);
}

// Funzione del thread giocatore
void player_thread(void *arg) {
    player_data_t *pd = (player_data_t *)arg;

    // Alloca le cartelle (originali e copia per segnare i numeri)
    int ***cards = malloc(sizeof(int **) * pd->n_cards);
    int ***flag_cards = malloc(sizeof(int **) * pd->n_cards);
    for (int i = 0; i < pd->n_cards; i++) {
        cards[i] = initialize_card();
        flag_cards[i] = initialize_card();
    }

    // Ricezione delle cartelle dal dealer
    for (int i = 0; i < pd->n_cards; i++) {
        if (sem_wait(&pd->sh->read_sems[pd->id]) != 0) {
            perror("sem_wait read_sems");
            exit(EXIT_FAILURE);
        }
        copy_card(pd->sh->card, cards[i]);
        copy_card(pd->sh->card, flag_cards[i]);
        if (sem_post(&pd->sh->write_sem) != 0) {
            perror("sem_post write_sem");
            exit(EXIT_FAILURE);
        }
        printf("[P%d]: ricevuta card ", pd->id + 1);
        print_card(cards[i]);
    }

    int current_status = -1;
    while (!pd->sh->done) {
        if (sem_wait(&pd->sh->read_sems[pd->id]) != 0) {
            perror("sem_wait read_sems");
            exit(EXIT_FAILURE);
        }
        if (pd->sh->done) {
            break;
        }
        for (int i = 0; i < pd->n_cards; i++) {
            int status = mark_number_on_card(flag_cards[i], pd->sh->current_number);
            if (status != current_status) {
                if (status == 1) {
                    printf("[P%d] card con Bingo\n ", pd->id + 1);
                    print_card(cards[i]);
                    pd->sh->player_id_bingo = pd->id;
                    copy_card(cards[i], pd->sh->card);
                    break;
                } else if (status == 0 && !pd->sh->is_quintet) { //se status è uguale a zero e non è stata fatta ancora la cinquina
                    printf("[P%d] card con cinquina\n ", pd->id + 1);
                    print_card(cards[i]);
                    pd->sh->player_id_quintet = pd->id;
                    copy_card(cards[i], pd->sh->card);
                }
                current_status = status;
            }
        }
        if (sem_post(&pd->sh->write_sem) != 0) {
            perror("sem_post write_sem");
            exit(EXIT_FAILURE);
        }
    }
    for (int i = 0; i < pd->n_cards; i++) {
        free_card(cards[i]);
        free_card(flag_cards[i]);
    }
    free(cards);
    free(flag_cards);
}

// Funzione del thread dealer
void dealer_thread(void *arg) {
    dealer_data_t *dd = (dealer_data_t *)arg;
    printf("[D] ci saranno %d giocatori con %d card ciascuno\n", dd->n_players, dd->n_cards);

    int card_counter = 1;
    // Distribuzione delle cartelle ai giocatori
    for (int i = 0; i < dd->n_players; i++) {
        for (int j = 0; j < dd->n_cards; j++) {
            generate_card(dd->sh->card);
            printf("[D] genero e distribuisco la card n.%d ", card_counter++);
            print_card(dd->sh->card);
            if (sem_post(&dd->sh->read_sems[i]) != 0) {
                perror("sem_post read_sems");
                exit(EXIT_FAILURE);
            }
            if (sem_wait(&dd->sh->write_sem) != 0) {
                perror("sem_wait write_sem");
                exit(EXIT_FAILURE);
            }
        }
    }
    printf("[D] fine della distribuzione delle card e inizio di estrazione dei numeri\n");

    // Preparazione per l'estrazione dei numeri
    int numbers[MAX_NUMBERS];
    for (int i = 0; i < MAX_NUMBERS; i++) {
        numbers[i] = i + 1;
    }
    shuffle(numbers, MAX_NUMBERS);
    int k = 0;
    while (!dd->sh->done && k < MAX_NUMBERS) {
        dd->sh->player_id_bingo = -1;
        dd->sh->player_id_quintet = -1;
        dd->sh->current_number = numbers[k++];
        printf("[D] numero estratto: %d\n", dd->sh->current_number);
        // Notifica a tutti i giocatori il nuovo numero
        for (int i = 0; i < dd->n_players; i++) {
            if (sem_post(&dd->sh->read_sems[i]) != 0) {
                perror("sem_post read_sems");
                exit(EXIT_FAILURE);
            }
            if (sem_wait(&dd->sh->write_sem) != 0) {
                perror("sem_wait write_sem");
                exit(EXIT_FAILURE);
            }
            if (dd->sh->player_id_bingo >= 0) {
                printf("[D] il giocatore n.%d ha vinto il Bingo con la scheda\n ", dd->sh->player_id_bingo + 1);
                print_card(dd->sh->card);
                dd->sh->done = true;
                break;
            } else if (!dd->sh->is_quintet && dd->sh->player_id_quintet >= 0) {
                printf("[D] il giocatore n.%d ha vinto la cinquina con la scheda\n ", dd->sh->player_id_quintet + 1);
                print_card(dd->sh->card);
                dd->sh->is_quintet = true;
            }
        }
    }
    printf("[D] Fine del gioco\n");
    // Notifica di terminazione a tutti i giocatori
    for (int i = 0; i < dd->n_players; i++) {
        if (sem_post(&dd->sh->read_sems[i]) != 0) {
            perror("sem_post read_sems termination");
            exit(EXIT_FAILURE);
        }
    }
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <n_players> <n_cards>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    int n_players = atoi(argv[1]);
    int n_cards = atoi(argv[2]);

    srand(time(NULL));
    shared_data_t *sd = init_shared_data(n_players);

    // Creazione dei thread giocatori
    player_data_t **players = malloc(sizeof(player_data_t *) * n_players);
    for (int i = 0; i < n_players; i++) {
        players[i] = malloc(sizeof(player_data_t));
        players[i]->id = i;
        players[i]->n_cards = n_cards;
        players[i]->sh = sd;
        if (pthread_create(&players[i]->thread_id, NULL, (void*)player_thread, players[i]) != 0) {
            perror("pthread_create player");
            exit(EXIT_FAILURE);
        }
    }
    // Creazione del thread dealer
    dealer_data_t *dealer = malloc(sizeof(dealer_data_t));
    if (!dealer) {
        perror("malloc dealer_data");
        exit(EXIT_FAILURE);
    }
    dealer->n_players = n_players;
    dealer->n_cards = n_cards;
    dealer->sh = sd;
    if (pthread_create(&dealer->thread_id, NULL, (void*)dealer_thread, dealer) != 0) {
        perror("pthread_create dealer");
        exit(EXIT_FAILURE);
    }
    // Attesa del termine del thread dealer
    if (pthread_join(dealer->thread_id, NULL) != 0) {
        perror("pthread_join dealer");
        exit(EXIT_FAILURE);
    }
    // Attesa della terminazione dei thread giocatori
    for (int i = 0; i < n_players; i++) {
        if (pthread_join(players[i]->thread_id, NULL) != 0) {
            perror("pthread_join player");
            exit(EXIT_FAILURE);
        }
        free(players[i]);
    }
    free(players);
    destroy_shared_data(sd, n_players);
    free(dealer);
}
