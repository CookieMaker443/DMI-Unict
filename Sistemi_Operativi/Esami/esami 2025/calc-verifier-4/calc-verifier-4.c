#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>
#include <semaphore.h>
#include <pthread.h>

#define BUFFER_SIZE 100

typedef enum { INS, ADD, SUB, MUL, RES, DONE } operator;

typedef struct{
    long long operando_1;
    long long operando_2;
    operator operazione;
    long long risultato;
    long long sum;
    int fase;     // 0: OP1, 1: OPS legge operazione, 2: OP2, 3: CALC, 4: OPS stampa somma/fine
    bool done;
    sem_t read_s, write_s;
} shared;

typedef struct{
    pthread_t tid;
    shared* sh;
    char* filename;
} thread_op1;

typedef struct{
    pthread_t tid;
    shared* sh;
    char* filename;
} thread_op2;

typedef struct{
    pthread_t tid;
    shared* sh;
    char* filename;
} thread_ops;

typedef struct{
    pthread_t tid;
    shared* sh;
} thread_calc;

shared* init_shared(){
    shared* sh = malloc(sizeof(shared));
    sh->operazione = INS;
    sh->done = false;
    sh->sum = sh->fase = 0;
    if(sem_init(&sh->read_s, 0, 1) != 0){
        perror("sem_init");
        exit(EXIT_FAILURE);
    }
    if(sem_init(&sh->write_s, 0, 0) != 0){
        perror("sem_init");
        exit(EXIT_FAILURE);
    }
    return sh;
}

void shared_destroy(shared* sh){
    sem_destroy(&sh->read_s);
    sem_destroy(&sh->write_s);
    free(sh);
}

void op1_thread_function(void* arg){
    thread_op1* td = (thread_op1*)arg;
    char buffer[BUFFER_SIZE];
    long long value;
    FILE* f;
    printf("[OP1] leggo gli operandi dal file '%s'\n", td->filename);
    if((f = fopen(td->filename, "r")) == NULL){
        perror("fopen");
        exit(EXIT_FAILURE);
    }
    int counter = 0;
    while(fgets(buffer, BUFFER_SIZE, f)){
        if(buffer[strlen(buffer) - 1] == '\n'){
            buffer[strlen(buffer) - 1] = '\0';
        }
        if(sem_wait(&td->sh->read_s) != 0){
            perror("sem_wait");
            exit(EXIT_FAILURE);
        }
        while(td->sh->fase != 0){
            if(sem_post(&td->sh->read_s) != 0){
                perror("sem_post");
                exit(EXIT_FAILURE);
            }
        }
        value = atoll(buffer);
        td->sh->operando_1 = value;
        printf("[OP1] primo operando n.%d: %lld\n", counter+1, td->sh->operando_1);
        td->sh->fase = 1;  // passa a: OPS deve leggere operazione
        if(sem_post(&td->sh->read_s) != 0){
            perror("sem_post");
            exit(EXIT_FAILURE);
        }
        counter++;
    }
    printf("[OP1] termino\n");
    fclose(f);
}

void op2_thread_function(void* arg){
    thread_op2* td = (thread_op2*)arg;
    FILE* f;
    char buffer[BUFFER_SIZE];
    long long value;
    printf("[OP2] leggo gli operandi dal file '%s'\n", td->filename);
    if((f = fopen(td->filename, "r")) == NULL){
        perror("fopen");
        exit(EXIT_FAILURE);
    }
    int counter = 0;
    while(fgets(buffer, BUFFER_SIZE, f)){
        if(buffer[strlen(buffer) - 1] == '\n'){
            buffer[strlen(buffer) - 1] = '\0';
        }
        if(sem_wait(&td->sh->read_s) != 0){
            perror("sem_wait");
            exit(EXIT_FAILURE);
        }
        while(td->sh->fase != 2){
            if(sem_post(&td->sh->read_s) != 0){
                perror("sem_post");
                exit(EXIT_FAILURE);
            }
        }
        value = atoll(buffer);
        td->sh->operando_2 = value;
        printf("[OP2] secondo operando n.%d: %lld\n", counter+1, td->sh->operando_2);
        td->sh->fase = 3;  // passa a: CALC deve operare
        if(sem_post(&td->sh->read_s) != 0){
            perror("sem_post");
            exit(EXIT_FAILURE);
        }
        counter++;
    }
    printf("[OP2] termino\n");
    fclose(f);
}

