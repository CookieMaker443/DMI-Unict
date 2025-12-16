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

// Struttura condivisa (il Tabellone)
typedef struct {
    char frase_da_scoprire[MAX_FRASE_SIZE]; // frase con lettere oscurate
    int contatore_lettere[ALFABETO_SIZE];   // tiene traccia delale lettere già chiamate
    int n_giocatori;                        // numero di giocatori
    int *punteggi;                          // punteggio per ogni giocatore
    char lettera;                           // ultima lettera scelta
    int fine;                               // flag di fine gioco

    // Variabili di sincronizzazione condivise
    pthread_mutex_t lock;
    pthread_cond_t cond_turn;    // per segnalare al giocatore che è il suo turno
    pthread_cond_t cond_letter;  // per notificare a Mike che il giocatore ha scelto la lettera
    pthread_cond_t cond_all_ready; // per far aspettare Mike finché tutti i giocatori non sono pronti

    int current_player; // indice del giocatore di turno; -1 se nessuno
    int letter_ready;   // flag: 1 se il giocatore ha scelto la lettera, 0 altrimenti
    int players_ready;  // conteggio dei giocatori che hanno segnalato la propria prontezza
} tabellone_t;

// Struttura per ogni giocatore
typedef struct {
    int codice;          // identificatore (0,1,2,...)
    tabellone_t *tabellone;
} giocatore_t;

// Funzioni di utilità
void shuffle(int *array, int n) {
    if (n > 1){
        for (int i = 0; i < n; i++){
            int j = rand() % n;
            int t = array[j];
            array[j] = array[i];
            array[i] = t;
        }
    }
}

char seleziona_lettera(int *contatore_lettere) {
    int j = 0;
    int lettere_non_chiamate[ALFABETO_SIZE];
    for (int i = 0; i < ALFABETO_SIZE; i++){
        if (contatore_lettere[i] == 0){
            lettere_non_chiamate[j++] = i;
        }
    }
    if (j == 0)
        return 0;
    int indice_lettera = lettere_non_chiamate[rand() % j];
    return 'A' + indice_lettera;
}

void reset(int *array, int n){
    for (int i = 0; i < n; i++)
        array[i] = 0;
}

void nascondi_lettere(char *dest_str, const char *original_str){
    int size = strlen(original_str);
    for (int i = 0; i < size; i++){
        if (isalpha(original_str[i]))
            dest_str[i] = '#';
        else
            dest_str[i] = original_str[i];
    }
    dest_str[size] = '\0';
}

int mostra_lettere(char *dest_str, const char *original_str, char c){
    int n = 0;
    int size = strlen(original_str);
    for (int i = 0; i < size; i++){
        if (original_str[i] == c){
            dest_str[i] = c;
            n++;
        }
    }
    return n;
}

int tabellone_completato(const char *str){
    return strchr(str, '#') == NULL;
}

int argmax(int *array, int n){
    int index = -1;
    int max = 0;
    for (int i = 0; i < n; i++){
        if (array[i] > max){
            max = array[i];
            index = i;
        }
    }
    return index;
}

// Thread del giocatore: attende il proprio turno e sceglie una lettera
void *giocatore_thread(void *args){
    giocatore_t *giocatore = (giocatore_t *)args;
    tabellone_t *tab = giocatore->tabellone;
    
    // Segnala la propria prontezza
    pthread_mutex_lock(&tab->lock);
    tab->players_ready++;
    if(tab->players_ready == tab->n_giocatori)
        pthread_cond_signal(&tab->cond_all_ready);
    pthread_mutex_unlock(&tab->lock);
    
    printf("[G%d] avviato e pronto\n", giocatore->codice + 1);
    
    while(1){
        pthread_mutex_lock(&tab->lock);
        // Attende finché non è il suo turno o il gioco è finito
        while(tab->current_player != giocatore->codice && !tab->fine){
            pthread_cond_wait(&tab->cond_turn, &tab->lock);
        }
        if(tab->fine){
            pthread_mutex_unlock(&tab->lock);
            break;
        }
        pthread_mutex_unlock(&tab->lock);
        
        // Se è il suo turno, seleziona la lettera
        char lettera = seleziona_lettera(tab->contatore_lettere);
        
        pthread_mutex_lock(&tab->lock);
        tab->lettera = lettera;
        printf("[G%d] scelgo la lettera '%c'\n", giocatore->codice + 1, lettera);
        tab->letter_ready = 1;
        // Notifica a Mike che la scelta è pronta
        pthread_cond_signal(&tab->cond_letter);
        pthread_mutex_unlock(&tab->lock);
    }
    return NULL;
}

