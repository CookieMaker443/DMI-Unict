#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <bits/pthreadtypes.h>
#include <pthread.h>
#define BUFFER_SIZE 2048

typedef enum{INS, ADD, SUB, MUL, RES, DONE} operator;

typedef struct{
    long long operando_1;
    long long operando_2;
    long long risultato;
    operator op;
    unsigned id_richiedente;
    unsigned done;
    unsigned success; 

    pthread_mutex_t lock;
    pthread_cond_t cond_calc;
    pthread_cond_t cond_add;
    pthread_cond_t cond_sub;
    pthread_cond_t cond_mul;
}shared;

typedef struct{
    pthread_t tid;
    unsigned thread_n;
    unsigned ncalc;
    char* input_file;

    shared* sh;
}thread_data;

shared* init_shared(){
    shared* shar = malloc(sizeof(shared));

    shar->done = shar->success = 0;
    shar->op = INS;

    if(pthread_mutex_init(&shar->lock, 0) != 0){
        perror("pthread_mutex_init");
        exit(EXIT_FAILURE);
    }

    if(pthread_cond_init(&shar->cond_calc, 0) != 0){
        perror("pthread_cond_init-calc");
        exit(EXIT_FAILURE);
    }

    if(pthread_cond_init(&shar->cond_add, 0) != 0){
        perror("pthread_cond_init-add");
        exit(EXIT_FAILURE);
    }

    if(pthread_cond_init(&shar->cond_sub, 0) != 0){
        perror("pthread_cond_init-sub");
        exit(EXIT_FAILURE);
    }

    if(pthread_cond_init(&shar->cond_mul, 0) != 0){
        perror("pthread_cond_init-mul");
        exit(EXIT_FAILURE);
    }

    return shar;
}

void shared_destroy(shared* sh){
    pthread_mutex_destroy(&sh->lock);
    pthread_cond_destroy(&sh->cond_calc);
    pthread_cond_destroy(&sh->cond_add);
    pthread_cond_destroy(&sh->cond_sub);
    pthread_cond_destroy(&sh->cond_mul);
    free(sh);
}

