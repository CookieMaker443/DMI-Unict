#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>

#define BUFFER_SIZE 100

typedef enum { INS, ADD, SUB, MUL, DONE } operator;

typedef struct {
    long long operando1;
    long long operando2;
    long long risultato;
    operator op;
    int id_richiedente;
    bool risultato_ready;
    bool op_executed;  // Flag per indicare che l'operazione è stata eseguita
    unsigned nthread_done; // numero di thread CALC terminati
    unsigned success;      // numero di computazioni corrette
    bool done;             // flag di terminazione

    pthread_mutex_t lock;
    pthread_cond_t* calc;  // array di variabili di condizione per i thread CALC
    pthread_cond_t cond_op; // unica variabile di condizione per l'operazione
} shared;

typedef struct {
    pthread_t tid;
    unsigned thread_n;
    char* filename;
    shared* sh;
    int ncalc;
} thread_calc;

typedef struct {
    pthread_t tid;
    shared* sh;
} thread_op; // thread che esegue le operazioni (ADD, SUB, MUL)

shared* init_shared(int ncalc) {
    shared* sh = malloc(sizeof(shared));

    sh->success = sh->nthread_done = sh->risultato = 0;
    sh->risultato_ready = false;
    sh->op_executed = false;
    sh->done = false;
    sh->op = INS;
    sh->calc = malloc(sizeof(pthread_cond_t) * ncalc);
    for (int i = 0; i < ncalc; i++) {
        if (pthread_cond_init(&sh->calc[i], NULL) != 0) {
            perror("pthread_cond_init calc");
            exit(EXIT_FAILURE);
        }
    }
    if (pthread_mutex_init(&sh->lock, NULL) != 0) {
        perror("pthread_mutex_init");
        exit(EXIT_FAILURE);
    }
    if (pthread_cond_init(&sh->cond_op, NULL) != 0) {
        perror("pthread_cond_init cond_op");
        exit(EXIT_FAILURE);
    }
    return sh;
}

void shared_destroy(shared* sh, int ncalc) {
    pthread_mutex_destroy(&sh->lock);
    pthread_cond_destroy(&sh->cond_op);
    for (int i = 0; i < ncalc; i++) {
        pthread_cond_destroy(&sh->calc[i]);
    }
    free(sh->calc);
    free(sh);
}

void calc_function(void* arg) {
    thread_calc* td = (thread_calc*) arg;
    FILE* f;
    char buffer[BUFFER_SIZE];
    long long value;
    long long risultato;

    if ((f = fopen(td->filename, "r")) == NULL) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    printf("[CALC-%u] file da verificare: '%s'\n", td->thread_n + 1, td->filename);
    if (fgets(buffer, BUFFER_SIZE, f)) {
        value = atoll(buffer);
        printf("[CALC-%u] valore iniziale della computazione: %lld\n", td->thread_n + 1, value);
    }

    while (fgets(buffer, BUFFER_SIZE, f)) {
        if (buffer[strlen(buffer) - 1] == '\n')
            buffer[strlen(buffer) - 1] = '\0';

        // Se il formato non rispetta l'operazione, interpreto il valore finale
        if (buffer[1] != ' ') {
            risultato = atoll(buffer);
            break;
        }

        printf("[CALC-%u] prossima operazione: '%s'\n", td->thread_n + 1, buffer);

        if (pthread_mutex_lock(&td->sh->lock) != 0) {
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }

        // Imposta la richiesta
        td->sh->operando1 = value;
        td->sh->operando2 = atoll(buffer + 2);
        td->sh->id_richiedente = td->thread_n;
        td->sh->op_executed = false;  // reset del flag
        // Imposta il tipo di operazione in base al primo carattere
        if (buffer[0] == '+')
            td->sh->op = ADD;
        else if (buffer[0] == '-')
            td->sh->op = SUB;
        else if (buffer[0] == 'x')
            td->sh->op = MUL;

        // Notifica il thread operatore (unico) che è disponibile una nuova richiesta
        if (pthread_cond_signal(&td->sh->cond_op) != 0) {
            perror("pthread_cond_signal cond_op");
            exit(EXIT_FAILURE);
        }
        // Attende che il risultato sia pronto
        while (!td->sh->risultato_ready) {
            if (pthread_cond_wait(&td->sh->calc[td->sh->id_richiedente], &td->sh->lock) != 0) {
                perror("pthread_cond_wait calc");
                exit(EXIT_FAILURE);
            }
        }
        // Legge il risultato
        value = td->sh->risultato;
        printf("[CALC-%u] risultato ricevuto: %lld\n", td->thread_n + 1, value);
        td->sh->risultato_ready = false; // reset per la prossima operazione

        if (pthread_mutex_unlock(&td->sh->lock) != 0) {
            perror("pthread_mutex_unlock");
            exit(EXIT_FAILURE);
        }
    }
    fclose(f);

    if (pthread_mutex_lock(&td->sh->lock) != 0) {
        perror("pthread_mutex_lock");
        exit(EXIT_FAILURE);
    }
    td->sh->nthread_done++;
    if (risultato == value) {
        td->sh->success++;
        printf("[CALC-%u] computazione terminata in modo corretto: %lld\n", td->thread_n + 1, value);
    } else {
        printf("[CALC-%u] computazione terminata in modo errato: %lld\n", td->thread_n + 1, value);
        exit(EXIT_FAILURE);
    }
    // Se è l'ultimo thread calc, segnala la terminazione
    if (td->sh->nthread_done == td->ncalc) {
        td->sh->op = DONE;
        td->sh->done = true;
        if (pthread_cond_signal(&td->sh->cond_op) != 0) {
            perror("pthread_cond_signal cond_op");
            exit(EXIT_FAILURE);
        }
    }
    if (pthread_mutex_unlock(&td->sh->lock) != 0) {
        perror("pthread_mutex_unlock");
        exit(EXIT_FAILURE);
    }
}