void ops_thread_function(void* arg){
    thread_ops* td = (thread_ops*)arg;
    FILE* f;
    char buffer[BUFFER_SIZE];
    long long risultato_atteso;
    printf("[OPS] leggo le operazioni e il risultato atteso dal file '%s'\n", td->filename);
    if((f = fopen(td->filename, "r")) == NULL){
        perror("fopen");
        exit(EXIT_FAILURE);
    }
    int counter = 0;
    while(1){
        if(sem_wait(&td->sh->read_s) != 0){
            perror("sem_wait");
            exit(EXIT_FAILURE);
        }
        if(td->sh->fase != 1){
            if(sem_post(&td->sh->read_s) != 0){
                perror("sem_post");
                exit(EXIT_FAILURE);
            }
            continue;
        }
        if(!fgets(buffer, BUFFER_SIZE, f)){
            td->sh->done = true;
            break;
        }
        if(buffer[0] != '+' && buffer[0] != '-' && buffer[0] != 'x'){
            risultato_atteso = atoll(buffer);
            td->sh->done = true;
            td->sh->fase = 4;
            if(sem_post(&td->sh->read_s) != 0){
                perror("sem_post");
                exit(EXIT_FAILURE);
            }
            break;
        }
        if(buffer[0] == '+'){
            td->sh->operazione = ADD;
        }
        else if(buffer[0] == '-'){
            td->sh->operazione = SUB;
        }
        else if(buffer[0] == 'x'){
            td->sh->operazione = MUL;
        } else {
            fprintf(stderr,"[OPS] errore operazione nei file\n");
            exit(EXIT_FAILURE);
        }
        printf("[OPS] operazione n.%d: %c\n", counter+1, buffer[0]);
        td->sh->fase = 2; 
        if(sem_post(&td->sh->read_s) != 0){
            perror("sem_post");
            exit(EXIT_FAILURE);
        }
        counter++;
        if(sem_wait(&td->sh->write_s) != 0){
            perror("sem_wait");
            exit(EXIT_FAILURE);
        }
        if(sem_wait(&td->sh->read_s) != 0){
            perror("sem_wait");
            exit(EXIT_FAILURE);
        }
        if(td->sh->fase == 4){
            printf("[OPS] sommatoria dei risultati parziali dopo %d operazione/i: %lld\n", counter, td->sh->sum);
            td->sh->fase = 0; // inizia nuovo ciclo
        }
        if(sem_post(&td->sh->read_s) != 0){
            perror("sem_post");
            exit(EXIT_FAILURE);
        }
    }
    fclose(f);

    if(sem_post(&td->sh->read_s) != 0){
        perror("sem_post");
        exit(EXIT_FAILURE);
    }
    if(sem_wait(&td->sh->read_s) != 0){
        perror("sem_wait");
        exit(EXIT_FAILURE);
    }
    if(td->sh->sum == risultato_atteso){
        printf("[OPS] risultato finale atteso: %lld (corretto)\n", td->sh->sum);
    } else {
        fprintf(stderr, "[OPS] risultato %lld (ERRATO)\n", td->sh->sum);
        exit(EXIT_FAILURE);
    }
    if(sem_post(&td->sh->read_s) != 0){
        perror("sem_post");
        exit(EXIT_FAILURE);
    }
    printf("[OPS] termino\n");
}

