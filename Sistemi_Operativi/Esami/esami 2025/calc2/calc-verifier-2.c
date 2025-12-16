#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>

#define BUFFER_SIZE 100

typedef enum { ADD  , SUB, MUL, DONE } operator;

typedef struct {
    long long operando_1;
    long long operando_2;
    long long risultato;
    operator op;
    int id_richiedente; // id del thread CALC che ha richiesto l'operazione
    int done;
    int success;

    pthread_mutex_t lock;
    pthread_mutex_t write_lock;
    sem_t add, sub, mul;
    sem_t* calc; // array di semafori, uno per ogni thread CALC
} shared;

typedef struct {
    pthread_t tid;
    unsigned thread_n;
    char* filename;
    int ncalc;
    shared* sh;
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

    sh->success = sh->done = 0;
    sh->id_richiedente = 0;
    sh->calc = malloc(sizeof(sem_t) * ncalc);

    for (int i = 0; i < ncalc; i++) {
        if(sem_init(&sh->calc[i], 0, 0) != 0) {
            perror("sem_init calc");
            exit(EXIT_FAILURE);
        }
    }

    if(pthread_mutex_init(&sh->lock, NULL) != 0) {
        perror("pthread_mutex_init lock");
        exit(EXIT_FAILURE);
    }

    if(pthread_mutex_init(&sh->write_lock, NULL) != 0) {
        perror("pthread_mutex_init write_lock");
        exit(EXIT_FAILURE);
    }
 
    if(sem_init(&sh->add, 0, 0) != 0) {
        perror("sem_init add");
        exit(EXIT_FAILURE);
    }
    if(sem_init(&sh->sub, 0, 0) != 0) {
        perror("sem_init sub");
        exit(EXIT_FAILURE);
    }
    if(sem_init(&sh->mul, 0, 0) != 0) {
        perror("sem_init mul");
        exit(EXIT_FAILURE);
    }

    return sh;
}

void shared_destroy(shared* sh, int ncalc) {
    for (int i = 0; i < ncalc; i++) {
        sem_destroy(&sh->calc[i]);
    }
    free(sh->calc);
    pthread_mutex_destroy(&sh->lock);
    pthread_mutex_destroy(&sh->write_lock);
    sem_destroy(&sh->add);
    sem_destroy(&sh->sub);
    sem_destroy(&sh->mul);
    free(sh);
}

