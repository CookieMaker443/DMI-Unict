#define _GNU_SOURCE 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>
#include <ctype.h>

#define BUFFER_SIZE 100
#define ALFABETO_SIZE 26

typedef struct{
    char frase_da_scoprire[BUFFER_SIZE];
    int lettere_chiamate[ALFABETO_SIZE];
    int* punteggi;
    char temp_letter;
    bool done;

    pthread_mutex_t lock;
    pthread_cond_t cond_player, cond_turn, cond_letter;

    int id_player;       // ID del giocatore a cui tocca fare la scelta
    int letter_ready;    // Flag: 1 se il giocatore ha già scelto la lettera
    int player_ready;    // Numero di giocatori pronti
} shared;

typedef struct{
    pthread_t tid;
    unsigned thread_n;
    int nplayer;
    shared* sh;
} thread_gi;

void reset(int* arr, int n){
    for(int i = 0; i < n; i++){
        arr[i] = 0;
    }
}

shared* init_shared(int nplayer){
    shared* sh = malloc(sizeof(shared));

    sh->punteggi = calloc(nplayer, sizeof(int));
    sh->done = sh->letter_ready = sh->player_ready = 0;
    sh->id_player = -1;
    reset(sh->lettere_chiamate, ALFABETO_SIZE);

    if(pthread_mutex_init(&sh->lock, NULL) != 0){
        perror("pthread_mutex_init");
        exit(EXIT_FAILURE);
    }
    if(pthread_cond_init(&sh->cond_player, NULL) != 0){
        perror("pthread_cond_init");
        exit(EXIT_FAILURE);
    }
    if(pthread_cond_init(&sh->cond_turn, NULL) != 0){
        perror("pthread_cond_init");
        exit(EXIT_FAILURE);
    }
    if(pthread_cond_init(&sh->cond_letter, NULL) != 0){
        perror("pthread_cond_init");
        exit(EXIT_FAILURE);
    }
    return sh;
}

void shared_destroy(shared* sh){
    pthread_mutex_destroy(&sh->lock);
    pthread_cond_destroy(&sh->cond_player);
    pthread_cond_destroy(&sh->cond_turn);
    pthread_cond_destroy(&sh->cond_letter);
    free(sh->punteggi);
    free(sh);
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
    int index_letter = lettere_non_chiamate[rand() % j];
    return 'A' + index_letter;
}

void nascondi_lettera(char* hide_words, const char* original_words){
    int len = strlen(original_words);
    for(int i = 0; i < len; i++){
        if(isalpha(original_words[i])){
            hide_words[i] = '#';
        } else {
            hide_words[i] = original_words[i];
        }
    }
    hide_words[len] = '\0'; 
}

int mostra_lettera(char* hide_words, const char* original_words, char c){
    int j = 0;
    int len = strlen(original_words);
    for(int i = 0; i < len; i++){
        if(original_words[i] == c){
            hide_words[i] = c;
            j++;
        }
    }
    return j;
}

int tab_complete(const char* str){
    return strchr(str, '#') == NULL;
}

int winner(int* arr, int n){
    int max = 0;
    int index = -1;
    for(int i = 0; i < n; i++){
        if(arr[i] > max){
            max = arr[i];
            index = i;
        }
    }
    return index;
}

