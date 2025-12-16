#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>

#define BUFFER_SIZE 100
#define MAX_REQUEST 5

typedef enum{INS, ADD, SUB, MUL}operator;

typedef struct{
    long long operando1, operando2;
    operator op;
    unsigned id_thread;
}record;

typedef struct{
    record buffer[MAX_REQUEST];
    long long* vec; //una per ogni thread file
    unsigned ndone;
    unsigned success;
    bool done;
    int index_in, index_out;
    int size;

    pthread_mutex_t lock;
    sem_t full, empty, calc, response;
}shared;

typedef struct{
    char* filename;
    unsigned thread_n;
    pthread_t tid;
    shared* sh;
    int ncalc;
}thread_file;

typedef struct{
    pthread_t tid;
    shared* sh;
}thread_calc;

shared* init_shared(int ncalc){
    shared* sh = malloc(sizeof(shared));

    sh->vec = malloc(sizeof(long long) * ncalc);
    for(int i = 0; i < ncalc; i++){
        sh->vec[i] = 0;
    }

    sh->ndone = sh->success = sh->index_in = sh->index_out = sh->size = 0;
    sh->done = false;

    if(pthread_mutex_init(&sh->lock, NULL) != 0){
        perror("pthread_mutex_init");
        exit(EXIT_FAILURE);
    }

    if(sem_init(&sh->full, 0, 0) != 0){
        perror("sem_init");
        exit(EXIT_FAILURE);
    }

    if(sem_init(&sh->empty, 0 , MAX_REQUEST) != 0){
        perror("sem_init");
        exit(EXIT_FAILURE);
    }

    if(sem_init(&sh->calc, 0, 0) != 0){
        perror("sem_init");
        exit(EXIT_FAILURE);
    }

    if(sem_init(&sh->response, 0, 0) != 0){
        perror("sem_init");
        exit(EXIT_FAILURE);
    }

    return sh;
}

void shared_destroy(shared* sh){
    pthread_mutex_destroy(&sh->lock);
    sem_destroy(&sh->full);
    sem_destroy(&sh->empty);
    sem_destroy(&sh->calc);
    sem_destroy(&sh->response);
    free(sh->vec);
    free(sh);
}

