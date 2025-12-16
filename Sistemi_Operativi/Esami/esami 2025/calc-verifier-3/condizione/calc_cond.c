#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>

#define BUFFER_SIZE 100
#define MAX_REQUEST 5

typedef enum{ADD, SUB, MUL}operator;

typedef struct{
    long long operator1, operator2;
    operator op;
    unsigned thread_id;
}record;

typedef struct{
    record buffer[MAX_REQUEST];
    long long* vec;
    unsigned compleate_thread;
    unsigned done;
    int index_in, index_out;
    int size;
    int* fase; 

    pthread_mutex_t lock;
    pthread_cond_t full, empty;
    pthread_cond_t calc;
}shared;

typedef struct{
    unsigned thread_n;
    char* filename;
    unsigned ncalc;
    pthread_t tid;

    shared* sh;
}thread_data;

shared* init_shared(int nfile){
    shared* sh = malloc(sizeof(shared));

    sh->done = sh->size = sh->index_in = sh->index_out = sh->compleate_thread = 0;
    sh->vec = malloc(sizeof(long long) * nfile);
    sh->fase = malloc(sizeof(int) * nfile);

    for(int i = 0; i < nfile; i++){
        sh->fase[i] = 0;
    }

    if(pthread_mutex_init(&sh->lock, NULL) != 0){
        perror("pthread_mutex_init");
        exit(EXIT_FAILURE);
    }

    if(pthread_cond_init(&sh->full, NULL) != 0){
        perror("pthread_cond_init");
        exit(EXIT_FAILURE);
    }

    if(pthread_cond_init(&sh->empty, NULL) != 0){
        perror("pthread_cond_init");
        exit(EXIT_FAILURE);
    }

    if(pthread_cond_init(&sh->calc, NULL) != 0){
        perror("pthread_cond_init");
        exit(EXIT_FAILURE);
    }

    return sh;
}

void shared_destroy(shared* sh){
    pthread_mutex_destroy(&sh->lock);
    pthread_cond_destroy(&sh->full);
    pthread_cond_destroy(&sh->empty);
    pthread_cond_destroy(&sh->calc);
    free(sh->vec);
    free(sh);
}