int main(int argc, char **argv){
    if(argc < 4){
        perror("Argomenti non validi");
        exit(EXIT_FAILURE);
    }

    int n_giocatori = atoi(argv[1]);
    int n_partite = atoi(argv[2]);
    char *frasi_filename = argv[3];

    // Carico le frasi dal file
    char buffer[MAX_FRASE_SIZE];
    FILE *frasi_fp = fopen(frasi_filename, "r");
    if (frasi_fp == NULL){
        perror("Errore apertura file delle frasi");
        exit(EXIT_FAILURE);
    }
    int n_frasi = 0;
    while (!feof(frasi_fp)){
        if (fgetc(frasi_fp) == '\n')
            n_frasi++;
    }
    fseek(frasi_fp, 0, SEEK_SET);
    
    if(n_frasi < n_partite){
        perror("Numero delle frasi inferiore al numero delle partite");
        exit(EXIT_FAILURE);
    }
    
    char **elenco_frasi = malloc(sizeof(char *) * n_frasi);
    int j = 0;
    while (fgets(buffer, MAX_FRASE_SIZE, frasi_fp) != NULL){
        int l = strcspn(buffer, "\n");
        for (int i = 0; i < l; i++){
            buffer[i] = toupper(buffer[i]);
        }
        elenco_frasi[j] = malloc(sizeof(char) * (l + 1));
        strncpy(elenco_frasi[j], buffer, l);
        elenco_frasi[j][l] = '\0';
        j++;
    }
    fclose(frasi_fp);
    printf("[M] lette %d possibili frasi da indovinare per %d partite\n", n_frasi, n_partite);

    // Inizializzo il tabellone condiviso e le variabili di sincronizzazione
    tabellone_t *tab = malloc(sizeof(tabellone_t));
    tab->n_giocatori = n_giocatori;
    tab->punteggi = calloc(n_giocatori, sizeof(int));
    tab->fine = 0;
    tab->current_player = -1;
    tab->letter_ready = 0;
    tab->players_ready = 0;
    reset(tab->contatore_lettere, ALFABETO_SIZE);

    pthread_mutex_init(&tab->lock, NULL);
    pthread_cond_init(&tab->cond_turn, NULL);
    pthread_cond_init(&tab->cond_letter, NULL);
    pthread_cond_init(&tab->cond_all_ready, NULL);

    // Creo i thread dei giocatori
    giocatore_t **giocatori = malloc(sizeof(giocatore_t *) * n_giocatori);
    pthread_t *threads = malloc(sizeof(pthread_t) * n_giocatori);
    for (int i = 0; i < n_giocatori; i++){
        giocatore_t *g = malloc(sizeof(giocatore_t));
        g->codice = i;
        g->tabellone = tab;
        if(pthread_create(&threads[i], NULL, giocatore_thread, g) != 0){
            perror("Errore nella creazione del thread");
            exit(EXIT_FAILURE);
        }
        giocatori[i] = g;
    }
    
    // Attende che tutti i giocatori siano pronti
    pthread_mutex_lock(&tab->lock);
    while(tab->players_ready < n_giocatori)
        pthread_cond_wait(&tab->cond_all_ready, &tab->lock);
    pthread_mutex_unlock(&tab->lock);
    printf("[M] Tutti i giocatori sono pronti, possiamo iniziare!\n");
    
    // Genero la sequenza casuale delle frasi
    int *frasi_indici = malloc(n_frasi * sizeof(int));
    for (int i = 0; i < n_frasi; i++){
        frasi_indici[i] = i;
    }
    shuffle(frasi_indici, n_frasi);
    int frase_indice_corrente = 0;
    
    // Ciclo sulle partite
    for (int partita = 0; partita < n_partite; partita++){
        char *frase_originale = elenco_frasi[frasi_indici[frase_indice_corrente++]];
        printf("[M] scelta la frase \"%s\" per la partita n.%d\n", frase_originale, partita + 1);
        
        // Aggiorna il tabellone: nasconde le lettere e resetta il contatore
        nascondi_lettere(tab->frase_da_scoprire, frase_originale);
        reset(tab->contatore_lettere, ALFABETO_SIZE);
        
        int current_player = 0;
        // Finché la frase non è completata
        while (!tabellone_completato(tab->frase_da_scoprire)){
            pthread_mutex_lock(&tab->lock);
            // Imposta il giocatore corrente e resetta il flag
            tab->current_player = current_player;
            tab->letter_ready = 0;
            // Notifica a tutti i giocatori (solo quello interessato procederà)
            pthread_cond_broadcast(&tab->cond_turn);
            // Attende che il giocatore scelga la lettera
            while(tab->letter_ready == 0)
                pthread_cond_wait(&tab->cond_letter, &tab->lock);
            
            char lettera = tab->lettera;
            // Segna che la lettera è stata chiamata
            tab->contatore_lettere[lettera - 'A']++;
            int lettere_trovate = mostra_lettere(tab->frase_da_scoprire, frase_originale, lettera);
            int punteggio_base = (rand() % 4 + 1) * 100;
            int punteggio = punteggio_base * lettere_trovate;
            tab->punteggi[current_player] += punteggio;
            
            if(lettere_trovate == 0){
                printf("[M] nessuna occorrenza per %c\n", lettera);
                // Passa al turno del giocatore successivo
                current_player = (current_player + 1) % n_giocatori;
            } else {
                printf("[M] ci sono %d occorrenze per %c; assegnati %dx%d=%d\n",
                       lettere_trovate, lettera, punteggio_base, lettere_trovate, punteggio);
                // Il giocatore mantiene il turno in caso di successo
            }
            printf("[M] tabellone: %s\n", tab->frase_da_scoprire);
            pthread_mutex_unlock(&tab->lock);
        }
        
        // Partita terminata: stampa i punteggi attuali
        pthread_mutex_lock(&tab->lock);
        printf("[M] frase completata; punteggi attuali: ");
        for (int i = 0; i < n_giocatori; i++){
            printf("G%d:%d ", i + 1, tab->punteggi[i]);
        }
        printf("\n");
        pthread_mutex_unlock(&tab->lock);
    }
    
    // Termina il gioco: segnala a tutti i giocatori di uscire
    pthread_mutex_lock(&tab->lock);
    tab->fine = 1;
    pthread_cond_broadcast(&tab->cond_turn);
    pthread_mutex_unlock(&tab->lock);
    
    // Stampa il vincitore
    int vincitore = argmax(tab->punteggi, n_giocatori);
    printf("[M] questa era l'ultima partita: il vincitore e' G%d\n", vincitore + 1);
    
    // Pulizia: attende la terminazione di tutti i thread giocatori
    for (int i = 0; i < n_giocatori; i++){
        pthread_join(threads[i], NULL);
        free(giocatori[i]);
    }
    free(threads);
    free(giocatori);
    free(frasi_indici);
    for (int i = 0; i < n_frasi; i++){
        free(elenco_frasi[i]);
    }
    free(elenco_frasi);
    
    free(tab->punteggi);
    pthread_mutex_destroy(&tab->lock);
    pthread_cond_destroy(&tab->cond_turn);
    pthread_cond_destroy(&tab->cond_letter);
    pthread_cond_destroy(&tab->cond_all_ready);
    free(tab);
    
    return 0;
}
