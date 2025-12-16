#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>
#include <fcntl.h>

#define BUFFER_SIZE 100

typedef enum{INS, ADD, SUB, MUL, RES, DONE}operator;

typedef struct{
    long long operator_1;
    long long operator_2;
    operator op;
    long long risultato;
    bool done;
    int fase;

    pthread_mutex_t lock;
    pthread_cond_t cond_op, cond_calc;
}shared;

typedef struct{
    char* filename;
    unsigned op_n;
    pthread_t tid;
    shared* sh;
}thread_op1;

typedef struct{
    char* filename;
    unsigned op_n;
    pthread_t tid;
    shared* sh;
}thread_op2;

typedef struct{
    char* filename;
    unsigned op_n;
    pthread_t tid;
    shared* sh;
}thread_ops;

typedef struct{
    pthread_t tid;
    unsigned ncalc;
    shared* sh;
}thread_calc;

shared* init_shared(){
    shared* sh = malloc(sizeof(shared));

    sh->risultato = 0;
    sh->fase = sh->done = 0;

    if(pthread_mutex_init(&sh->lock, NULL) != 0){
        perror("pthread_mutex_init");
        exit(EXIT_FAILURE);
    }

    if(pthread_cond_init(&sh->cond_op, NULL) != 0){
        perror("pthread_cond_init");
        exit(EXIT_FAILURE);
    }

    if(pthread_cond_init(&sh->cond_calc, NULL) != 0){
        perror("Pthread_cond_init");
        exit(EXIT_FAILURE);
    }

    return sh;
}

void destroy_shared(shared* sh){
    pthread_mutex_destroy(&sh->lock);
    pthread_cond_destroy(&sh->cond_calc);
    pthread_cond_destroy(&sh->cond_op);
    free(sh);
}