void file_function(void* arg){
    thread_data* td = (thread_data*)arg;

    FILE* f;
    char buffer[BUFFER_SIZE];
    long long value;
    long long risultato_atteso;
    
    if((f = fopen(td->filename, "r")) == NULL){
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    printf("[FILE-%u] file da verificare: '%s'\n", td->thread_n + 1, td->filename);

    if(fgets(buffer, BUFFER_SIZE, f)){
        value = atoll(buffer);
        printf("[FILE-%u] valore iniziale della computazione: %lld\n", td->thread_n + 1, value);
    }

    while(fgets(buffer, BUFFER_SIZE, f)){
        if(buffer[strlen(buffer) - 1] == '\n'){
            buffer[strlen(buffer) - 1] = '\0';
        }

        if(buffer[1] != ' '){
            risultato_atteso = atoll(buffer);
            break;
        }

        printf("[FILE-%u] prossima operazione: %s\n", td->thread_n + 1, buffer);

        if(pthread_mutex_lock(&td->sh->lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }

        while(td->sh->size == MAX_REQUEST){
            if(pthread_cond_wait(&td->sh->empty, &td->sh->lock) != 0){
                perror("pthread_cond_wait");
                exit(EXIT_FAILURE);
            }
        }

        td->sh->index_in = (td->sh->index_in + 1) % MAX_REQUEST;
        td->sh->size++;
        td->sh->buffer[td->sh->index_in].operator1 = value;
        td->sh->buffer[td->sh->index_in].operator2 = atoll(buffer + 2);
        td->sh->buffer[td->sh->index_in].thread_id = td->thread_n;

        if(buffer[0] == '+'){
            td->sh->buffer[td->sh->index_in].op = ADD;
        }else if(buffer[0] == '-'){
            td->sh->buffer[td->sh->index_in].op = SUB;
        }
        else if(buffer[0] == 'x'){
            td->sh->buffer[td->sh->index_in].op = MUL;
        }
        else{
            fprintf(stderr, "[FILE-%u] operazione non trovata\n", td->thread_n + 1);
            exit(EXIT_FAILURE);
        }

        td->sh->fase[td->thread_n] = 1;

        if(pthread_cond_signal(&td->sh->full) != 0){
            perror("pthread_cond_signal");
            exit(EXIT_FAILURE);
        }

        if(pthread_mutex_unlock(&td->sh->lock) != 0){
            perror("pthread_mutex_unlock");
            exit(EXIT_FAILURE);
        }

        if(pthread_mutex_lock(&td->sh->lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }

        while(td->sh->fase[td->thread_n] != 2){
            if(pthread_cond_wait(&td->sh->calc, &td->sh->lock) != 0){
                perror("phtread_cond_wait");
                exit(EXIT_FAILURE);
            }
        }

        value = td->sh->vec[td->thread_n];
        printf("[FILE-%u] risultato ricevuto: %lld\n", td->thread_n + 1, value);
        td->sh->fase[td->thread_n] = 0;

        if(pthread_mutex_unlock(&td->sh->lock) != 0){
            perror("pthread_mutex_unlock");
            exit(EXIT_FAILURE);
        }
    }
    fclose(f);

    if(pthread_mutex_lock(&td->sh->lock) != 0){
        perror("pthread_mutex_lock");
        exit(EXIT_FAILURE);
    }    

    td->sh->compleate_thread++;

    if(pthread_cond_signal(&td->sh->full) != 0){
        perror("pthread_cond_signal");
        exit(EXIT_FAILURE);
    }

    if(pthread_mutex_unlock(&td->sh->lock) != 0){
        perror("pthread_mutex_unlock");
        exit(EXIT_FAILURE);
    }

    if(pthread_mutex_lock(&td->sh->lock) != 0){
        perror("pthread_mutex_lock");
        exit(EXIT_FAILURE);
    }

    if(value == risultato_atteso){
        td->sh->done++;
        printf("[FILE-%u] computazione terminata in modo corretto: %lld\n", td->thread_n + 1, value);
    }else{
        printf("[FILE-%u] computazione terminata in modo errato: %lld\n", td->thread_n + 1, value);
    }

    if(pthread_mutex_unlock(&td->sh->lock) != 0){
        perror("pthread_mutex_unlock");
        exit(EXIT_FAILURE);
    }
}


void calc_function(void* arg){
    thread_data* td = (thread_data*)arg;

    while(1){
        if(pthread_mutex_lock(&td->sh->lock) != 0){
             perror("pthread_mutex_lock");
             exit(EXIT_FAILURE);
        }

        while(td->sh->size == 0 &&td->sh->compleate_thread < td->thread_n){
            if(pthread_cond_wait(&td->sh->full, &td->sh->lock) != 0){
                perror("pthread_cond_wait");
                exit(EXIT_FAILURE);
            }
        }

        if(td->sh->size == 0 && td->sh->compleate_thread == td->thread_n){
            if(pthread_mutex_unlock(&td->sh->lock) != 0){
                perror("pthread_mutex_unlock");
                exit(EXIT_FAILURE);
            }
            break;
        }

        td->sh->index_out = (td->sh->index_out + 1) % MAX_REQUEST;
        td->sh->size--;
        record rec = td->sh->buffer[td->sh->index_out];

        if(pthread_cond_signal(&td->sh->empty) != 0){
            perror("pthread_cond_signal");
            exit(EXIT_FAILURE);
        }

        if(pthread_mutex_unlock(&td->sh->lock) != 0){
            perror("pthread_mutex_unlock");
            exit(EXIT_FAILURE);
        }

        long long res;

        if(rec.op == ADD){
            res = rec.operator1 + rec.operator2;
            printf("[CALC] calcolo effettuato: %lld + %lld = %lld\n", rec.operator1 , rec.operator2, res);
        }
        else if(rec.op == SUB){
            res = rec.operator1 - rec.operator2;
            printf("[CALC] calcolo effettuato: %lld - %lld = %lld\n", rec.operator1, rec.operator2, res);
        }
        else if(rec.op == MUL){
            res = rec.operator1 * rec.operator2;
            printf("[CALC] calcolo effettuato: %lld x %lld = %lld\n", rec.operator1, rec.operator2, res);
        }

        if(pthread_mutex_lock(&td->sh->lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }

        td->sh->vec[rec.thread_id] = res;

        td->sh->fase[rec.thread_id] = 2;

        if(pthread_cond_broadcast(&td->sh->calc) != 0){
            perror("pthread_cond_broadcast");
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
        fprintf(stderr, "Usage: %s <calc-file-1.txt> <calc-file-2.txt> <...> <calc-file-n>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int ncalc = argc - 1;
    thread_data td[ncalc + 1];
    shared* sh = init_shared(ncalc);

    for(int i = 0; i < ncalc; i++){
        td[i].sh = sh;
        td[i].thread_n = i;
        td[i].filename = argv[i + 1];
        if(pthread_create(&td[i].tid, 0, (void*)file_function, &td[i]) != 0){
            perror("pthread_create file");
            exit(EXIT_FAILURE);
        }
    }
    // Creazione del thread CALC: lo posizioniamo in td[ncalc]
    td[ncalc].sh = sh;
    // In questo campo thread_n usiamo il numero totale dei file thread per il controllo di terminazione
    td[ncalc].thread_n = ncalc;
    if(pthread_create(&td[ncalc].tid, 0, (void*)calc_function, &td[ncalc]) != 0){
        perror("pthread_create calc");
        exit(EXIT_FAILURE);
    }
    // Attesa della terminazione di tutti i thread
    for(int i = 0; i < ncalc + 1; i++){
        if(pthread_join(td[i].tid, NULL) != 0){
            perror("pthread_join");
            exit(EXIT_FAILURE);
        }
    }
    printf("[MAIN] verifiche completate con successo: %u/%d\n", sh->done, ncalc);
    shared_destroy(sh);
}