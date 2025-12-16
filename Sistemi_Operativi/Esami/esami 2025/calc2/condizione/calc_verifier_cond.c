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
    bool op_executed;  // Flag per indicare che l'operazione è già stata eseguita
    unsigned nthread_done; // indica quanti thread hanno concluso le operazioni
    unsigned success; // indica i risultati corretti degli nfile
    bool done;

    pthread_mutex_t lock;
    pthread_cond_t* calc;
    pthread_cond_t add, sub, mul;
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
} thread_add;

typedef struct {
    pthread_t tid;
    shared* sh;
} thread_sub;

typedef struct {
    pthread_t tid;
    shared* sh;
} thread_mul;

shared* init_shared(int ncalc) {
    shared* sh = malloc(sizeof(shared));

    sh->success = sh->done = sh->risultato = 0;
    sh->risultato_ready = false;
    sh->op_executed = false;  // inizialmente l'operazione non è stata eseguita
    sh->done = false;
    sh->op = INS;
    sh->calc = malloc(sizeof(pthread_cond_t) * ncalc);
    
    for (int i = 0; i < ncalc; i++) {
        if (pthread_cond_init(&sh->calc[i], NULL) != 0) {
            perror("pthread_cond_init");
            exit(EXIT_FAILURE);
        }
    }

    if (pthread_mutex_init(&sh->lock, NULL) != 0) {
        perror("pthread_mutex_init");
        exit(EXIT_FAILURE);
    }

    if (pthread_cond_init(&sh->add, NULL) != 0) {
        perror("pthread_cond_init");
        exit(EXIT_FAILURE);
    }
    if (pthread_cond_init(&sh->sub, NULL) != 0) {
        perror("pthread_cond_init");
        exit(EXIT_FAILURE);
    }
    if (pthread_cond_init(&sh->mul, NULL) != 0) {
        perror("pthread_cond_init");
        exit(EXIT_FAILURE);
    }

    return sh;
}

void shared_destroy(shared* sh, int ncalc) {
    pthread_mutex_destroy(&sh->lock);
    pthread_cond_destroy(&sh->add);
    pthread_cond_destroy(&sh->sub);
    pthread_cond_destroy(&sh->mul);
    for (int i = 0; i < ncalc; i++) {
        pthread_cond_destroy(&sh->calc[i]);
    }
    free(sh->calc);
    free(sh);
}

void calc_function(void* arg) {
    thread_calc* td = (thread_calc*)arg;

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
        if (buffer[strlen(buffer) - 1] == '\n') {
            buffer[strlen(buffer) - 1] = '\0';
        }

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

        // Imposto i dati della richiesta
        td->sh->operando1 = value;
        td->sh->operando2 = atoll(buffer + 2);
        td->sh->id_richiedente = td->thread_n;
        td->sh->op_executed = false;  // Resetto il flag prima della nuova operazione

        if (buffer[0] == '+') {
            td->sh->op = ADD;
            if (pthread_cond_signal(&td->sh->add) != 0) {
                perror("pthread_cond_signal");
                exit(EXIT_FAILURE);
            }
        }
        else if (buffer[0] == '-') {
            td->sh->op = SUB;
            if (pthread_cond_signal(&td->sh->sub) != 0) {
                perror("pthread_cond_signal");
                exit(EXIT_FAILURE);
            }
        }
        else if (buffer[0] == 'x') {
            td->sh->op = MUL;
            if (pthread_cond_signal(&td->sh->mul) != 0) {
                perror("pthread_cond_signal");
                exit(EXIT_FAILURE);
            }
        }

        // Attende che l'operatore esegua la richiesta
        while (!td->sh->risultato_ready) {
            if (pthread_cond_wait(&td->sh->calc[td->sh->id_richiedente], &td->sh->lock) != 0) {
                perror("pthread_cond_wait");
                exit(EXIT_FAILURE);
            }
        }

        // Una volta ricevuto il risultato, aggiorno il valore e resetto il flag di "risultato_ready"
        value = td->sh->risultato;
        printf("[CALC-%u] risultato ricevuto: %lld\n", td->thread_n + 1, value);
        td->sh->risultato_ready = false;

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
    }
    else {
        printf("[CALC-%u] computazione terminata in modo errato: %lld\n", td->thread_n + 1, value);
    }

    // Se tutti i thread hanno finito, segnalo la terminazione agli operatori
    if (td->sh->nthread_done == td->ncalc) {
        td->sh->op = DONE;
        td->sh->done = true;
        if (pthread_cond_broadcast(&td->sh->add) != 0) {
            perror("pthread_cond_signal");
            exit(EXIT_FAILURE);
        }
        if (pthread_cond_broadcast(&td->sh->sub) != 0) {
            perror("pthread_cond_signal");
            exit(EXIT_FAILURE);
        }
        if (pthread_cond_broadcast(&td->sh->mul) != 0) {
            perror("pthread_cond_signal");
            exit(EXIT_FAILURE);
        }
    }

    if (pthread_mutex_unlock(&td->sh->lock) != 0) {
        perror("pthread_mutex_unlock");
        exit(EXIT_FAILURE);
    }
}

