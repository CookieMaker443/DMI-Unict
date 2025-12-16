#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#define BUFFER_SIZE 100

/* Struttura condivisa: contiene tutte le informazioni relative all’asta e gli strumenti di sincronizzazione */
typedef struct {
    int auction_index;                    // Indice asta corrente (diverso da 0 se l’asta è attiva)
    char object_description[BUFFER_SIZE]; // Descrizione dell'oggetto in asta
    int minimum_offer;                    // Offerta minima accettabile
    int maximum_offer;                    // Offerta massima (usata per generare l’offerta casuale)
    int current_offer;                    // Offerta ricevuta da un bidder
    int current_bidder_index;             // Indice del bidder che ha inviato l’offerta
    int exit_flag;                        // Flag per segnalare la fine delle aste (1 = terminare)
    
    pthread_mutex_t mutex;                // Mutex per proteggere l’accesso ai campi condivisi
    pthread_cond_t cond_bidder;           // Condizione per notificare ai bidder l’inizio di una nuova asta
    pthread_cond_t cond_judge;            // Condizione per notificare al giudice che un bidder ha inviato l’offerta
} shared_data_t;

/* Struttura dati per ciascun thread bidder */
typedef struct {
    unsigned thread_n;      // Identificativo: 1..N per i bidder
    pthread_t tid;          // ID del thread
    shared_data_t *sh;      // Puntatore alla struttura dati condivisa
} thread_data_t;

/* Funzione di parsing:
   Esamina una riga del file nel formato "oggetto,minimum,maximum"
   ed estrae i dati in name, min e max. */
int parse_line(char *line, char *name, int *min, int *max) {
    char *token = strtok(line, ",");
    if (token != NULL) {
        strcpy(name, token);
        token = strtok(NULL, ",");
        if (token != NULL) {
            *min = atoi(token);
            token = strtok(NULL, ",");
            if (token != NULL) {
                *max = atoi(token);
                return 0;
            }
        }
    }
    return -1;
}

/* Funzione eseguita dai thread bidder.
   Ogni bidder attende il segnale del giudice (tramite cond_bidder) per effettuare un’offerta. */
void *bidder_thread(void *arg) {
    thread_data_t *td = (thread_data_t *)arg;
    shared_data_t *sh = td->sh;
    
    printf("[B%d] offerente pronto\n", td->thread_n);
    
    while (1) {
        if (pthread_mutex_lock(&sh->mutex) != 0) {
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }
        /* Attende una nuova asta (auction_index != 0) oppure la richiesta di terminare */
        while (sh->auction_index == 0 && sh->exit_flag == 0) {
            if (pthread_cond_wait(&sh->cond_bidder, &sh->mutex) != 0) {
                perror("pthread_cond_wait");
                exit(EXIT_FAILURE);
            }
        }
        /* Se è stato impostato il flag di terminazione, esce */
        if (sh->exit_flag) {
            pthread_mutex_unlock(&sh->mutex);
            break;
        }
        /* Genera un’offerta casuale compresa tra 1 e maximum_offer */
        int offer = 1 + rand() % sh->maximum_offer;
        sh->current_offer = offer;
        /* Per identificare il bidder, memorizziamo l'indice (i bidder sono numerati da 1) */
        sh->current_bidder_index = td->thread_n - 1;
        /* Notifica al giudice che l’offerta è pronta */
        if (pthread_cond_signal(&sh->cond_judge) != 0) {
            perror("pthread_cond_signal");
            exit(EXIT_FAILURE);
        }
        if (pthread_mutex_unlock(&sh->mutex) != 0) {
            perror("pthread_mutex_unlock");
            exit(EXIT_FAILURE);
        }
    }
    
    return NULL;
}

/* MAIN: il thread principale agisce da giudice.
   Legge il file delle aste, aggiorna la struttura condivisa, invia i segnali ai bidder,
   raccoglie le offerte e determina il vincitore per ciascuna asta. */
