#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include <semaphore.h>

#define BUFFER_SIZE 100

typedef enum{INS, ADD, SUB, MUL, DONE}operator;

typedef struct{
    long long operando1, operando2;
    operator op;
    long long risulato_corrente;
    bool done;
    int fase;

    sem_t read, calc;
}shared;

typedef struct{
    char* first_operands;
    pthread_t tid;
    shared* sh;
}thread_op1;

typedef struct{
    char* second_operands;
    pthread_t tid;
    shared* sh;
}thread_op2;

typedef struct{
    char* operations;
    pthread_t tid;
    shared* sh;
}thread_ops;

typedef struct{
    pthread_t tid;
    shared* sh;
}thread_calc;

shared* init_shared(){
    shared* sh = malloc(sizeof(shared));

    sh->fase = 0;
    sh->op = INS;
    sh->done = false;
    
    if(sem_init(&sh->read, 0 , 1) != 0){
        perror("sem_init");
        exit(EXIT_FAILURE);
    }

    if(sem_init(&sh->calc, 0, 0) != 0){
        perror("sem_init");
        exit(EXIT_FAILURE);
    }

    return sh;
}

void shared_destroy(shared* sh){
    sem_destroy(&sh->read);
    sem_destroy(&sh->calc);
    free(sh);
}