void calc_function(void* arg) {
    thread_calc* td = (thread_calc*)arg;
    FILE* f;
    char buffer[BUFFER_SIZE];
    long long value;
    long long risultato;

    if((f = fopen(td->filename, "r")) == NULL) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    printf("[CALC-%u] file da verificare: '%s'\n", td->thread_n + 1, td->filename);

    if(fgets(buffer, BUFFER_SIZE, f)) {
        value = atoll(buffer);
        printf("[CALC-%u] valore iniziale della computazione: %lld\n", td->thread_n + 1, value);
    }

    while(fgets(buffer, BUFFER_SIZE, f)) {
        if(buffer[strlen(buffer) - 1] == '\n') {
            buffer[strlen(buffer) - 1] = '\0';
        }

        // Se la riga non contiene un'operazione, allora è la riga con il risultato atteso
        if(buffer[1] != ' ') {
            risultato = atoll(buffer);
            break;
        }

        printf("[CALC-%u] prossima operazione: '%s'\n", td->thread_n + 1, buffer);

        // Inizia sezione critica per depositare la richiesta
        if(pthread_mutex_lock(&td->sh->lock) != 0) {
            perror("pthread_mutex_lock lock");
            exit(EXIT_FAILURE);
        }

        if(pthread_mutex_lock(&td->sh->write_lock) != 0) {
            perror("pthread_mutex_lock write_lock");
            exit(EXIT_FAILURE);
        }

        td->sh->operando_1 = value;
        td->sh->operando_2 = atoll(buffer + 1);
        td->sh->id_richiedente = td->thread_n;
        
        if(buffer[0] == '+') {
            td->sh->op = ADD;
            if(sem_post(&td->sh->add) != 0) {
                perror("sem_post add");
                exit(EXIT_FAILURE);
            }
        }
        else if(buffer[0] == '-') {
            td->sh->op = SUB;
            if(sem_post(&td->sh->sub) != 0) {
                perror("sem_post sub");
                exit(EXIT_FAILURE);
            }
        }
        else if(buffer[0] == 'x') {
            td->sh->op = MUL;
            if(sem_post(&td->sh->mul) != 0) {
                perror("sem_post mul");
                exit(EXIT_FAILURE);
            }
        }

        if(pthread_mutex_unlock(&td->sh->write_lock) != 0) {
            perror("pthread_mutex_unlock write_lock");
            exit(EXIT_FAILURE);
        }

        if(sem_wait(&td->sh->calc[td->sh->id_richiedente]) != 0) {
            perror("sem_wait calc");
            exit(EXIT_FAILURE);
        }

        value = td->sh->risultato;
        printf("[CALC-%u] risultato ricevuto: %lld\n", td->thread_n + 1, value);

        if(pthread_mutex_unlock(&td->sh->lock) != 0) {
            perror("pthread_mutex_unlock lock");
            exit(EXIT_FAILURE);
        }
    }
    fclose(f);
    
    if(pthread_mutex_lock(&td->sh->lock) != 0) {
        perror("pthread_mutex_lock lock");
        exit(EXIT_FAILURE);
    }

    td->sh->done++;
    if(value == risultato) {
        td->sh->success++;
        printf("[CALC-%u] computazione terminata in modo corretto: %lld\n", td->thread_n + 1, value);
    }
    else {
        printf("[CALC-%u] computazione terminata in modo errato: %lld\n", td->thread_n + 1, value);
        exit(EXIT_FAILURE);
    }

    if(td->sh->done == td->ncalc) {
        td->sh->op = DONE;
        if(sem_post(&td->sh->add) != 0) {
            perror("sem_post add");
            exit(EXIT_FAILURE);
        }
        if(sem_post(&td->sh->sub) != 0) {
            perror("sem_post sub");
            exit(EXIT_FAILURE);
        }
        if(sem_post(&td->sh->mul) != 0) {
            perror("sem_post mul");
            exit(EXIT_FAILURE);
        }
    }

    if(pthread_mutex_unlock(&td->sh->lock) != 0) {
        perror("pthread_mutex_unlock lock");
        exit(EXIT_FAILURE);
    }
}