int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <auction_file> <num_bidders>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    char *auction_file = argv[1];
    int global_num_bidders = atoi(argv[2]);
    
    /* Inizializzazione della struttura condivisa */
    shared_data_t *shared = malloc(sizeof(shared_data_t));
    if (shared == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    memset(shared, 0, sizeof(shared_data_t));
    shared->auction_index = 0;
    shared->exit_flag = 0;
    if (pthread_mutex_init(&shared->mutex, NULL) != 0) {
        perror("pthread_mutex_init");
        exit(EXIT_FAILURE);
    }
    if (pthread_cond_init(&shared->cond_bidder, NULL) != 0) {
        perror("pthread_cond_init");
        exit(EXIT_FAILURE);
    }
    if (pthread_cond_init(&shared->cond_judge, NULL) != 0) {
        perror("pthread_cond_init");
        exit(EXIT_FAILURE);
    }
    
    /* Creazione dei thread bidder */
    int total_bidder = global_num_bidders;
    thread_data_t *td = malloc(sizeof(thread_data_t) * total_bidder);
    if (td == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < total_bidder; i++) {
        td[i].thread_n = i + 1;  // I bidder sono numerati da 1 a total_bidder
        td[i].sh = shared;
        if (pthread_create(&td[i].tid, NULL, bidder_thread, &td[i]) != 0) {
            perror("pthread_create bidder");
            exit(EXIT_FAILURE);
        }
    }
    
    /* Il giudice (main) apre il file delle aste e le gestisce una per volta */
    FILE *f = fopen(auction_file, "r");
    if (f == NULL) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }
    
    char buffer[BUFFER_SIZE];
    int auction_count = 1;
    
    /* Array per raccogliere le offerte e l’ordine di arrivo */
    int *offers  = malloc(sizeof(int) * total_bidder);
    int *ranking = malloc(sizeof(int) * total_bidder);
    int order = 0;
    
    /* Variabili per il riepilogo finale */
    int total_auctions = 0;
    int auctions_assigned = 0;
    int auctions_void = 0;
    int total_collected = 0;
    
    while (fgets(buffer, BUFFER_SIZE, f) != NULL) {
        char name[BUFFER_SIZE];
        int min, max;
        if (parse_line(buffer, name, &min, &max) != 0) {
            fprintf(stderr, "Formato della riga non valido\n");
            continue;
        }
        
        /* Aggiornamento dei dati dell'asta nella struttura condivisa */
        if (pthread_mutex_lock(&shared->mutex) != 0) {
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }
        strcpy(shared->object_description, name);
        shared->minimum_offer = min;
        shared->maximum_offer = max;
        shared->auction_index = auction_count;  // Indica che un'asta è attiva
        /* Notifica a tutti i bidder l’avvio della nuova asta */
        if (pthread_cond_broadcast(&shared->cond_bidder) != 0) {
            perror("pthread_cond_broadcast");
            exit(EXIT_FAILURE);
        }
        if (pthread_mutex_unlock(&shared->mutex) != 0) {
            perror("pthread_mutex_unlock");
            exit(EXIT_FAILURE);
        }
        
        printf("[J] lancio asta n. %d per %s con offerta minima %d EUR e massima %d EUR\n",
               auction_count, shared->object_description, shared->minimum_offer, shared->maximum_offer);
        
        /* Raccolta delle offerte da tutti i bidder */
        for (int i = 0; i < total_bidder; i++) {
            if (pthread_mutex_lock(&shared->mutex) != 0) {
                perror("pthread_mutex_lock");
                exit(EXIT_FAILURE);
            }
            if (pthread_cond_wait(&shared->cond_judge, &shared->mutex) != 0) {
                perror("pthread_cond_wait");
                exit(EXIT_FAILURE);
            }
            int bidder_index = shared->current_bidder_index;
            offers[bidder_index] = shared->current_offer;
            ranking[bidder_index] = order++;
            printf("[J] ricevuta offerta da B%d\n", bidder_index + 1);
            if (pthread_mutex_unlock(&shared->mutex) != 0) {
                perror("pthread_mutex_unlock");
                exit(EXIT_FAILURE);
            }
        }
        
        /* Determinazione del vincitore per l'asta corrente */
        int winner = -1;
        int best_offer = -1;
        int valid_count = 0;
        for (int i = 0; i < total_bidder; i++) {
            if (offers[i] >= shared->minimum_offer && offers[i] <= shared->maximum_offer) {
                valid_count++;
                if (offers[i] > best_offer) {
                    best_offer = offers[i];
                    winner = i;
                } else if (offers[i] == best_offer) {
                    if (ranking[i] < ranking[winner]) { // in caso di parità, vince il primo arrivato
                        winner = i;
                    }
                }
            }
        }
        
        /* Aggiornamento dei contatori per il riepilogo finale */
        total_auctions++;
        if (winner >= 0) {
            auctions_assigned++;
            total_collected += best_offer;
            printf("[J] l'asta n. %d per %s si è conclusa con %d offerte valide su %d, "
                   "il vincitore è B%d che si aggiudica l'oggetto per %d EUR\n",
                   auction_count, shared->object_description, valid_count, total_bidder,
                   winner + 1, best_offer);
        } else {
            auctions_void++;
            printf("[J] l'asta n. %d per %s si è conclusa senza alcuna offerta valida, oggetto non assegnato\n",
                   auction_count, shared->object_description);
        }
        auction_count++;
        
        /* Resetta l'indicatore di asta attiva */
        if (pthread_mutex_lock(&shared->mutex) != 0) {
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }
        shared->auction_index = 0;
        if (pthread_mutex_unlock(&shared->mutex) != 0) {
            perror("pthread_mutex_unlock");
            exit(EXIT_FAILURE);
        }
    }
    
    fclose(f);
    
    /* Stampa del riepilogo finale */
    printf("[J] sono state svolte %d aste di cui %d andate assegnate e %d andate a vuoto; il totale raccolto è di %d EUR\n",
           total_auctions, auctions_assigned, auctions_void, total_collected);
    
    /* Segnala ai bidder di terminare */
    if (pthread_mutex_lock(&shared->mutex) != 0) {
        perror("pthread_mutex_lock");
        exit(EXIT_FAILURE);
    }
    shared->exit_flag = 1;
    if (pthread_cond_broadcast(&shared->cond_bidder) != 0) {
        perror("pthread_cond_broadcast");
        exit(EXIT_FAILURE);
    }
    if (pthread_mutex_unlock(&shared->mutex) != 0) {
        perror("pthread_mutex_unlock");
        exit(EXIT_FAILURE);
    }
    
    /* Attende la terminazione di tutti i thread bidder */
    for (int i = 0; i < total_bidder; i++) {
        if (pthread_join(td[i].tid, NULL) != 0) {
            perror("pthread_join");
            exit(EXIT_FAILURE);
        }
    }
    
    /* Cleanup */
    pthread_mutex_destroy(&shared->mutex);
    pthread_cond_destroy(&shared->cond_bidder);
    pthread_cond_destroy(&shared->cond_judge);
    free(shared);
    free(td);
    free(offers);
    free(ranking);
    
    return 0;
}