void op1_function(void* arg){
    thread_op1* td = (thread_op1*)arg;

    FILE* f;
    char buffer[BUFFER_SIZE];
    int counter = 1;
    if((f = fopen(td->first_operands, "r")) == NULL){
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    printf("[OP1] leggo gli operandi dal file '%s'\n", td->first_operands);

    while(fgets(buffer, BUFFER_SIZE, f)){
        if(buffer[strlen(buffer) -1 ] == '\n'){
            buffer[strlen(buffer) - 1] = '\0';
        }

        if(sem_wait(&td->sh->read) != 0){
            perror("sem_wait");
            exit(EXIT_FAILURE);
        }

        while(td->sh->fase != 0){
            if(sem_post(&td->sh->read) != 0){
                perror("sem_post");
                exit(EXIT_FAILURE);
            }
        }
        
        td->sh->operando1 = atoll(buffer);
        printf("[OP1] primo operando n.%d: %llu\n", counter, td->sh->operando1);
        td->sh->fase = 1;

        if(sem_post(&td->sh->read) != 0){
            perror("sem_post");
            exit(EXIT_FAILURE);
        }
        counter++;
    }
    fclose(f);
    printf("[OP1] termino\n");
}

void ops_function(void* arg){
    thread_ops* td = (thread_ops*)arg;

    FILE* f;
    char buffer[BUFFER_SIZE];
    int  counter = 1;
    int counter_op = 1;
    long long risultato = 0;
    long long value = 0;

    if((f = fopen(td->operations, "r")) == NULL){
        perror("fopen");
        exit(EXIT_FAILURE);
    }   

    printf("[OPS]  leggo le operazioni e il risultato atteso dal file %s\n", td->operations);
    
    while(fgets(buffer, BUFFER_SIZE, f)){
        if(buffer[strlen(buffer) - 1] == '\n'){
            buffer[strlen(buffer) - 1]  = '\0';
        }

        if(sem_wait(&td->sh->read) != 0){
            perror("sem_wait");
            exit(EXIT_FAILURE);
        }

        while(td->sh->fase != 1){
            if(sem_post(&td->sh->read) != 0){
                perror("sem_post");
                exit(EXIT_FAILURE);
            }
        }
        

        if(buffer[0] != '+' && buffer[0] != '-' && buffer[0] != 'x'){
            risultato = atoll(buffer);
            td->sh->done = true;
            break;
        }

        if(buffer[0] == '+'){
            td->sh->op = ADD;
        }
        else if(buffer[0] == '-'){
            td->sh->op = SUB;
        }
        else if(buffer[0] == 'x'){
            td->sh->op = MUL;
        }

        printf("[OPS] operazione n.%d: %c\n", counter, buffer[0]);
        td->sh->fase = 2;

        if(sem_post(&td->sh->read) != 0){
            perror("sem_post");
            exit(EXIT_FAILURE);
        }
        counter++;
        
        if(sem_wait(&td->sh->calc) != 0){
            perror("sem_wait");
            exit(EXIT_FAILURE);
        }

        if(sem_wait(&td->sh->read) != 0){
            perror("sem_wait");
            exit(EXIT_FAILURE);
        }

        if(td->sh->fase == 4){
            value += td->sh->risulato_corrente;
            printf("[OPS] sommatoria dei risultati parziali dopo %d operazione/i: %lld\n", counter, value);
            td->sh->fase = 0;
        }
        if(sem_post(&td->sh->read) != 0){
            perror("sem_post");
            exit(EXIT_FAILURE);
        }
    }
    fclose(f);


    if(value == risultato){
        printf("[OPS] risultato finale atteso: %lld (corretto)\n", value);
    } else {
        fprintf(stderr, "[OPS] risultato %lld (ERRATO)\n", value);
        exit(EXIT_FAILURE);
    }
    printf("[OPS] Termino\n");
}

void op2_function(void* arg){
    thread_op2* td = (thread_op2*)arg;

    FILE* f;
    char buffer[BUFFER_SIZE];
    int counter = 1;

    if((f = fopen(td->second_operands, "r")) == NULL){
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    printf("[OP2] leggo gli operandi dal file '%s'\n", td->second_operands);

    while(fgets(buffer, BUFFER_SIZE, f)){
        if(buffer[strlen(buffer) -1 ] == '\n'){
            buffer[strlen(buffer) - 1] = '\0';
        }

        if(sem_wait(&td->sh->read) != 0){
            perror("sem_wait");
            exit(EXIT_FAILURE);
        }

        while(td->sh->fase != 2){
            if(sem_post(&td->sh->read) != 0){
                perror("sem_post");
                exit(EXIT_FAILURE);
            }
        }

        td->sh->operando2 = atoll(buffer);
        printf("[OP2] secondo operando n.%d: %lld\n", counter, td->sh->operando2);
        td->sh->fase = 3;
        
        if(sem_post(&td->sh->read) != 0){
            perror("sem_post");
            exit(EXIT_FAILURE);
        }

        counter++;
    }
    fclose(f);
    printf("[OP2] termino\n");
}

void calc_function(void* arg){
    thread_calc* td = (thread_calc*)arg;
    int counter = 1;

    while(1){
        if(sem_wait(&td->sh->read) != 0){
            perror("sem_wait");
            exit(EXIT_FAILURE);
        }

        if(td->sh->done){
            if(sem_post(&td->sh->read) != 0){
                perror("sem_post");
                exit(EXIT_FAILURE);
            }
            break;
        }      

        if(td->sh->fase != 3){
            if(sem_post(&td->sh->read) != 0){
                perror("sem_post");
                exit(EXIT_FAILURE);
            }
            continue;
        }

        if(td->sh->op == ADD){
            td->sh->risulato_corrente = td->sh->operando1 + td->sh->operando2;
            printf("[CALC] operazione minore n.%d: %lld - %lld = %lld\n", counter, td->sh->operando1, td->sh->operando2, td->sh->risulato_corrente);
        }
        else if(td->sh->op == SUB){
            td->sh->risulato_corrente = td->sh->operando1 - td->sh->operando2;
            printf("[CALC] operazione minore n.%d: %lld - %lld = %lld\n", counter, td->sh->operando1, td->sh->operando2, td->sh->risulato_corrente);
        }
        else if(td->sh->op == MUL){
            td->sh->risulato_corrente = td->sh->operando1 * td->sh->operando2;
            printf("[CALC] operazione minore n.%d: %lld x %lld = %lld\n", counter, td->sh->operando1, td->sh->operando2, td->sh->risulato_corrente);
        }
            
        td->sh->fase = 4;
        
        if(sem_post(&td->sh->read) != 0){
            perror("sem_post");
            exit(EXIT_FAILURE);
        }

        if(sem_post(&td->sh->calc) != 0){
            perror("sem_post");
            exit(EXIT_FAILURE);
        }
        counter++;
    }
    printf("[CALC] Termino\n");
}

int main(int argc, char** argv){
    if(argc != 4){
        fprintf(stderr, "Usage: %s <first-operands> <second-operands> <operations>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    printf("[MAIN] creo i thread ausiliari\n");

    shared* sh = init_shared();

    thread_op1 op1;
    op1.first_operands = argv[1];
    op1.sh = sh;
    if(pthread_create(&op1.tid, NULL, (void*)op1_function, &op1) != 0){
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    thread_op2 op2;
    op2.second_operands = argv[2];
    op2.sh = sh;
    if(pthread_create(&op2.tid, NULL, (void*)op2_function, &op2) != 0){
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    thread_ops ops;
    ops.operations = argv[3];
    ops.sh = sh;
    if(pthread_create(&ops.tid, NULL, (void*)ops_function, &ops) != 0){
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    thread_calc calc;
    calc.sh = sh;
    if(pthread_create(&calc.tid, NULL, (void*)calc_function, &calc) != 0){
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    if(pthread_join(op1.tid, NULL) != 0){
        perror("pthread_join");
        exit(EXIT_FAILURE);
    }
    if(pthread_join(op2.tid, NULL) != 0){
        perror("pthread_join");
        exit(EXIT_FAILURE);
    }
    if(pthread_join(ops.tid, NULL) != 0){
        perror("pthread_join");
        exit(EXIT_FAILURE);
    }
    if(pthread_join(calc.tid, NULL) != 0){
        perror("pthread_join");
        exit(EXIT_FAILURE);
    }

    shared_destroy(sh);
    printf("[MAIN] termino il processo");
}