void op1_function(void* arg){
    thread_op1* td = (thread_op1*)arg;

    char buffer[BUFFER_SIZE];
    FILE* f;

    printf("[OP1] leggo gli operandi dal file '%s'\n", td->filename);

    if((f = fopen(td->filename, "r")) == NULL){
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    while(fgets(buffer, BUFFER_SIZE, f)){
        if(buffer[strlen(buffer) - 1] == '\n'){
            buffer[strlen(buffer) - 1] = '\0';
        }

        if(pthread_mutex_lock(&td->sh->lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }

        while(td->sh->fase != 0){
            if(pthread_cond_wait(&td->sh->cond_op, &td->sh->lock) != 0){
                perror("pthread_cond_wait");
                exit(EXIT_FAILURE);
            }
        }

        td->sh->operator_1 = atoll(buffer);
        printf("[OP1] primo operando  n.%u: %lld\n", td->op_n + 1, td->sh->operator_1);
        td->sh->fase = 1;
       
        if(pthread_cond_broadcast(&td->sh->cond_op) != 0){
            perror("pthread_cond_signal");
            exit(EXIT_FAILURE);
        }

        if(pthread_mutex_unlock(&td->sh->lock) != 0){
            perror("pthread_mutex_unlock");
            exit(EXIT_FAILURE);
        }
    }
    fclose(f);
    printf("[OP1] termino\n");
}

void op2_function(void* arg){
    thread_op2* td = (thread_op2*)arg;

    char buffer[BUFFER_SIZE];
    FILE* f;

    printf("[OP2] leggo gli operandi dal file '%s'\n", td->filename);

    if((f = fopen(td->filename, "r")) == NULL){
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    while(fgets(buffer, BUFFER_SIZE, f)){
        if(buffer[strlen(buffer) - 1] == '\n'){
            buffer[strlen(buffer) - 1] = '\0';
        }

        if(pthread_mutex_lock(&td->sh->lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }

        while(td->sh->fase != 2){
            if(pthread_cond_wait(&td->sh->cond_op, &td->sh->lock) != 0){
                perror("pthread_cond_wait");
                exit(EXIT_FAILURE);
            }
        }

        td->sh->operator_2 = atoll(buffer);
        printf("[OP2] secondo operando  n.%u: %lld\n", td->op_n + 1, td->sh->operator_2);
        td->sh->fase = 3;

        if(pthread_cond_broadcast(&td->sh->cond_op) != 0){
            perror("pthread_cond_signal");
            exit(EXIT_FAILURE);
        }

        if(pthread_mutex_unlock(&td->sh->lock) != 0){
            perror("pthread_mutex_unlock");
            exit(EXIT_FAILURE);
        }
    }
    fclose(f);
    printf("[OP2] termino\n");
}

void ops_function(void* arg){
    thread_ops* td = (thread_ops*)arg;

    char buffer[BUFFER_SIZE];
    FILE* f;
    long long risultato_finale = 0;
    int counter = 1;
    printf("[OPS] leggo le operazioni e il  risultato finale atteso dal file '%s'\n", td->filename);

    if((f = fopen(td->filename, "r")) == NULL){
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    while(1){
        if(pthread_mutex_lock(&td->sh->lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }

        while(td->sh->fase != 1){
            if(pthread_cond_wait(&td->sh->cond_op, &td->sh->lock) != 0){
                perror("pthread_cond_wait");
                exit(EXIT_FAILURE);
            }
        }

        if(!fgets(buffer, BUFFER_SIZE, f)){
            if(pthread_mutex_unlock(&td->sh->lock) != 0){
                perror("pthread_mutex_unlock");
                exit(EXIT_FAILURE);
            }
            break;
        }

        if(buffer[0] != '+' && buffer[0] != '-' && buffer[0] != 'x'){
            risultato_finale = atoll(buffer);
            td->sh->fase = 5;
            td->sh->done = true;
            if(pthread_cond_broadcast(&td->sh->cond_op) != 0){
                perror("pthread_cond_signal");
                exit(EXIT_FAILURE);
            }
            if(pthread_mutex_unlock(&td->sh->lock) != 0){
                perror("pthread_mutex_unlock");
                exit(EXIT_FAILURE);
            }
            break;
        }

        switch(buffer[0]){
            case '+':
            td->sh->op = ADD;
            break;
            case '-':
            td->sh->op = SUB;
            break;
            case 'x':
            td->sh->op = MUL;
            break;
            default:
            fprintf(stderr, "[OPS] operazione non riconosciuta\n");
            exit(EXIT_FAILURE);
        }
        printf("[OPS] operazione n.%d: '%c'\n", td->op_n + 1, buffer[0]);
        td->sh->fase = 2;

        if(pthread_cond_broadcast(&td->sh->cond_op) != 0){
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

        while(td->sh->fase != 4){
            if(pthread_cond_wait(&td->sh->cond_calc, &td->sh->lock) != 0){
                perror("pthread_cond_wait");
                exit(EXIT_FAILURE);
            }
        }

        printf("[OPS] sommatoria dei risultati parziali dopo %d operazione/i: %lld\n", counter++, td->sh->risultato);
        td->sh->fase = 0;

        if(pthread_cond_broadcast(&td->sh->cond_op) != 0){
            perror("pthread_cond_signal");
            exit(EXIT_FAILURE);
        }

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

    while(td->sh->fase != 5){
        if(pthread_cond_wait(&td->sh->cond_op, &td->sh->lock) != 0){
            perror("pthread_cond_wait");
            exit(EXIT_FAILURE);
        }
    } 

    if(td->sh->risultato == risultato_finale){
        printf("[OPS] risultato finale atteso: %lld (corretto)\n", td->sh->risultato);
    }else{
        fprintf(stderr, "[OPS] risultato finale attesso: %lld (ERRATO)\n", td->sh->risultato);
    }

    if(pthread_mutex_unlock(&td->sh->lock) != 0){
        perror("pthread_mutex_unlock");
        exit(EXIT_FAILURE);
    }
    printf("[OPS] terminato");
}

void calc_function(void* arg){
    thread_calc* td = (thread_calc*)arg;

    while(1){
        if(pthread_mutex_lock(&td->sh->lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }

        while(td->sh->fase != 3 && !td->sh->done){
            if(pthread_cond_wait(&td->sh->cond_op, &td->sh->lock) != 0){
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

        long long temp = 0;
        switch(td->sh->op){
            case ADD:
            temp = td->sh->operator_1 + td->sh->operator_2;
            printf("[CALC] operazione minore n.%u: %lld + %lld = %lld\n", td->ncalc +1, td->sh->operator_1, td->sh->operator_2, temp);
            break;
            case SUB:
            temp = td->sh->operator_1 - td->sh->operator_2;
            printf("[CALC] operazione minore n.%u: %lld - %lld = %lld\n", td->ncalc +1, td->sh->operator_1, td->sh->operator_2, temp);
            break;
            case MUL:
            temp = td->sh->operator_1 * td->sh->operator_2;
            printf("[CALC] operazione minore n.%u: %lld x %lld = %lld\n", td->ncalc +1, td->sh->operator_1, td->sh->operator_2, temp);
            break;
            default:
            fprintf(stderr, "[CALC] operazione non riconosciuta\n");
            exit(EXIT_FAILURE);
        }
        td->sh->risultato += temp;
        td->sh->fase = 4;
        if(pthread_cond_signal(&td->sh->cond_calc) != 0){
            perror("pthread_cond_signal");
            exit(EXIT_FAILURE);
        }

        if(pthread_cond_broadcast(&td->sh->cond_op) != 0){
            perror("pthread_cond_signal");
            exit(EXIT_FAILURE);
        }

        if(pthread_mutex_unlock(&td->sh->lock) != 0){
            perror("pthread_mutex_unlock");
            exit(EXIT_FAILURE);
        }
    }
    printf("[CALC] termino\n");
}

int main(int argc, char** argv){
    if(argc != 4){
        fprintf(stderr, "Usage: %s <first-operands> <second-operandos> <operations>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    shared* sh = init_shared();
    thread_op1* td1 = malloc(sizeof(thread_op1));
    td1->filename = argv[1];
    td1->sh = sh;
    if(pthread_create(&td1->tid, NULL, (void*)op1_function, td1) != 0){
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    thread_op2* td2 = malloc(sizeof(thread_op2));
    td2->filename = argv[2];
    td2->sh = sh;
    if(pthread_create(&td2->tid, NULL, (void*)op2_function, td2) != 0){
        perror("pthread_Create");
        exit(EXIT_FAILURE);
    }

    thread_ops* op = malloc(sizeof(thread_ops));
    op->filename = argv[3];
    op->sh = sh;
    if(pthread_create(&op->tid, NULL, (void*)ops_function, op) != 0){
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    thread_calc* tdc = malloc(sizeof(thread_calc));
    tdc->sh = sh;
    if(pthread_create(&tdc->tid, NULL, (void*)calc_function, tdc) != 0){
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

    if(pthread_join(op->tid, NULL) != 0){
        perror("pthread_join");
        exit(EXIT_FAILURE);
    }

    if(pthread_join(tdc->tid, NULL) != 0){
        perror("pthread_join");
        exit(EXIT_FAILURE);
    }
    
    destroy_shared(sh);
    free(td1);
    free(td2);
    free(op);
    free(tdc);
    printf("[MAIN] termino il processo\n");
}