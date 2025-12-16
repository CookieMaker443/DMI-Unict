#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>

#define BUFFER_SIZE 100
#define STRING_SIZE 20

typedef struct{
    char nome_bambino[STRING_SIZE];
    char nome_regalo[STRING_SIZE];
    int comportamento;
    int costo_regalo;
    unsigned ndone;
    bool done;
    
    int nletters;
    int nbambini_buoni;
    int nbambini_cattivi;
    int total_price;

    sem_t sem_es, sem_bn, sem_ei, sem_ep, sem_ec;
}shared;

typedef struct{
    char* letter_file;
    int nletters;
    pthread_t tid;
    unsigned thread_n;
    shared* sh;
}thread_es;

typedef struct{
    pthread_t tid;
    shared* sh;
}thread_bn;

typedef struct{
    char* good_bads_file;
    pthread_t tid;
    shared* sh;
    char** names;
    int* comportamento;
    int nchild;
}thread_ei;

typedef struct{
    char* presents_file;
    pthread_t tid;
    shared* sh;
    char** presents;
    int* price;
    int npresents;
}thread_ep;

typedef struct{
    pthread_t tid;
    shared* sh;
}thread_ec;

shared* init_shared(){
    shared* sh = malloc(sizeof(shared));

    sh->ndone = sh->nbambini_buoni = sh->nbambini_cattivi = sh->nletters = sh->total_price = 0;
    sh->done = false;

    if(sem_init(&sh->sem_es, 0, 1) != 0){
        perror("sem_init");
        exit(EXIT_FAILURE);
    }

    if(sem_init(&sh->sem_bn, 0, 0) != 0){
        perror("sem_init");
        exit(EXIT_FAILURE);
    }

    if(sem_init(&sh->sem_ei, 0, 0) != 0){
        perror("sem_init");
        exit(EXIT_FAILURE);
    }   

    if(sem_init(&sh->sem_ep, 0, 0) != 0){
        perror("sem_init");
        exit(EXIT_FAILURE);
    }

    if(sem_init(&sh->sem_ec, 0, 0) != 0){
        perror("sem_init");
        exit(EXIT_FAILURE);
    }

    return sh;
}

void shared_destroy(shared* sh){
    sem_destroy(&sh->sem_es);
    sem_destroy(&sh->sem_bn);
    sem_destroy(&sh->sem_ei);
    sem_destroy(&sh->sem_ep);
    sem_destroy(&sh->sem_ec);
    free(sh);
}