void file_function(void* arg){
    thread_file* td = (thread_file*)arg;

    FILE* f;
    char buffer[BUFFER_SIZE];
    long long value;
    long long risultato;

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
            risultato = atoll(buffer);
            break;
        }

        printf("[FILE-%u] prossima operazione: '%s'\n", td->thread_n + 1, buffer);

        if(sem_wait(&td->sh->empty) != 0){
            perror("sem_wait");
            exit(EXIT_FAILURE);
        }

        if(pthread_mutex_lock(&td->sh->lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }

       
        td->sh->index_in = (td->sh->index_in + 1) % MAX_REQUEST;
        td->sh->size++;
        td->sh->buffer[td->sh->index_in].operando1 = value;
        td->sh->buffer[td->sh->index_in].operando2 = atoll(buffer + 2);
        td->sh->buffer[td->sh->index_in].id_thread = td->thread_n;

        if(buffer[0] == '+'){
            td->sh->buffer[td->sh->index_in].op = ADD;
        }
        else if(buffer[0] == '-'){
            td->sh->buffer[td->sh->index_in].op = SUB;
        }
        else if(buffer[0] == 'x'){
            td->sh->buffer[td->sh->index_in].op = MUL;
        }  

        if(pthread_mutex_unlock(&td->sh->lock) != 0){
            perror("pthread_mutex_unlock");
            exit(EXIT_FAILURE);
        }

        if(sem_post(&td->sh->full) != 0){
            perror("sem_post");
            exit(EXIT_FAILURE);
        }

        if(sem_post(&td->sh->calc) != 0){
            perror("sem_post");
            exit(EXIT_FAILURE);
        }

        if(sem_wait(&td->sh->response) != 0){
            perror("sem_wait");
            exit(EXIT_FAILURE);
        }

        if(pthread_mutex_lock(&td->sh->lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }

        value = td->sh->vec[td->thread_n];
        printf("[FILE-%u] risultato ricevuto: %lld\n", td->thread_n + 1, value);

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

    td->sh->ndone++;

    if(value == risultato){
        td->sh->success++;
        printf("[FILE-%u] computazione terminata in modo corretto: %lld\n", td->thread_n + 1, value);
    }else{
        printf("[FILE-%u] computazione terminata in modo errato: %lld\n", td->thread_n + 1, value);
    }
    
    if(td->sh->ndone == td->ncalc){
        td->sh->done = true;
        if(sem_post(&td->sh->full) != 0){
            perror("sem_post");
            exit(EXIT_FAILURE);
        }
        if(sem_post(&td->sh->calc) != 0){
            perror("sem_post");
            exit(EXIT_FAILURE);
        }
    }

    if(pthread_mutex_unlock(&td->sh->lock) != 0){
        perror("pthread_mutex_unlock");
        exit(EXIT_FAILURE);
    }
}

void calc_function(void* arg){
    thread_calc* td = (thread_calc*)arg;

    while(1){
        if(sem_wait(&td->sh->full) != 0){
            perror("sem_wait");
            exit(EXIT_FAILURE);
        }

        if(sem_wait(&td->sh->calc) != 0){
            perror("sem_wait");
            exit(EXIT_FAILURE);
        }

        if(pthread_mutex_lock(&td->sh->lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }

        if(td->sh->done){
            if(pthread_mutex_unlock(&td->sh->lock) != 0){
                perror("pthread_mutex_unlock");
                exit(EXIT_FAILURE);
            }
            break;
        }

        td->sh->index_out = (td->sh->index_out + 1) % MAX_REQUEST;
        td->sh->size--;
        long long risultato = 0;
        if(td->sh->buffer[td->sh->index_out].op == ADD){
            risultato = td->sh->buffer[td->sh->index_out].operando1 + td->sh->buffer[td->sh->index_out].operando2;
            printf("[CALC] calcolo effettuato: %lld + %lld = %lld\n", td->sh->buffer[td->sh->index_out].operando1, td->sh->buffer[td->sh->index_out].operando2, risultato);
        }
        else if(td->sh->buffer[td->sh->index_out].op == SUB){
            risultato = td->sh->buffer[td->sh->index_out].operando1 - td->sh->buffer[td->sh->index_out].operando2;
            printf("[CALC] calcolo effettuato: %lld - %lld = %lld\n", td->sh->buffer[td->sh->index_out].operando1, td->sh->buffer[td->sh->index_out].operando2, risultato);
        }
        else if(td->sh->buffer[td->sh->index_out].op == MUL){
            risultato = td->sh->buffer[td->sh->index_out].operando1 * td->sh->buffer[td->sh->index_out].operando2;
            printf("[CALC] calcolo effettuato: %lld x %lld = %lld\n", td->sh->buffer[td->sh->index_out].operando1, td->sh->buffer[td->sh->index_out].operando2, risultato);
        }

        td->sh->vec[td->sh->buffer[td->sh->index_out].id_thread] = risultato;

        if(sem_post(&td->sh->response) != 0){
            perror("sem_post");
            exit(EXIT_FAILURE);
        }

        if(pthread_mutex_unlock(&td->sh->lock) != 0){
            perror("pthread_mutex_unlock");
            exit(EXIT_FAILURE);
        }

        if(sem_post(&td->sh->empty) != 0){
            perror("sem_post");
            exit(EXIT_FAILURE);
        }
    }
}

int main(int argc, char** argv){
    if(argc < 3){
        fprintf(stderr, "Usage: %s <calc-file-1> <calc-file-2> <...> <calc-file-n>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int ncalc = argc - 1;
    shared* sh = init_shared(ncalc);
    thread_file* td = malloc(sizeof(thread_file) * ncalc);

    for(int i = 0; i < ncalc; i++){
        td[i].filename = argv[i + 1];
        td[i].ncalc = ncalc;
        td[i].sh = sh;
        td[i].thread_n = i;
        if(pthread_create(&td[i].tid, NULL, (void*)file_function, &td[i]) != 0){
            perror("Phtread_create");
            exit(EXIT_FAILURE);
        }
    }

    thread_calc cl;
    cl.sh = sh;
    if(pthread_create(&cl.tid, NULL, (void*)calc_function, &cl) != 0){
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    for(int i = 0; i < ncalc; i++){
        if(pthread_join(td[i].tid, NULL) != 0){
            perror("pthread_join");
            exit(EXIT_FAILURE);
        }
    }

    if(pthread_join(cl.tid, NULL) != 0){
        perror("pthread_join");
        exit(EXIT_FAILURE);
    }

    printf("[MAIN] verifiche completate con successo: %d/%d\n", sh->success, ncalc);
    free(td);
    shared_destroy(sh);
}