void calc_thread(void* arg){
    thread_calc* td = (thread_calc*)arg;
    int counter = 0;
    while(1){
        if(sem_wait(&td->sh->read_s) != 0){
            perror("sem_wait");
            exit(EXIT_FAILURE);
        }
        if(td->sh->fase != 3){
            if(sem_post(&td->sh->read_s) != 0){
                perror("sem_post");
                exit(EXIT_FAILURE);
            }
            continue;
        }
        if(td->sh->done){
            if(sem_post(&td->sh->read_s) != 0){
                perror("sem_post");
                exit(EXIT_FAILURE);
            }
            break;
        }
        if(td->sh->operazione == ADD){
            td->sh->risultato = td->sh->operando_1 + td->sh->operando_2;
            td->sh->operazione = RES;
            printf("[CALC] operazione minore n.%d: %lld + %lld = %lld\n", counter+1, td->sh->operando_1, td->sh->operando_2, td->sh->risultato);
        }
        else if(td->sh->operazione == SUB){
            td->sh->risultato = td->sh->operando_1 - td->sh->operando_2;
            td->sh->operazione = RES;
            printf("[CALC] operazione minore n.%d: %lld - %lld = %lld\n", counter+1, td->sh->operando_1, td->sh->operando_2, td->sh->risultato);
        }
        else if(td->sh->operazione == MUL){
            td->sh->risultato = td->sh->operando_1 * td->sh->operando_2;
            td->sh->operazione = RES;
            printf("[CALC] operazione minore n.%d: %lld x %lld = %lld\n", counter+1, td->sh->operando_1, td->sh->operando_2, td->sh->risultato);
        }
        td->sh->sum += td->sh->risultato;
        counter++;
        td->sh->fase = 4;
        if(sem_post(&td->sh->read_s) != 0){
            perror("sem_post");
            exit(EXIT_FAILURE);
        }
        if(sem_post(&td->sh->write_s) != 0){
            perror("sem_post");
            exit(EXIT_FAILURE);
        }
    }    
    printf("[CALC] termino\n");
}

int main(int argc, char** argv){
    if(argc != 4){
        fprintf(stderr, "Usage: %s <first-operands> <second-operands> <operations>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    shared* sh = init_shared();
    thread_op1* td1 = malloc(sizeof(thread_op1));
    td1->sh = sh;
    td1->filename = argv[1];
    if(pthread_create(&td1->tid, NULL, (void*)op1_thread_function, td1) != 0){
        perror("Pthread_create");
        exit(EXIT_FAILURE);
    }
    thread_op2* td2 = malloc(sizeof(thread_op2));
    td2->sh = sh;
    td2->filename = argv[2];
    if(pthread_create(&td2->tid, NULL, (void*)op2_thread_function, td2) != 0){
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }
    thread_ops* ops = malloc(sizeof(thread_ops));
    ops->sh = sh;
    ops->filename = argv[3];
    if(pthread_create(&ops->tid, NULL, (void*)ops_thread_function, ops) != 0){
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }
    thread_calc* calc = malloc(sizeof(thread_calc));
    calc->sh = sh;
    if(pthread_create(&calc->tid, NULL, (void*)calc_thread, calc) != 0){
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }
    if(pthread_join(td1->tid, NULL) != 0){
        perror("pthread_join");
        exit(EXIT_FAILURE);
    }
    if(pthread_join(td2->tid, NULL) != 0){
        perror("pthread_join");
        exit(EXIT_FAILURE);
    }
    if(pthread_join(ops->tid, NULL) != 0){
        perror("pthread_join");
        exit(EXIT_FAILURE);
    }
    if(pthread_join(calc->tid, NULL) != 0){
        perror("pthread_join");
        exit(EXIT_FAILURE);
    }
    shared_destroy(sh);
    free(td1);
    free(td2);
    free(ops);
    free(calc);
    printf("[MAIN] termino il processo");
}