void gi_thread(void* arg){
    thread_gi* td = (thread_gi*)arg;
    
    // Segnala prontezza
    if(pthread_mutex_lock(&td->sh->lock) != 0){
        perror("pthread_mutex_lock");
        exit(EXIT_FAILURE);
    }
    td->sh->player_ready++;
    if(td->sh->player_ready == td->nplayer){
        if(pthread_cond_signal(&td->sh->cond_player) != 0){
            perror("pthread_cond_signal");
            exit(EXIT_FAILURE);
        }
    }
    if(pthread_mutex_unlock(&td->sh->lock) != 0){
        perror("pthread_mutex_unlock");
        exit(EXIT_FAILURE);
    }
    
    printf("[G%d] avviato e pronto\n", td->thread_n + 1);
    
    while(1){
        if(pthread_mutex_lock(&td->sh->lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }
        // Attende che sia il suo turno oppure la terminazione
        while(!td->sh->done && td->sh->id_player != td->thread_n){
            if(pthread_cond_wait(&td->sh->cond_turn, &td->sh->lock) != 0){
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
        // All'interno del lock, esegue la selezione della lettera
        char lettera = seleziona_lettera(td->sh->lettere_chiamate);
        td->sh->temp_letter = lettera;  // Memorizza la scelta
        printf("[G%d] scelgo la lettera '%c'\n", td->thread_n + 1, lettera);
        td->sh->letter_ready = 1;       // Imposta il flag che indica che la scelta è pronta
        if(pthread_cond_signal(&td->sh->cond_letter) != 0){
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
    if(argc < 4){
        fprintf(stderr, "Usage: %s <n-player> <m-partite> <file.txt>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int nplayer = atoi(argv[1]);
    int mpartite = atoi(argv[2]);
    FILE* f;
    char temp[BUFFER_SIZE];
    shared* sh = init_shared(nplayer);

    if((f = fopen(argv[3], "r")) == NULL){
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    int nrows = 0;
    while(fgets(temp, BUFFER_SIZE, f)){
        if(temp[0] != '\n' && temp[0] != '\0'){
            nrows++;
        }
    }
    rewind(f);

    if(mpartite > nrows){
        fprintf(stderr, "[MIKE] numero di partite troppo grandi, inserire un numero inferiore\n");
        exit(EXIT_FAILURE);
    }
    
    char buffer[BUFFER_SIZE];
    char** elenco_frasi = malloc(sizeof(char*) * nrows);
    int j =  0;
    while(fgets(buffer, BUFFER_SIZE, f)){
        if(buffer[strlen(buffer) - 1] == '\n'){
            buffer[strlen(buffer) - 1] = '\0';
        }
        for(int i = 0; i < strlen(buffer); i++){
            buffer[i] = toupper(buffer[i]);
        }
        elenco_frasi[j++] = strdup(buffer);
    }
    fclose(f);

    printf("[MIKE] lette %d possibili frasi da indovinare per %d partite\n", nrows, mpartite);

    // Creazione dei thread dei giocatori
    thread_gi* td = malloc(sizeof(thread_gi) * nplayer);
    for(int i = 0; i < nplayer; i++){
        td[i].thread_n = i;
        td[i].sh = sh;
        td[i].nplayer = nplayer;
        if(pthread_create(&td[i].tid, NULL, (void*)gi_thread, &td[i]) != 0){
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }
    
    // Attende che tutti i giocatori siano pronti
    if(pthread_mutex_lock(&sh->lock) != 0){
        perror("pthread_mutex_lock");
        exit(EXIT_FAILURE);
    }
    while(sh->player_ready < nplayer){
        if(pthread_cond_wait(&sh->cond_player, &sh->lock) != 0){
            perror("pthread_cond_wait");
            exit(EXIT_FAILURE);
        }
    }
    if(pthread_mutex_unlock(&sh->lock) != 0){
        perror("pthread_mutex_unlock");
        exit(EXIT_FAILURE);
    }
    printf("[MIKE] tutti i giocatori sono pronti, possiamo iniziare!\n");

    int* frasi_indici = malloc(sizeof(int) * nrows);
    for(int i = 0; i < nrows; i++){
        frasi_indici[i] = i;
    }
    shuffle(frasi_indici, nrows);
    int counter_frase = 0; 

    // Ciclo per ogni partita
    for(int partita = 0; partita < mpartite; partita++){
        char* frase_originale = elenco_frasi[frasi_indici[counter_frase++]];
    
        printf("[MIKE] scelta la frase \"%s\" per la partina n. %d\n", frase_originale, partita + 1);

        nascondi_lettera(sh->frase_da_scoprire, frase_originale);
        reset(sh->lettere_chiamate, ALFABETO_SIZE);

        int current_player = 0;
        while(!tab_complete(sh->frase_da_scoprire)){
            if(pthread_mutex_lock(&sh->lock) != 0){
                perror("pthread_mutex_lock");
                exit(EXIT_FAILURE);
            }
            // Prepara il turno: resetta il flag e assegna il turno al giocatore corrente
            sh->letter_ready = 0;
            sh->id_player = current_player;
            printf("[MIKE] adesso è il turno di G%d\n", current_player + 1);
            if(pthread_cond_broadcast(&sh->cond_turn) != 0){
                perror("pthread_cond_broadcast");
                exit(EXIT_FAILURE);
            }
            // Attende che il giocatore effettui la scelta
            while(sh->letter_ready == 0){
                if(pthread_cond_wait(&sh->cond_letter, &sh->lock) != 0){
                    perror("pthread_cond_wait");
                    exit(EXIT_FAILURE);
                }
            }
            char letter = sh->temp_letter;
            sh->lettere_chiamate[letter - 'A']++;
            int lettere_trovate = mostra_lettera(sh->frase_da_scoprire, frase_originale, letter);
            int punteggio_base = (rand() % 4 + 1) * 100;
            int punteggio = punteggio_base * lettere_trovate;
            sh->punteggi[current_player] += punteggio;
            
            if(lettere_trovate == 0){
                printf("[MIKE] nessuna occorrenza per %c\n", letter);
                current_player = (current_player + 1) % nplayer;
            } else {
                printf("[M] ci sono %d occorrenze per %c; assegnati %d x %d = %d\n", lettere_trovate, letter, punteggio_base, lettere_trovate, punteggio);
                /* Se la scelta è corretta, il giocatore mantiene il turno.
                   Altrimenti, il turno passa al giocatore successivo (già fatto sopra). */
            }
            printf("[MIKE] tabellone: %s\n", sh->frase_da_scoprire);
            if(pthread_mutex_unlock(&sh->lock) != 0){
                perror("pthread_mutex_unlock");
                exit(EXIT_FAILURE);
            }
        }

        if(pthread_mutex_lock(&sh->lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }
        printf("[MIKE] frase completata: Punteggi attuali: ");
        for(int i = 0; i < nplayer; i++){
            printf("G%d: %d ", i + 1, sh->punteggi[i]);
        }
        printf("\n");
        if(pthread_mutex_unlock(&sh->lock) != 0){
            perror("pthread_mutex_unlock");
            exit(EXIT_FAILURE);
        }
    }

    // Termina il gioco: segnala a tutti di uscire
    if(pthread_mutex_lock(&sh->lock) != 0){
        perror("pthread_mutex_lock");
        exit(EXIT_FAILURE);
    }
    sh->done = true;
    if(pthread_cond_broadcast(&sh->cond_turn) != 0){
        perror("pthread_cond_broadcast");
        exit(EXIT_FAILURE);
    }
    if(pthread_mutex_unlock(&sh->lock) != 0){
        perror("pthread_mutex_unlock");
        exit(EXIT_FAILURE);
    }

    int win = winner(sh->punteggi, nplayer);
    printf("[MIKE] questa era l'ultima partita: il vincitore e' G%d\n", win + 1);

    for(int i = 0; i < nplayer; i++){
        if(pthread_join(td[i].tid, NULL) != 0){
            perror("pthread_join");
            exit(EXIT_FAILURE);
        }
    }
    free(td);
    free(frasi_indici);
    for(int i = 0; i < nrows; i++){
        free(elenco_frasi[i]);
    }
    free(elenco_frasi);
    shared_destroy(sh);
}