void es_function(void* arg){
    thread_es* td = (thread_es*)arg;

    FILE* f;
    char buffer[BUFFER_SIZE];
    char* nome, * regalo;

    if((f = fopen(td->letter_file, "r")) == NULL){
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    printf("[ES%u] leggo le letterine dal file %s'\n", td->thread_n + 1, td->letter_file);

    while(fgets(buffer, BUFFER_SIZE, f)){
        if(buffer[strlen(buffer) - 1] == '\n'){
            buffer[strlen(buffer) - 1] = '\0';
        }

        if(sem_wait(&td->sh->sem_es) != 0){
            perror("sem_wait");
            exit(EXIT_FAILURE);
        }

        if((nome = strtok(buffer, ";")) != NULL && (regalo = strtok(NULL, ";")) != NULL){
            printf("[ES%u] il bambino '%s' desidera per natale desidera '%s'\n", td->thread_n +1, nome, regalo);
            strcpy(td->sh->nome_bambino, nome);
            strcpy(td->sh->nome_regalo, regalo);
        }

        if(sem_post(&td->sh->sem_bn) != 0){
            perror("sem_post");
            exit(EXIT_FAILURE);
        }
    }
    fclose(f);
    
    td->sh->ndone++;

    if(td->sh->ndone == td->nletters){
        td->sh->done = true;

        if(sem_post(&td->sh->sem_bn) != 0){
            perror("sem_post");
            exit(EXIT_FAILURE);
        } 

        if(sem_post(&td->sh->sem_ec) != 0){
            perror("sem_post");
            exit(EXIT_FAILURE);
        } 

        if(sem_post(&td->sh->sem_ep) != 0){
            perror("sem_post");
            exit(EXIT_FAILURE);
        } 

        if(sem_post(&td->sh->sem_ei) != 0){
            perror("sem_post");
            exit(EXIT_FAILURE);
        } 
    }
    printf("[ES%u] non ho più letterine da consegnare\n", td->thread_n +1);
}

void bn_function(void* arg){
    thread_bn* td = (thread_bn*)arg;

    while(1){
        if(sem_wait(&td->sh->sem_bn) != 0){
            perror("sem_Wait");
            exit(EXIT_FAILURE);
        }

        if(td->sh->done){
            break;
        }

        printf("[BN] come si è comportato il bambino '%s'?\n", td->sh->nome_bambino);
    
        if(sem_post(&td->sh->sem_ei) != 0){
            perror("sem_post");
            exit(EXIT_FAILURE);
        }
        
        if(sem_wait(&td->sh->sem_bn) != 0){
            perror("sem_wait");
            exit(EXIT_FAILURE);
        }

        if(td->sh->comportamento == 1){
            printf("[BN] il bambino '%s' riceverà il suo regalo '%s'\n", td->sh->nome_bambino, td->sh->nome_regalo);
            if(sem_post(&td->sh->sem_ep) != 0){
                perror("sem_post");
                exit(EXIT_FAILURE);
            }
        }
        else if(td->sh->comportamento == -1){
            printf("[BN] il bambino '%s' non riceverà alcun regalo quest'anno!\n", td->sh->nome_bambino);
            if(sem_post(&td->sh->sem_ec) != 0){
                perror("sem_post");
                exit(EXIT_FAILURE);
            }
        }
    } 
    printf("[BN] non ci sono più bambini da esaminare\n");
}

void ei_function(void* arg){
    thread_ei* td = (thread_ei*)arg;

    FILE* f;
    char buffer[BUFFER_SIZE];
    int size = 0;

    if((f = fopen(td->good_bads_file, "r")) == NULL){
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    while(fgets(buffer, BUFFER_SIZE, f)){
        size++;
    }
    rewind(f);

    td->names = malloc(sizeof(char*) * size);
    td->comportamento = malloc(sizeof(int) * size);
    int i = 0;
    char* nome, *comportamento;

    while(fgets(buffer, BUFFER_SIZE, f)){
        if(buffer[strlen(buffer) -1 ] == '\n'){
            buffer[strlen(buffer) - 1] = '\0';
        }

        if((nome = strtok(buffer, ";")) != NULL && (comportamento = strtok(NULL, ";")) != NULL){
            td->names[i] = malloc(sizeof(char) * strlen(nome) + 1);
            strcpy(td->names[i], nome);
            
            if(!strcmp(comportamento, "buono")){
                td->comportamento[i] = 1;
            }
            else{
                td->comportamento[i] = -1;
            }
        }
        i++;
    }
    fclose(f);
    td->nchild = i;

    while(1){
        if(sem_wait(&td->sh->sem_ei) != 0){
            perror("sem_Wait");
            exit(EXIT_FAILURE);
        }

        if(td->sh->done){
            break;
        }

        bool found = false;
        for(int i = 0; i < td->nchild; i++){
            if(!strcmp(td->names[i], td->sh->nome_bambino)){
                td->sh->comportamento = td->comportamento[i];
                found = true;
                break;
            }
        }
        
        if(found){
            if(td->sh->comportamento > 0){
                printf("[EI] il bambino '%s' è stato buono quest'anno\n", td->sh->nome_bambino);
            }else{
                printf("[EI] il bambino '%s' è stato cattivo quest'anno\n", td->sh->nome_bambino);
            }
        }else{
            printf("[EI] il bambino non è stato trovato\n");
            td->sh->comportamento = 0;
        }

        if(sem_post(&td->sh->sem_bn) != 0){
            perror("sem_post");
            exit(EXIT_FAILURE);
        }
    }

    for(int i = 0; i < td->nchild; i++){
        free(td->names[i]);
    }
    free(td->names);
    free(td->comportamento);
}

void ep_function(void* arg){
    thread_ep* td = (thread_ep*)arg;

    FILE* f;
    char buffer[BUFFER_SIZE];
    int size = 0;

    if((f = fopen(td->presents_file, "r")) == NULL){
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    while(fgets(buffer, BUFFER_SIZE, f)){
        size++;
    }
    rewind(f);

    td->presents = malloc(sizeof(char*) * size);
    td->price = malloc(sizeof(int) * size);
    int i = 0;
    char* regalo, *costo;

    while(fgets(buffer, BUFFER_SIZE, f)){
        if(buffer[strlen(buffer) - 1] == '\n'){
            buffer[strlen(buffer) - 1] = '\0';
        }

        if((regalo = strtok(buffer, ";")) != NULL && (costo = strtok(NULL, ";")) != NULL){
            td->presents[i] = malloc(sizeof(char) * strlen(regalo) + 1);
            strcpy(td->presents[i], regalo);
            td->price[i] = atoi(costo);
        }
        i++;
    }
    td->npresents = i;
    fclose(f);

    while(1){
        if(sem_wait(&td->sh->sem_ep) != 0){
            perror("sem_wait");
            exit(EXIT_FAILURE);
        }

        if(td->sh->done){
            break;
        }

        bool found = false;
        for(int i = 0; i < td->npresents; i++){
            if(!strcmp(td->presents[i], td->sh->nome_regalo)){
                td->sh->costo_regalo = td->price[i];
                found = true;
                break;
            }
        }

        if(found){
            printf("[EP] creo il regalo '%s' per il bambino '%s' al costo di %d EUR\n", td->sh->nome_regalo, td->sh->nome_bambino, td->sh->costo_regalo);
        }else{
            printf("[EP] non è stato trovato alcun regalo\n");
        }

        if(sem_post(&td->sh->sem_ec) != 0){
            perror("sem_post");
            exit(EXIT_FAILURE);
        } 
    }

    for(int i = 0; i < td->npresents; i++){
        free(td->presents[i]);
    }
    free(td->presents);
    free(td->price);
}

void ec_function(void* arg){
    thread_ec* td = (thread_ec*)arg;

    while (1){
        if(sem_wait(&td->sh->sem_ec) != 0){
            perror("sem_wait");
            exit(EXIT_FAILURE);
        }

        if(td->sh->done){
            printf("[EC] quest'anno abbiamo ricevuto %d richieste da %d bambini buoni e da %d cattivi con un costo totale di produzione di %d €\n", td->sh->nletters, td->sh->nbambini_buoni, td->sh->nbambini_cattivi, td->sh->total_price);
            break;
        }

        td->sh->nletters++;

        if(td->sh->comportamento > 0){
            td->sh->nbambini_buoni++;
            td->sh->total_price += td->sh->costo_regalo;
            printf("[EC] aggiornate le statistiche dei bambini buoni (%d) e dei costi totali (%d €)\n", td->sh->nbambini_buoni, td->sh->total_price);
        }else{
            td->sh->nbambini_cattivi++;
            printf("[EC] aggiorno le statistiche dei bambini cattivi (%d)\n", td->sh->nbambini_cattivi);
        }

        if(sem_post(&td->sh->sem_es) != 0){
            perror("sem_post");
            exit(EXIT_FAILURE);
        }
    }
    
}

int main(int argc, char** argv){
    if(argc < 4){
        fprintf(stderr, "Usage: %s <present-file> <goods-bads-file> <letters-file-1> [... Letter-files-n]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int nlettersfiles = argc - 3;
    shared* sh = init_shared();

    thread_es* es = malloc(sizeof(thread_es) * nlettersfiles);
    for(int i = 0; i <nlettersfiles; i++){
        es[i].thread_n = i + 1;
        es[i].sh = sh;
        es[i].letter_file = argv[i + 3];
        es[i].nletters = nlettersfiles;
        if(pthread_create(&es[i].tid, NULL, (void*)es_function, &es[i]) != 0){
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }

    thread_bn bn;
    bn.sh = sh;
    if(pthread_create(&bn.tid, NULL, (void*)bn_function, &bn) != 0){
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }
    
    thread_ei ei;
    ei.sh = sh;
    ei.good_bads_file = argv[2];
    if(pthread_create(&ei.tid, NULL, (void*)ei_function, &ei) != 0){
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }
    
    thread_ep ep;
    ep.sh = sh;
    ep.presents_file = argv[1];
    if(pthread_create(&ep.tid, NULL, (void*)ep_function, &ep) != 0){
        perror("pthread_Create");
        exit(EXIT_FAILURE);
    }

    thread_ec ec;
    ec.sh = sh;
    if(pthread_create(&ec.tid, NULL, (void*)ec_function, &ec) != 0){
        perror("pthread_Create");
        exit(EXIT_FAILURE);
    }

    for(int i = 0; i < nlettersfiles; i++){
        if(pthread_join(es[i].tid, NULL) != 0){
            perror("pthread_join");
            exit(EXIT_FAILURE);
        }
    }

    if(pthread_join(ep.tid, NULL) != 0){
        perror("pthread_join");
        exit(EXIT_FAILURE);
    }

    if(pthread_join(ec.tid, NULL) != 0){
        perror("pthread_join");
        exit(EXIT_FAILURE);
    }

    if(pthread_join(ei.tid, NULL) != 0){
        perror("pthread_join");
        exit(EXIT_FAILURE);
    }

    if(pthread_join(bn.tid, NULL) != 0){
        perror("pthread_join");
        exit(EXIT_FAILURE);
    }
    free(es);
    shared_destroy(sh);
}