void add_function(void* arg){
    thread_add* td = (thread_add*)arg;
    
    while(1){
        if(sem_wait(&td->sh->add) != 0){
            perror("sem_wait");
            exit(EXIT_FAILURE);
        }

        if(pthread_mutex_lock(&td->sh->write_lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }

        if(td->sh->op == DONE){
            if(pthread_mutex_unlock(&td->sh->write_lock) != 0){
                perror("pthread_mutex_unlock");
                exit(EXIT_FAILURE);
            }
            break;
        }

        td->sh->risultato = td->sh->operando_1 + td->sh->operando_2;
        printf("[ADD] calcolo effettuato: %lld + %lld = %lld\n", td->sh->operando_1, td->sh->operando_2, td->sh->risultato);
    
        if(sem_post(&td->sh->calc[td->sh->id_richiedente]) != 0){
            perror("sem_post");
            exit(EXIT_FAILURE);
        }
    
        if(pthread_mutex_unlock(&td->sh->write_lock) != 0){
            perror("pthread_mutex_unlock");
            exit(EXIT_FAILURE);
        }
    }
}

void sub_function(void* arg){
    thread_sub* td = (thread_sub*)arg;
    
    while(1){
        if(sem_wait(&td->sh->sub) != 0){
            perror("sem_wait");
            exit(EXIT_FAILURE);
        }

        if(pthread_mutex_lock(&td->sh->write_lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }

        if(td->sh->op == DONE){
            if(pthread_mutex_unlock(&td->sh->write_lock) != 0){
                perror("pthread_mutex_unlock");
                exit(EXIT_FAILURE);
            }
            break;
        }

        td->sh->risultato = td->sh->operando_1 - td->sh->operando_2;
        printf("[SUB] calcolo effettuato: %lld - %lld = %lld\n", td->sh->operando_1, td->sh->operando_2, td->sh->risultato);
    
        if(sem_post(&td->sh->calc[td->sh->id_richiedente]) != 0){
            perror("sem_post");
            exit(EXIT_FAILURE);
        }
    
        if(pthread_mutex_unlock(&td->sh->write_lock) != 0){
            perror("pthread_mutex_unlock");
            exit(EXIT_FAILURE);
        }
    }
}

void mul_function(void* arg){
    thread_mul* td = (thread_mul*)arg;
    
    while(1){
        if(sem_wait(&td->sh->mul) != 0){
            perror("sem_wait");
            exit(EXIT_FAILURE);
        }

        if(pthread_mutex_lock(&td->sh->write_lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }

        if(td->sh->op == DONE){
            if(pthread_mutex_unlock(&td->sh->write_lock) != 0){
                perror("pthread_mutex_unlock");
                exit(EXIT_FAILURE);
            }
            break;
        }

        td->sh->risultato = td->sh->operando_1 * td->sh->operando_2;
        printf("[MUL] calcolo effettuato: %lld x %lld = %lld\n", td->sh->operando_1, td->sh->operando_2, td->sh->risultato);
    
        if(sem_post(&td->sh->calc[td->sh->id_richiedente]) != 0){
            perror("sem_post");
            exit(EXIT_FAILURE);
        }
    
        if(pthread_mutex_unlock(&td->sh->write_lock) != 0){
            perror("pthread_mutex_unlock");
            exit(EXIT_FAILURE);
        }
    }
}

int main(int argc, char** argv){
    if(argc < 3){
        fprintf(stderr, "Usage: %s <calc1.txt> <calc2.txt> <...> <calcn.txt>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int ncalc = argc - 1;
    shared* sh = init_shared(ncalc);
    thread_calc* cc = malloc(sizeof(thread_calc) * ncalc);
    for(int i = 0; i < ncalc; i++){
        cc[i].filename = argv[i + 1];
        cc[i].ncalc = ncalc;
        cc[i].sh = sh;
        cc[i].thread_n = i;
        if(pthread_create(&cc[i].tid, NULL, (void*)calc_function, &cc[i]) != 0){
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }

    thread_add* ad = malloc(sizeof(thread_add));
    ad->sh = sh;
    if(pthread_create(&ad->tid, NULL, (void*)add_function, ad) != 0){
        perror("pthread_create add");
        exit(EXIT_FAILURE);
    }

    thread_sub* sub = malloc(sizeof(thread_sub));
    sub->sh = sh;
    if(pthread_create(&sub->tid, NULL, (void*)sub_function, sub) != 0){
        perror("pthread_create sub");
        exit(EXIT_FAILURE);
    }

    thread_mul* mul = malloc(sizeof(thread_mul));
    mul->sh = sh;
    if(pthread_create(&mul->tid, NULL, (void*)mul_function, mul) != 0){
        perror("pthread_create mul");
        exit(EXIT_FAILURE);
    }

    if(pthread_join(ad->tid, NULL) != 0){
        perror("pthread_join add");
        exit(EXIT_FAILURE);
    }
    if(pthread_join(sub->tid, NULL) != 0){
        perror("pthread_join sub");
        exit(EXIT_FAILURE);
    }
    if(pthread_join(mul->tid, NULL) != 0){
        perror("pthread_join mul");
        exit(EXIT_FAILURE);
    }
    for(int i = 0; i < ncalc; i++){
        if(pthread_join(cc[i].tid, NULL) != 0){
            perror("pthread_join calc");
            exit(EXIT_FAILURE);
        }
    }

    printf("[MAIN] verifiche completate con successo: %d/%d\n", sh->success, ncalc);
    shared_destroy(sh, ncalc);
    free(cc);
    free(ad);
    free(sub);
    free(mul);
}