void operation_function(void* arg) {
    thread_op* td = (thread_op*)arg;
    while (1) {
        if (pthread_mutex_lock(&td->sh->lock) != 0) {
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }
        // Attende finché non c'è una richiesta (op diverso da INS) oppure viene segnalata DONE
        while (td->sh->op == INS && !td->sh->done) {
            if (pthread_cond_wait(&td->sh->cond_op, &td->sh->lock) != 0) {
                perror("pthread_cond_wait cond_op");
                exit(EXIT_FAILURE);
            }
        }
        // Se è terminazione, esce dal ciclo
        if (td->sh->done) {
            if (pthread_mutex_unlock(&td->sh->lock) != 0) {
                perror("pthread_mutex_unlock");
                exit(EXIT_FAILURE);
            }
            break;
        }
        // Se non è già stata eseguita, esegue l'operazione in base al tipo
        if (!td->sh->op_executed) {
            switch (td->sh->op) {
                case ADD:
                    td->sh->risultato = td->sh->operando1 + td->sh->operando2;
                    printf("[OP] calcolo ADD effettuato: %lld + %lld = %lld\n", td->sh->operando1, td->sh->operando2, td->sh->risultato);
                    break;
                case SUB:
                    td->sh->risultato = td->sh->operando1 - td->sh->operando2;
                    printf("[OP] calcolo SUB effettuato: %lld - %lld = %lld\n", td->sh->operando1, td->sh->operando2, td->sh->risultato);
                    break;
                case MUL:
                    td->sh->risultato = td->sh->operando1 * td->sh->operando2;
                    printf("[OP] calcolo MUL effettuato: %lld x %lld = %lld\n", td->sh->operando1, td->sh->operando2, td->sh->risultato);
                    break;
                default:
                    fprintf(stderr, "[OP] nessun calcolo effetuato\n");
                    exit(EXIT_FAILURE);
                    break;
            }

            td->sh->op_executed = true;
            td->sh->risultato_ready = true;
            // Notifica il thread CALC corrispondente
            if (pthread_cond_signal(&td->sh->calc[td->sh->id_richiedente]) != 0) {
                perror("pthread_cond_signal calc");
                exit(EXIT_FAILURE);
            }
            // Resetta il campo op a INS per attendere la nuova richiesta
            td->sh->op = INS;
        }
        if (pthread_mutex_unlock(&td->sh->lock) != 0) {
            perror("pthread_mutex_unlock");
            exit(EXIT_FAILURE);
        }
    }
}

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <calc1.txt> <calc2.txt> ... <calcn.txt>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    int ncalc = argc - 1;
    shared* sh = init_shared(ncalc);
    thread_calc* cc = malloc(sizeof(thread_calc) * ncalc);
    for (int i = 0; i < ncalc; i++) {
        cc[i].filename = argv[i + 1];
        cc[i].ncalc = ncalc;
        cc[i].thread_n = i;
        cc[i].sh = sh;
        if (pthread_create(&cc[i].tid, NULL, (void*)calc_function, &cc[i]) != 0) {
            perror("pthread_create calc");
            exit(EXIT_FAILURE);
        }
    }
    // Crea un unico thread operatore
    thread_op op_thread;
    op_thread.sh = sh;
    if (pthread_create(&op_thread.tid, NULL, (void*)operation_function, &op_thread) != 0) {
        perror("pthread_create operation_function");
        exit(EXIT_FAILURE);
    }
    // Aspetta che i thread calc terminino
    for (int i = 0; i < ncalc; i++) {
        if (pthread_join(cc[i].tid, NULL) != 0) {
            perror("pthread_join calc");
            exit(EXIT_FAILURE);
        }
    }
    // Aspetta il thread operatore
    if (pthread_join(op_thread.tid, NULL) != 0) {
        perror("pthread_join operation_function");
        exit(EXIT_FAILURE);
    }
    printf("[MAIN] verifiche completate con successo: %u/%d\n", sh->success, ncalc);
    shared_destroy(sh, ncalc);
    free(cc);
}
