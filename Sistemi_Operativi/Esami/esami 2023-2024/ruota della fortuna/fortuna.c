#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <pthread.h>
#include <unistd.h>
#include <ctype.h>
#include <stdbool.h>

#define MAX_FRASE_SIZE 100
#define ALFABETO_SIZE 26

typedef struct{
    char frase_da_scoprire[MAX_FRASE_SIZE]; // frase con lettere oscurate
    int contatore_lettere[ALFABETO_SIZE];   // tiene traccia delle lettere già chiamate
    int n_giocatori;                        // numero di giocatori
    int *punteggi;                          // punteggio per ogni giocatore
    char lettera;                           // ultima lettera scelta
    bool done;                              // flag di fine gioco
    int letter_ready;
    int current_player;                     // player attuale che sta giocando
    int player_ready;                       // giocatori pronti

    // Variabili di sincronizzazione condivise
    pthread_mutex_t lock;
    pthread_cond_t cond_turn;       // segnala al giocatore che è il suo turno
    pthread_cond_t cond_letter;     // notifica che il giocatore ha scelto la lettera
    pthread_cond_t cond_all_ready;  // fa attendere finché tutti i giocatori non sono pronti
} tabellone;

typedef struct{
    pthread_t tid;
    unsigned thread_n;
    tabellone* tab;
} thread_player;

void reset(int* arr, int n){
    for(int i = 0; i < n; i++){
        arr[i] = 0;
    }
}

