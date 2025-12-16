#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <ctype.h>

#define BUFFER_SIZE 100
#define ALFABETO_SIZE 26

typedef struct{
    char frase_da_scoprire[BUFFER_SIZE];
    int lettere_chiamate[ALFABETO_SIZE];
    int* punteggio;
    char lettera;
    bool done;
    int letter_ready; // 1 scelta, 0 no
    int player_ready;
    int id_player;


    pthread_mutex_t lock;
    pthread_cond_t cond_turn, cond_letter, cond_ready;
}tabellone;

typedef struct{
    pthread_t tid;
    unsigned thread_n;
    int nplayers;
    tabellone* sh;
}thread_gi;

tabellone* init_tab(int nplayers){
    tabellone* sh = malloc(sizeof(tabellone));

    sh->player_ready = sh->letter_ready = 0;
    sh->id_player = -1;
    sh->done = false;
    sh->punteggio = malloc(sizeof(int) * nplayers);
    for(int i = 0; i < nplayers; i++){
        sh->punteggio[i] = 0;
    }

    if(pthread_mutex_init(&sh->lock, NULL) != 0){
        perror("pthread_mutex_init");
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

    if(pthread_cond_init(&sh->cond_ready, NULL) != 0){
        perror("pthread_cond_init");
        exit(EXIT_FAILURE);
    }

    return sh;
}

void tab_destroy(tabellone* sh){
    pthread_mutex_destroy(&sh->lock);
    pthread_cond_destroy(&sh->cond_turn);
    pthread_cond_destroy(&sh->cond_letter);
    pthread_cond_destroy(&sh->cond_ready);
    free(sh->punteggio);
    free(sh);
}

void shuffle(int* arr, int n){
    if(n > 1){
        for(int i = 0; i <n; i++){
            int j = rand()%n;
            int t = arr[j];
            arr[j] = arr[i];
            arr[i] = t;
        }
    }
}

void nascondi_lettere(char* dest_str, char* frase_orignale){
    int size = strlen(frase_orignale);
    for(int i = 0; i < size; i++){
        if(isalpha(frase_orignale[i])){
            dest_str[i] = '#';
        }else{
            dest_str[i] = frase_orignale[i];
        }
    }
    dest_str[size] = '\0';
}

char seleziona_lettera(int* lettere_chiamate){
    int j = 0;
    int lettere_non_chiamate[ALFABETO_SIZE];
    for(int i = 0; i < ALFABETO_SIZE; i++){
        if(lettere_chiamate[i] == 0){
            lettere_non_chiamate[j++] = i;
        }
    }
    if(j == 0){
        return 0;
    }
    int indice_lettera = lettere_non_chiamate[rand()%j];
    return 'A' + indice_lettera;
}

int mostra_lettera(char* frase_nascosta, const char* frase_originale, char c){
    int n = 0;
    int size = strlen(frase_originale);
    for(int i = 0; i < size; i++){
        if(frase_originale[i] == c){
            frase_nascosta[i] = c;
            n++;
        }
    }
    return n;
}

int tab_complete(const char* str){
    return strchr(str, '#') == NULL;
}

int winner(int* arr, int n){
    int winner = -1;
    int max = 0;
    for(int i = 0; i < n; i++){
        if(arr[i] > max){
            max = arr[i];
            winner = i;
        }
    }
    return winner;
}

void reset(int* arr, int n){
    for(int i = 0; i < n; i++){
        arr[i] = 0;
    }
}

void gi_function(void* arg){
    thread_gi* td = (thread_gi*)arg;

    if(pthread_mutex_lock(&td->sh->lock) != 0){
        perror("pthread_mutex_lock");
        exit(EXIT_FAILURE);
    }

    td->sh->player_ready++;
    printf("[G%u] avviato e pronto\n", td->thread_n + 1);
    if(td->sh->player_ready == td->nplayers){
        if(pthread_cond_signal(&td->sh->cond_ready) != 0){
            perror("pthread_cond_signal");
            exit(EXIT_FAILURE);
        }
    }

    if(pthread_mutex_unlock(&td->sh->lock) != 0){
        perror("pthread_mutex_unlock");
        exit(EXIT_FAILURE);
    }

    while(1){
        if(pthread_mutex_lock(&td->sh->lock) != 0){
            perror("Pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }

        while(td->sh->id_player != td->thread_n && !td->sh->done){
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

        char lettera = seleziona_lettera(td->sh->lettere_chiamate);
        td->sh->lettera = lettera;
        printf("[G%u] scelgo lettera '%c'\n", td->thread_n + 1, lettera);
        td->sh->letter_ready = 1;

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
    if(argc != 4){
        fprintf(stderr, "Usage: %s <n-numero-giocatori> <m-numero-partite> <file-con-frasi>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int ngiocatori = atoi(argv[1]);
    int mpartite = atoi(argv[2]);
    char* file_frasi = argv[3];

    FILE* f;
    char buffer[BUFFER_SIZE];
    int nfrasi = 0;
    while(fgets(buffer, BUFFER_SIZE, f)){
        if(buffer[strlen(buffer) - 1] == '\n'){
            buffer[strlen(buffer) - 1] = '\0';
        }
        nfrasi++;
    }
    rewind(f);

    if(nfrasi < mpartite){
        fprintf(stderr, "[M] numero di partite alto il numero massimo possibile è %d\n", nfrasi);
        exit(EXIT_FAILURE);
    }
    
    printf("[M] lette %d possibili frasi da indovinare per %d partite\n", nfrasi, mpartite);


    tabellone* sh = init_tab(ngiocatori);
    thread_gi* td = malloc(sizeof(thread_gi) * ngiocatori);
    for(int i = 0; i < ngiocatori; i++){
        td[i].sh = sh;
        td[i].nplayers = ngiocatori;
        td[i].thread_n = i;
        if(pthread_create(&td[i].tid, NULL, (void*)gi_function, &td[i]) != 0){
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }

    char** elenco_frasi = malloc(sizeof(char*) * nfrasi);
    int j = 0;
    while(fgets(buffer, BUFFER_SIZE, f)){
        int len = strlen(buffer);
        if(buffer[len- 1] == '\n'){
            buffer[len - 1] = '\0';
            len--;
        }
        for(int i = 0; i < len; i++){
            buffer[i] = toupper(buffer[i]);
        }
        elenco_frasi[j++] = strdup(buffer);
    }
    fclose(f);

    if(pthread_mutex_lock(&sh->lock) != 0){
        perror("pthread_mutex_lock");
        exit(EXIT_FAILURE);
    }

    while(sh->player_ready < ngiocatori){
        if(pthread_cond_wait(&td->sh->cond_ready, &td->sh->lock) != 0){
            perror("pthread_cond_wait");
            exit(EXIT_FAILURE);
        }
    }

    if(pthread_mutex_unlock(&td->sh->lock) != 0){
        perror("pthread_mutex_unlock");
        exit(EXIT_FAILURE);
    }

    printf("[M] tutti i giocatori sono pronti, prossimo iniziare!\n");
    int* indici_frasi = malloc(sizeof(int) * nfrasi);
    for(int i = 0; i < nfrasi; i++){
        indici_frasi[i] = i;
    }
    shuffle(indici_frasi, nfrasi);
    int indice_frase_corrente = 0;

    for(int i = 0; i < mpartite; i++){
        char* frase_originale = elenco_frasi[indici_frasi[indice_frase_corrente++]];
        printf("[M] scelto la frase \"%s\" per la partita n:%d\n", frase_originale, i + 1);
        
        nascondi_lettere(td->sh->frase_da_scoprire, frase_originale);
        reset(td->sh->lettere_chiamate, ALFABETO_SIZE);

        int current_player = 0;
        while(!tab_complete(td->sh->frase_da_scoprire)){
            if(pthread_mutex_lock(&sh->lock) != 0){
                perror("Pthread_mutex_lock");
                exit(EXIT_FAILURE);
            }

            td->sh->id_player = current_player;
            td->sh->letter_ready = 0;

            if(pthread_cond_broadcast(&td->sh->cond_turn) != 0){
                perror("pthread_Cond_broadcast");
                exit(EXIT_FAILURE);
            }

            while(td->sh->letter_ready == 0){
                if(pthread_cond_wait(&td->sh->cond_letter, &td->sh->lock) != 0){
                    perror("pthread_cond_wait");
                    exit(EXIT_FAILURE);
                }
            }

            char lettera = td->sh->lettera;
            td->sh->lettere_chiamate[lettera - 'A']++;
            int lettere_trovate = mostra_lettera(td->sh->frase_da_scoprire, frase_originale, lettera);
            int punteggio = (rand()% 4 + 1) * 100;
            int player_punteggio = punteggio * lettere_trovate;
            td->sh->punteggio[current_player] += punteggio;

            if(lettere_trovate == 0){
                printf("[M] Nessuna occorrenza per %c\n", lettera);
                current_player = (current_player + 1) % ngiocatori;
            }
            else {
                printf("[M] ci sono %d occorrenze per %c; assegnati %dx%d=%d punti a G%d\n", lettere_trovate, lettera, punteggio, lettere_trovate, player_punteggio, i + 1);   
            }
            printf("[M] tabellone: %s\n", td->sh->frase_da_scoprire);
            if(pthread_mutex_unlock(&sh->lock) != 0){
                perror("pthread_mutex_unlock");
                exit(EXIT_FAILURE);
            }
        }

        if(pthread_mutex_lock(&sh->lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }
        printf("[M] frase completata; punteggi attuali: ");
        for(int i = 0; i  < ngiocatori; i++){
            printf("G:%d:%d ", i + 1, td->sh->punteggio[i]);
        }
        printf("\n");
        if(pthread_mutex_unlock(&sh->lock) != 0){
            perror("pthread_mutex_unlock");
            exit(EXIT_FAILURE);
        }
    }

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

    int vincitore = winner(td->sh->punteggio, ngiocatori);
    printf("[M] questa era l'ultima partita: il vincitore e' G%d\n", vincitore + 1);

    for(int i = 0; i < ngiocatori; i++){
        if(pthread_join(td[i].tid, NULL) != 0){
            perror("pthread_join");
            exit(EXIT_FAILURE);
        }
    }
    free(td);
    free(indici_frasi);
    for(int i = 0; i < nfrasi; i++){
        free(elenco_frasi[i]);
    }
    free(elenco_frasi);
    tab_destroy(sh);
}