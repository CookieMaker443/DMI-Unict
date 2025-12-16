#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <semaphore.h>
#include <pthread.h>
#include <errno.h>
#define BUFFER_SIZE 2048

typedef enum { INS, ADD, SUB, MUL, RES, DONE } operator;

typedef struct{
    long long operator_1;
    long long operator_2;
    long long risultato;
    operator op;
    unsigned id_richiedente;
    unsigned done;
    unsigned success;
    
    pthread_mutex_t lock;
    sem_t calc, add, sub, mul;
    sem_t slot;  // Semaforo aggiuntivo per proteggere il "job slot"
} shared;

typedef struct{
    pthread_t tid;
    unsigned thread_n;
    unsigned ncalc;
    char* filename;
    shared* sh;
} thread_data;

shared* init_shared(){
    shared* sh = malloc(sizeof(shared));
    sh->done = sh->success = 0;
    sh->op = INS;
    sh->id_richiedente = 0;
    if(pthread_mutex_init(&sh->lock, NULL) != 0){
        perror("pthread_mutex_init");
        exit(EXIT_FAILURE);
    }
    if(sem_init(&sh->calc, 0, 0) != 0){
        perror("sem_init calc");
        exit(EXIT_FAILURE);
    }
    if(sem_init(&sh->add, 0, 0) != 0){
        perror("sem_init add");
        exit(EXIT_FAILURE);
    }
    if(sem_init(&sh->sub, 0, 0) != 0){
        perror("sem_init sub");
        exit(EXIT_FAILURE);
    }
    if(sem_init(&sh->mul, 0, 0) != 0){
        perror("sem_init mul");
        exit(EXIT_FAILURE);
    }
    // Inizializziamo il semaforo "slot" a 1 per garantire l'accesso esclusivo
    if(sem_init(&sh->slot, 0, 1) != 0){
        perror("sem_init slot");
        exit(EXIT_FAILURE);
    }
    return sh;
}

void shared_destroy(shared* sh){
    pthread_mutex_destroy(&sh->lock);
    sem_destroy(&sh->calc);
    sem_destroy(&sh->add);
    sem_destroy(&sh->sub);
    sem_destroy(&sh->mul);
    sem_destroy(&sh->slot);
    free(sh);
}