tabellone* init_tab(int n_giocatori){
    tabellone* tab = malloc(sizeof(tabellone));
    if(tab == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    tab->n_giocatori = n_giocatori;
    tab->letter_ready = 0;
    tab->player_ready = 0;
    tab->done = false;
    tab->current_player = -1;
    tab->punteggi = calloc(n_giocatori, sizeof(int));
    if(tab->punteggi == NULL) {
        perror("calloc");
        exit(EXIT_FAILURE);
    }
    reset(tab->contatore_lettere, ALFABETO_SIZE);

    if(pthread_mutex_init(&tab->lock, NULL) != 0){
        perror("pthread_mutex_init");
        exit(EXIT_FAILURE);
    }

    if(pthread_cond_init(&tab->cond_turn, NULL) != 0){
        perror("pthread_cond_init");
        exit(EXIT_FAILURE);
    }

    if(pthread_cond_init(&tab->cond_letter, NULL) != 0){
        perror("pthread_cond_init");
        exit(EXIT_FAILURE);
    }

    if(pthread_cond_init(&tab->cond_all_ready, NULL) != 0){
        perror("pthread_cond_init");
        exit(EXIT_FAILURE);
    }

    return tab;
}

void shared_destroy(tabellone* tab){
    pthread_mutex_destroy(&tab->lock);
    pthread_cond_destroy(&tab->cond_turn);
    pthread_cond_destroy(&tab->cond_letter);
    pthread_cond_destroy(&tab->cond_all_ready);
    free(tab->punteggi);
    free(tab);
}

void shuffle(int* arr, int n){
    if(n > 1){
        for(int i = 0; i < n; i++){
            int j = rand() % n;
            int t = arr[j];
            arr[j] = arr[i];
            arr[i] = t;
        }
    }
}

char seleziona_lettera(int* counter_letter){
    int j = 0;
    int lettere_non_chiamate[ALFABETO_SIZE];

    for(int i = 0; i < ALFABETO_SIZE; i++){
        if(counter_letter[i] == 0){
            lettere_non_chiamate[j++] = i; 
        }
    }

    if(j == 0){
        return 0;
    }
    int indice_letter = lettere_non_chiamate[rand() % j];
    return 'A' + indice_letter;
}

void nascondi_lettera(char* dest_par, const char* original_str){
    int len = strlen(original_str);
    for(int i = 0; i < len; i++){
        if(isalpha((unsigned char)original_str[i])){
            dest_par[i] = '#';
        } else {
            dest_par[i] = original_str[i];
        }
    }
    dest_par[len] = '\0';
}

int mostra_lettera(char* dest_par, const char* original_str, char c){
    int j = 0;
    int len = strlen(original_str);
    for(int i = 0; i < len; i++){
        if(original_str[i] == c){
            dest_par[i] = c;
            j++;
        }
    } 
    return j;
}

int tab_complete(const char* str){
    return strchr(str, '#') == NULL;
}

int argmax(int* arr, int n){
    int index = -1;
    int max = 0;
    for(int i = 0; i < n; i++){
        if(arr[i] > max){
            max = arr[i];
            index = i;
        }
    }
    return index;
}

void* player_thread(void* arg){
    thread_player* td = (thread_player*)arg;

    if(pthread_mutex_lock(&td->tab->lock) != 0){
        perror("pthread_mutex_lock");
        exit(EXIT_FAILURE);
    }
    
    // Incrementa il contatore dei giocatori pronti
    td->tab->player_ready++;
    if(td->tab->player_ready == td->tab->n_giocatori){
        if(pthread_cond_signal(&td->tab->cond_all_ready) != 0){
            perror("pthread_cond_signal");
            exit(EXIT_FAILURE);
        }
    }

    if(pthread_mutex_unlock(&td->tab->lock) != 0){
        perror("pthread_mutex_unlock");
        exit(EXIT_FAILURE);
    }

    printf("[G%d] avviato e pronto\n", td->thread_n + 1);

    while(1){
        if(pthread_mutex_lock(&td->tab->lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }

        while(!td->tab->done && td->tab->current_player != td->thread_n){
            if(pthread_cond_wait(&td->tab->cond_turn, &td->tab->lock) != 0){
                perror("pthread_cond_wait");
                exit(EXIT_FAILURE);
            }
        }

        if(td->tab->done){
            if(pthread_mutex_unlock(&td->tab->lock) != 0){
                perror("pthread_mutex_unlock");
                exit(EXIT_FAILURE);
            }
            break;
        }

        char lettera = seleziona_lettera(td->tab->contatore_lettere);
        td->tab->lettera = lettera;
        printf("[G%d] scelgo la lettera '%c'\n", td->thread_n + 1, lettera);
        td->tab->letter_ready = 1;
        // Segnala al main che la lettera è pronta
        if(pthread_cond_signal(&td->tab->cond_letter) != 0){
            perror("pthread_cond_signal");
            exit(EXIT_FAILURE);
        }
        if(pthread_mutex_unlock(&td->tab->lock) != 0){
            perror("pthread_mutex_unlock");
            exit(EXIT_FAILURE);
        }
    }
    return NULL;
}

int main(int argc, char** argv){
    if(argc < 4){
        fprintf(stderr, "Usage: %s <n-numero-giocatori> <m-numero-partite> <file.txt>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int n_giocatori = atoi(argv[1]);
    int m_partite = atoi(argv[2]);

    tabellone* tab = init_tab(n_giocatori);
    FILE* f;
    char buffer[MAX_FRASE_SIZE];

    if((f = fopen(argv[3], "r")) == NULL){
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    int n_frasi = 0;
    while(!feof(f)){
        if(fgetc(f) == '\n'){
            n_frasi++;
        }
    }
    fseek(f, 0, SEEK_SET);

    if(n_frasi < m_partite){
        fprintf(stderr, "Numero di frasi inferiore al numero delle partite\n");
        exit(EXIT_FAILURE);
    }

    char** elenco_frasi = malloc(sizeof(char*) * n_frasi);
    if(elenco_frasi == NULL){
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    int j = 0;
    while(fgets(buffer, MAX_FRASE_SIZE, f)){
        int len = strlen(buffer);
        if(len > 0 && buffer[len - 1] == '\n'){
            buffer[len - 1] = '\0';
            len--;
        }
        for(int i = 0; i < len; i++){
            buffer[i] = toupper((unsigned char)buffer[i]);
        }
        elenco_frasi[j++] = strdup(buffer);
    }
    fclose(f);
    printf("[M] lette %d possibili frasi da indovinare per %d partite\n", n_frasi, m_partite);

    thread_player* player = malloc(sizeof(thread_player) * n_giocatori);
    if(player == NULL){
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    for(int i = 0; i < n_giocatori; i++){
        player[i].thread_n = i;
        player[i].tab = tab;
        if(pthread_create(&player[i].tid, NULL, player_thread, &player[i]) != 0){
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }

    if(pthread_mutex_lock(&tab->lock) != 0){
        perror("pthread_mutex_lock");
        exit(EXIT_FAILURE);
    }

    while(tab->player_ready < n_giocatori){
        if(pthread_cond_wait(&tab->cond_all_ready, &tab->lock) != 0){
            perror("pthread_cond_wait");
            exit(EXIT_FAILURE);
        }
    }

    if(pthread_mutex_unlock(&tab->lock) != 0){
        perror("pthread_mutex_unlock");
        exit(EXIT_FAILURE);
    }
    printf("[M] tutti i giocatori sono pronti, prossimo iniziare!\n");

    int* frasi_indici = malloc(n_frasi * sizeof(int));
    if(frasi_indici == NULL){
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    for(int i = 0;  i < n_frasi; i++){
        frasi_indici[i] = i;
    }
    shuffle(frasi_indici, n_frasi);
    int frase_indice_corrente = 0;

    for(int partita = 0; partita < m_partite; partita++){
        char* frase_originale = elenco_frasi[frasi_indici[frase_indice_corrente++]];
        printf("[M] scelto la frase \"%s\" per la partita n:%d\n", frase_originale, partita + 1);
        
        nascondi_lettera(tab->frase_da_scoprire, frase_originale);
        reset(tab->contatore_lettere, ALFABETO_SIZE);

        int current_player = 0;
        while(!tab_complete(tab->frase_da_scoprire)){
            if(pthread_mutex_lock(&tab->lock) != 0){
                perror("pthread_mutex_lock");
                exit(EXIT_FAILURE);
            }

            tab->current_player = current_player;
            tab->letter_ready = 0;

            if(pthread_cond_broadcast(&tab->cond_turn) != 0){
                perror("pthread_cond_broadcast");
                exit(EXIT_FAILURE);
            }

            while(tab->letter_ready == 0){
                if(pthread_cond_wait(&tab->cond_letter, &tab->lock) != 0){
                    perror("pthread_cond_wait");
                    exit(EXIT_FAILURE);
                }
            }

            char lettera = tab->lettera;
            tab->contatore_lettere[lettera - 'A']++;
            int lettere_trovate = mostra_lettera(tab->frase_da_scoprire, frase_originale, lettera);
            int punteggio_base = (rand() % 4 + 1) * 100;
            int punteggio = punteggio_base * lettere_trovate;
            tab->punteggi[current_player] += punteggio;

            if(lettere_trovate == 0){
                printf("[M] nessuna occorrenza per %c\n", lettera);
                current_player = (current_player + 1) % n_giocatori;
            } else {
                printf("[M] ci sono %d occorrenze per %c; assegmati %d x %d = %d\n",
                       lettere_trovate, lettera, punteggio_base, lettere_trovate, punteggio);
            }
            printf("[M] tabellone: %s\n", tab->frase_da_scoprire);
            if(pthread_mutex_unlock(&tab->lock) != 0){
                perror("pthread_mutex_unlock");
                exit(EXIT_FAILURE);
            }
        }

        if(pthread_mutex_lock(&tab->lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }
        printf("[M] frase completata; punteggi attuali: ");
        for(int i = 0; i < n_giocatori; i++){
            printf("G%d: %d ", i + 1, tab->punteggi[i]);
        }
        printf("\n");
        if(pthread_mutex_unlock(&tab->lock) != 0){
            perror("pthread_mutex_unlock");
            exit(EXIT_FAILURE);
        }
    }

    if(pthread_mutex_lock(&tab->lock) != 0){
        perror("pthread_mutex_lock");
        exit(EXIT_FAILURE);
    }

    tab->done = true;

    if(pthread_cond_broadcast(&tab->cond_turn) != 0){
        perror("pthread_cond_broadcast");
        exit(EXIT_FAILURE);
    }

    if(pthread_mutex_unlock(&tab->lock) != 0){
        perror("pthread_mutex_unlock");
        exit(EXIT_FAILURE);
    }

    int winner = argmax(tab->punteggi, n_giocatori);
    printf("[M] questa era l'ultima partita: il vincitore e' G%d\n", winner + 1);

    for(int i = 0; i < n_giocatori; i++){
        if(pthread_join(player[i].tid, NULL) != 0){
            perror("pthread_join");
            exit(EXIT_FAILURE);
        }
    }
    free(player);
    free(frasi_indici);
    for(int i = 0; i < n_frasi; i++){
        free(elenco_frasi[i]);
    }
    free(elenco_frasi);

    shared_destroy(tab);
    return 0;
}