void calc_thread(void* arg){
    thread_data* td = (thread_data*)arg;
    FILE* f;
    char buffer[BUFFER_SIZE];
    long long totale;
    long long risultato;

    printf("[CALC-%u] file da verificare '%s'\n", td->thread_n + 1, td->input_file);

    if((f = fopen(td->input_file, "r")) == NULL){
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    if(fgets(buffer, BUFFER_SIZE, f)){
        totale = atoll(buffer);
        printf("[CALC-%u] valore iniziale della computazione: %lld\n", td->thread_n + 1, totale);
    }else{
        fprintf(stderr, "[CALC-%u] errore nella lettura del primo valore", td->thread_n + 1);
        exit(EXIT_FAILURE);
    }

    while(fgets(buffer, BUFFER_SIZE, f)){
        if(buffer[strlen(buffer) - 1] == '\n'){
            buffer[strlen(buffer) - 1] = '\0';
        }

        if(buffer[1] != ' '){
            risultato = atoll(buffer);
            break;
        }

        printf("[CALC-%u] prossima operazione '%s'\n", td->thread_n + 1, buffer);

        if(pthread_mutex_lock(&td->sh->lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }

        while(td->sh->op != INS){
            if(pthread_cond_wait(&td->sh->cond_calc, &td->sh->lock) != 0){
                perror("pthread_cond_wait");
                exit(EXIT_FAILURE);
            }
        }

        td->sh->id_richiedente = td->thread_n;
        td->sh->operando_1 = totale;
        td->sh->operando_2 = atoll(buffer + 2);

        if(buffer[0] == '+'){
            td->sh->op = ADD;

            if(pthread_cond_signal(&td->sh->cond_add) != 0){
                perror("pthread_cond_signal");
                exit(EXIT_FAILURE);
            }
        }
        else if(buffer[0] == '-'){
            td->sh->op = SUB;

            if(pthread_cond_signal(&td->sh->cond_sub) != 0){
                perror("pthread_cond_signal");
                exit(EXIT_FAILURE);
            }
        }
        else if(buffer[0] == 'x'){
            td->sh->op = MUL;

            if(pthread_cond_signal(&td->sh->cond_mul) != 0){
                perror("pthread_cond_signal");
                exit(EXIT_FAILURE);
            }
        }
        else{
            fprintf(stderr, "[CALC-%u] errore nel parsing del file in input\n", td->thread_n + 1);
            exit(EXIT_FAILURE);
        }

        if(pthread_mutex_unlock(&td->sh->lock) != 0){
            perror("pthread_mutex_unlock");
            exit(EXIT_FAILURE);
        }

        while(td->sh->op != RES || td->sh->id_richiedente != td->thread_n){
            if(pthread_cond_wait(&td->sh->cond_calc, &td->sh->lock) != 0){
                perror("pthreaad_cond_wait");
                exit(EXIT_FAILURE);
            } 
        }

        totale = td->sh->risultato;
        td->sh->op = INS;

        printf("[CALC-%u] risultato ricevuto %lld\n", td->thread_n + 1, totale);

        if(pthread_mutex_unlock(&td->sh->lock) != 0){
            perror("pthread_mutex_unlock");
            exit(EXIT_FAILURE);
        }
    }

    if(pthread_mutex_lock(&td->sh->lock) != 0){
        perror("pthread_mutex_lock");
        exit(EXIT_FAILURE);
    }

    while(td->sh->op != INS){
        if(pthread_cond_wait(&td->sh->cond_calc, &td->sh->lock) != 0){
            perror("pthread_cond_wait");
            exit(EXIT_FAILURE);
        }
    }

    td->sh->done++;

    if(totale == risultato){
        td->sh->success++;

        printf("[CALC-%u] computazione terminata in modo corretto: %lld\n", td->thread_n + 1, totale);
    }else{
        printf("[CALC%u] computazione terminata in modo non corretto: %lld\n", td->thread_n + 1, totale);
    }

    if(td->sh->done == td->ncalc){
        td->sh->op = DONE;

        if(pthread_cond_signal(&td->sh->cond_add) != 0){
            perror("pthread_cond_signal");
            exit(EXIT_FAILURE);
        }

        if(pthread_cond_signal(&td->sh->cond_sub) != 0){
            perror("pthread_cond_signal");
            exit(EXIT_FAILURE);
        }

        if(pthread_cond_signal(&td->sh->cond_mul) != 0){
            perror("pthread_cond_signal");
            exit(EXIT_FAILURE);
        }
    }else{
        if(pthread_cond_broadcast(&td->sh->cond_calc) != 0){
            perror("pthread_cond_broadcast");
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
        if(pthread_mutex_lock(&td->sh->lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }

        while(td->sh->op != ADD && td->sh->op != DONE){
            if(pthread_cond_wait(&td->sh->cond_add, &td->sh->lock) != 0){
                perror("pthread_cond_wait");
                exit(EXIT_FAILURE);
            }
        }

        if(td->sh->op == DONE){
            if(pthread_mutex_unlock(&td->sh->lock) != 0){
                perror("pthread_mutex_unlock");
                exit(EXIT_FAILURE);
            }
            break;
        }

        td->sh->risultato = td->sh->operando_1 + td->sh->operando_2;
        td->sh->op = RES;

        printf("[ADD] calcolo effettutato: %lld + %lld = %lld\n", td->sh->operando_1, td->sh->operando_2, td->sh->risultato);

        if(pthread_cond_broadcast(&td->sh->cond_calc) != 0){
            perror("pthread_cond_broadcast");
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
        if(pthread_mutex_lock(&td->sh->lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }

        while(td->sh->op != SUB && td->sh->op != DONE){
            if(pthread_cond_wait(&td->sh->cond_sub, &td->sh->lock) != 0){
                perror("pthread_cond_wait");
                exit(EXIT_FAILURE);
            }
        }

        if(td->sh->op == DONE){
            if(pthread_mutex_unlock(&td->sh->lock) != 0){
                perror("pthread_mutex_unlock");
                exit(EXIT_FAILURE);
            }
            break;
        }

        td->sh->risultato = td->sh->operando_1 - td->sh->operando_2;
        td->sh->op = RES;

        printf("[SUB] calcolo effettuato: %lld - %lld = %lld\n", td->sh->operando_1, td->sh->operando_2, td->sh->risultato);
    
        if(pthread_cond_broadcast(&td->sh->cond_calc) != 0){
            perror("pthread_cond_broadcast");
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
        if(pthread_mutex_lock(&td->sh->lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }

        while(td->sh->op != MUL && td->sh->op != DONE){
            if(pthread_cond_wait(&td->sh->cond_mul, &td->sh->lock) != 0){
                perror("pthread_cond_wait");
                exit(EXIT_FAILURE);
            }
        }

        if(td->sh->op == DONE){
            if(pthread_mutex_unlock(&td->sh->lock) != 0){
                perror("pthread_mutex_unlcok");
                exit(EXIT_FAILURE);
            }
            break;
        }

        td->sh->risultato = td->sh->operando_1 * td->sh->operando_2;
        td->sh->op = RES;

        printf("[MUL] calcolo effettuato: %lld x %lld = %lld\n", td->sh->operando_1, td->sh->operando_2, td->sh->risultato);

        if(pthread_cond_broadcast(&td->sh->cond_calc) != 0){
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
    if(argc < 2){
        fprintf(stderr, "USAGE: %s <calc-file-1> <calc-file-2> <...>\n", argv[0] );
        exit(EXIT_FAILURE);
    }

    thread_data td[3 + argc - 1];
    shared * sh = init_shared();

    for(int i = 0; i < argc - 1; i++){
        td[i].sh = sh;
        td[i].thread_n = i;
        td[i].input_file = argv[i + 1];
        td[i].ncalc = argc - 1;

        if(pthread_create(&td[i].tid, 0, (void*)calc_thread, &td[i]) != 0){
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }

    td[argc - 1].sh = sh;

    if(pthread_create(&td[argc - 1].tid, 0, (void* )add_thread, &td[argc -1]) != 0){
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    td[argc].sh = sh;

    if(pthread_create(&td[argc].tid, 0, (void* )sub_thread, &td[argc]) != 0){
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    td[argc + 1].sh = sh;

    if(pthread_create(&td[argc + 1].tid, 0, (void*)mul_thread, &td[argc + 1]) != 0){
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    for(int i = 0; i<= argc + 1; i++){
        if(pthread_join(td[i].tid, NULL) != 0){
            perror("pthread_join");
            exit(EXIT_FAILURE);
        }
    }

    printf("[MAIN] verifiche completate con successo: %u/%d\n", sh->success, argc - 1);
    
    shared_destroy(sh);
}