void add_function(void* arg) {
    thread_add* td = (thread_add*)arg;

    while (1) {
        if (pthread_mutex_lock(&td->sh->lock) != 0) {
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }

        while (td->sh->op != ADD && !td->sh->done) {
            if (pthread_cond_wait(&td->sh->add, &td->sh->lock) != 0) {
                perror("pthread_cond_wait");
                exit(EXIT_FAILURE);
            }
        }

        if (td->sh->done) {
            if (pthread_mutex_unlock(&td->sh->lock) != 0) {
                perror("pthread_mutex_unlock");
                exit(EXIT_FAILURE);
            }
            break;
        }

        // Esegue l'operazione solo se non è già stata eseguita
        if (!td->sh->op_executed) {
            td->sh->risultato = td->sh->operando1 + td->sh->operando2;
            printf("[ADD] calcolo effettuato: %lld + %lld = %lld\n", td->sh->operando1, td->sh->operando2, td->sh->risultato);
            td->sh->op_executed = true; // segnalo che l'operazione è stata eseguita
            td->sh->risultato_ready = true;
            if (pthread_cond_signal(&td->sh->calc[td->sh->id_richiedente]) != 0) {
                perror("pthread_cond_signal");
                exit(EXIT_FAILURE);
            }
        }

        if (pthread_mutex_unlock(&td->sh->lock) != 0) {
            perror("pthread_mutex_unlock");
            exit(EXIT_FAILURE);
        }
    }
}

void sub_function(void* arg) {
    thread_sub* td = (thread_sub*)arg;

    while (1) {
        if (pthread_mutex_lock(&td->sh->lock) != 0) {
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }

        while (td->sh->op != SUB && !td->sh->done) {
            if (pthread_cond_wait(&td->sh->sub, &td->sh->lock) != 0) {
                perror("pthread_cond_wait");
                exit(EXIT_FAILURE);
            }
        }

        if (td->sh->done) {
            if (pthread_mutex_unlock(&td->sh->lock) != 0) {
                perror("pthread_mutex_unlock");
                exit(EXIT_FAILURE);
            }
            break;
        }

        if (!td->sh->op_executed) {
            td->sh->risultato = td->sh->operando1 - td->sh->operando2;
            printf("[SUB] calcolo effettuato: %lld - %lld = %lld\n", td->sh->operando1, td->sh->operando2, td->sh->risultato);
            td->sh->op_executed = true;
            td->sh->risultato_ready = true;
            if (pthread_cond_signal(&td->sh->calc[td->sh->id_richiedente]) != 0) {
                perror("pthread_cond_signal");
                exit(EXIT_FAILURE);
            }
        }

        if (pthread_mutex_unlock(&td->sh->lock) != 0) {
            perror("pthread_mutex_unlock");
            exit(EXIT_FAILURE);
        }
    }
}

void mul_function(void* arg) {
    thread_mul* td = (thread_mul*)arg;

    while (1) {
        if (pthread_mutex_lock(&td->sh->lock) != 0) {
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }

        while (td->sh->op != MUL && !td->sh->done) { 
            if (pthread_cond_wait(&td->sh->mul, &td->sh->lock) != 0) {
                perror("pthread_cond_wait");
                exit(EXIT_FAILURE);
            }
        }

        if (td->sh->done) {
            if (pthread_mutex_unlock(&td->sh->lock) != 0) {
                perror("pthread_mutex_unlock");
                exit(EXIT_FAILURE);
            }
            break;
        }

        if (!td->sh->op_executed) {
            td->sh->risultato = td->sh->operando1 * td->sh->operando2;
            printf("[MUL] calcolo effettuato: %lld x %lld = %lld\n", td->sh->operando1, td->sh->operando2, td->sh->risultato);
            td->sh->op_executed = true;
            td->sh->risultato_ready = true;
            if (pthread_cond_signal(&td->sh->calc[td->sh->id_richiedente]) != 0) {
                perror("pthread_cond_signal");
                exit(EXIT_FAILURE);
            }
        }

        if (pthread_mutex_unlock(&td->sh->lock) != 0) {
            perror("pthread_mutex_unlock");
            exit(EXIT_FAILURE);
        }
    }
}

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <calc1.txt> <calc2.txt> <...> <calcn.txt>\n", argv[0]);
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
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }

    thread_add* add = malloc(sizeof(thread_add));
    add->sh = sh;
    if (pthread_create(&add->tid, NULL, (void*)add_function, add) != 0) {
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    thread_sub* sub = malloc(sizeof(thread_sub));
    sub->sh = sh;
    if (pthread_create(&sub->tid, NULL, (void*)sub_function, sub) != 0) {
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    thread_mul* mul = malloc(sizeof(thread_mul));
    mul->sh = sh;
    if (pthread_create(&mul->tid, NULL, (void*)mul_function, mul) != 0) {
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }
    
    for (int i = 0; i < ncalc; i++) {
        if (pthread_join(cc[i].tid, NULL) != 0) {
            perror("pthread_join calc");
            exit(EXIT_FAILURE);
        }
    }

    if (pthread_join(add->tid, NULL) != 0) {
        perror("pthread_join add");
        exit(EXIT_FAILURE);
    }
    if (pthread_join(sub->tid, NULL) != 0) {
        perror("pthread_join sub");
        exit(EXIT_FAILURE);
    }
    if (pthread_join(mul->tid, NULL) != 0) {
        perror("pthread_join mul");
        exit(EXIT_FAILURE);
    }

    printf("[MAIN] verifiche completate con successo: %u/%d\n", sh->success, ncalc);
    shared_destroy(sh, ncalc);
    free(cc);
    free(add);
    free(sub);
    free(mul);
}