void calc_thread(void* arg){
    thread_data* td = (thread_data*)arg;
    FILE* f;
    char buffer[BUFFER_SIZE];
    long long totale;
    long long risultato;  // valore atteso finale

    printf("[CALC-%u] file da verificare: '%s'\n", td->thread_n + 1, td->filename);

    if((f = fopen(td->filename, "r")) == NULL){
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    if(fgets(buffer, BUFFER_SIZE, f)){
        totale = atoll(buffer);
        printf("[CALC-%u] valore iniziale della computazione: %lld\n", td->thread_n + 1, totale);
    } else {
        fprintf(stderr, "[CALC-%u] errore nella lettura del file %s\n", td->thread_n + 1, td->filename);
        exit(EXIT_FAILURE);
    }

    while(fgets(buffer, BUFFER_SIZE, f)){
        if(buffer[strlen(buffer) - 1] == '\n'){
            buffer[strlen(buffer) - 1] = '\0';
        }
        // Se la linea non contiene l'operazione (ossia, se non ha uno spazio in posizione 1)
        if(buffer[1] != ' '){
            risultato = atoll(buffer);
            break;
        }

        printf("[CALC-%u] prossima operazione: '%s'\n", td->thread_n + 1, buffer);

        // Acquisisco il "slot" per poter depositare la richiesta senza interferenze
        if(sem_wait(&td->sh->slot) != 0){
            perror("sem_wait slot");
            exit(EXIT_FAILURE);
        }

        if(pthread_mutex_lock(&td->sh->lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }

        td->sh->id_richiedente = td->thread_n;
        td->sh->operator_1 = totale;
        td->sh->operator_2 = atoll(buffer + 1);
        printf("op1: %llu, op2: %llu\n", td->sh->operator_1, td->sh->operator_2);

        if(buffer[0] == '+'){
            td->sh->op = ADD;
            if(sem_post(&td->sh->add) != 0){
                perror("sem_post add");
                exit(EXIT_FAILURE);
            }
        } else if(buffer[0] == '-'){
            td->sh->op = SUB;
            if(sem_post(&td->sh->sub) != 0){
                perror("sem_post sub");
                exit(EXIT_FAILURE);
            }
        } else if(buffer[0] == 'x'){
            td->sh->op = MUL;
            if(sem_post(&td->sh->mul) != 0){
                perror("sem_post mul");
                exit(EXIT_FAILURE);
            }
        } else {
            fprintf(stderr, "[CALC-%u] errore operazione nei file\n", td->thread_n + 1);
            exit(EXIT_FAILURE);
        }
        if(pthread_mutex_unlock(&td->sh->lock) != 0){
            perror("pthread_mutex_unlock");
            exit(EXIT_FAILURE);
        }

        // Attendo che il thread di calcolo elabori l'operazione
        if(sem_wait(&td->sh->calc) != 0){
            perror("sem_wait calc");
            exit(EXIT_FAILURE);
        }

        if(pthread_mutex_lock(&td->sh->lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }
        totale = td->sh->risultato;
        td->sh->op = INS;  // resetto l'operazione
        printf("[CALC-%u] risultato ricevuto: %lld\n", td->thread_n + 1, totale);
        if(pthread_mutex_unlock(&td->sh->lock) != 0){
            perror("pthread_mutex_unlock");
            exit(EXIT_FAILURE);
        }
        // Rilascio il "slot" per permettere ad un altro thread CALC di depositare la sua richiesta
        if(sem_post(&td->sh->slot) != 0){
            perror("sem_post slot");
            exit(EXIT_FAILURE);
        }
    }

    if(pthread_mutex_lock(&td->sh->lock) != 0){
        perror("pthread_mutex_lock");
        exit(EXIT_FAILURE);
    }
    td->sh->done++;
    if(totale == risultato){
        td->sh->success++;
        printf("[CALC-%u] computazione terminata in modo corretto: %lld\n", td->thread_n + 1, totale);
    } else {
        printf("[CALC-%u] computazione terminata in modo errato: %lld\n", td->thread_n + 1, totale);
    }
    // Se questo è l'ultimo thread CALC, notifichiamo ai thread di calcolo (ADD, SUB, MUL) che possono terminare
    if(td->sh->done == td->ncalc){
        td->sh->op = DONE;
        if(sem_post(&td->sh->add) != 0){
            perror("sem_post add DONE");
            exit(EXIT_FAILURE);
        }
        if(sem_post(&td->sh->sub) != 0){
            perror("sem_post sub DONE");
            exit(EXIT_FAILURE);
        }
        if(sem_post(&td->sh->mul) != 0){
            perror("sem_post mul DONE");
            exit(EXIT_FAILURE);
        }
    }
    if(pthread_mutex_unlock(&td->sh->lock) != 0){
        perror("pthread_mutex_unlock");
        exit(EXIT_FAILURE);
    }
    fclose(f);
}

void add_thread(void* arg){
    thread_data* td = (thread_data*)arg;
    while(1){
        if(sem_wait(&td->sh->add) != 0){
            perror("sem_wait add");
            exit(EXIT_FAILURE);
        }
        if(pthread_mutex_lock(&td->sh->lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }
        if(td->sh->op == DONE) {
            pthread_mutex_unlock(&td->sh->lock);
            break;
        }
        td->sh->risultato = td->sh->operator_1 + td->sh->operator_2;
        td->sh->op = RES;
        printf("[ADD] calcolo effettuato: %lld + %lld = %lld\n",
               td->sh->operator_1, td->sh->operator_2, td->sh->risultato);
        if(sem_post(&td->sh->calc) != 0){
            perror("sem_post calc");
            exit(EXIT_FAILURE);
        }
        if(pthread_mutex_unlock(&td->sh->lock) != 0){
            perror("pthread_mutex_unlock");
            exit(EXIT_FAILURE);
        }
    }
}

void sub_thread(void* arg){
    thread_data* td = (thread_data*)arg;
    while(1){
        if(sem_wait(&td->sh->sub) != 0){
            perror("sem_wait sub");
            exit(EXIT_FAILURE);
        }
        if(pthread_mutex_lock(&td->sh->lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }
        if(td->sh->op == DONE) {
            pthread_mutex_unlock(&td->sh->lock);
            break;
        }
        td->sh->risultato = td->sh->operator_1 - td->sh->operator_2;
        td->sh->op = RES;
        printf("[SUB] calcolo effettuato: %lld - %lld = %lld\n",
               td->sh->operator_1, td->sh->operator_2, td->sh->risultato);
        if(sem_post(&td->sh->calc) != 0){
            perror("sem_post calc");
            exit(EXIT_FAILURE);
        }
        if(pthread_mutex_unlock(&td->sh->lock) != 0){
            perror("pthread_mutex_unlock");
            exit(EXIT_FAILURE);
        }
    }
}

void mul_thread(void* arg){
    thread_data* td = (thread_data*)arg;
    while(1){
        if(sem_wait(&td->sh->mul) != 0){
            perror("sem_wait mul");
            exit(EXIT_FAILURE);
        }
        if(pthread_mutex_lock(&td->sh->lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }
        if(td->sh->op == DONE) {
            pthread_mutex_unlock(&td->sh->lock);
            break;
        }
        td->sh->risultato = td->sh->operator_1 * td->sh->operator_2;
        td->sh->op = RES;
        printf("[MUL] calcolo effettuato: %lld x %lld = %lld\n",
               td->sh->operator_1, td->sh->operator_2, td->sh->risultato);
        if(sem_post(&td->sh->calc) != 0){
            perror("sem_post calc");
            exit(EXIT_FAILURE);
        }
        if(pthread_mutex_unlock(&td->sh->lock) != 0){
            perror("pthread_mutex_unlock");
            exit(EXIT_FAILURE);
        }
    }
}

int main(int argc, char** argv){
    if(argc < 3){
        fprintf(stderr, "Usage: %s <calc-file-1> <calc-file-2> ... <calc-file-n>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    // Numero totale di thread: n CALC + 3 thread per ADD, SUB, MUL.
    int ncalc = argc - 1;
    thread_data td[3 + ncalc];
    shared* sh = init_shared();

    // Creazione dei thread CALC
    for(int i = 0; i < ncalc; i++){
        td[i].sh = sh;
        td[i].thread_n = i;
        td[i].filename = argv[i + 1];
        td[i].ncalc = ncalc;
        if(pthread_create(&td[i].tid, NULL, (void*)calc_thread, &td[i]) != 0){
            perror("pthread_create calc_thread");
            exit(EXIT_FAILURE);
        }
    }

    // Creazione dei thread di operazione: ADD, SUB, MUL
    td[ncalc].sh = sh;
    if(pthread_create(&td[ncalc].tid, NULL, (void*)add_thread, &td[ncalc]) != 0){
        perror("pthread_create add_thread");
        exit(EXIT_FAILURE);
    }

    td[ncalc+1].sh = sh;
    if(pthread_create(&td[ncalc+1].tid, NULL, (void*)sub_thread, &td[ncalc+1]) != 0){
        perror("pthread_create sub_thread");
        exit(EXIT_FAILURE);
    }

    td[ncalc+2].sh = sh;
    if(pthread_create(&td[ncalc+2].tid, NULL, (void*)mul_thread, &td[ncalc+2]) != 0){
        perror("pthread_create mul_thread");
        exit(EXIT_FAILURE);
    }

    // Join di tutti i thread
    for(int i = 0; i < ncalc + 3; i++){
        if(pthread_join(td[i].tid, NULL) != 0){
            perror("pthread_join");
            exit(EXIT_FAILURE);
        }
    }
    printf("[MAIN] verifiche completate con successo: %u/%d\n", sh->success, ncalc);
    
    shared_destroy(sh);
    return 0;
}
