#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>

#define BUFFER_SIZE 100
#define STRING_SIZE 20

typedef struct{
    char child_name[STRING_SIZE];
    char gift_name[STRING_SIZE];
    int comportamento; // 1 buono, -1 cattivo
    int costo;
    bool done;

    pthread_mutex_t lock;
    pthread_cond_t cond_es;
    pthread_cond_t cond_ei, cond_ep, cond_ec, cond_bn;

    int fase;
    int nlettere_ricevute;
    int nbambini_buoni;
    int nbambini_cattivi;
    int total_price;
}shared;

typedef struct{
    char* letter_filename;
    shared* sh;
    unsigned thread_n;
    pthread_t tid;
}thread_es;

typedef struct{
    char* filename_good_bad;
    shared* sh;
    pthread_t tid;
    char** child_names;
    int* child_status;
    int n_child;
}thread_ei;

typedef struct{
    pthread_t tid;
    shared* sh;
    char* present_filename;
    char** presents;
    int* costs;
    int n_presents;
}thread_ep;

typedef struct{
    pthread_t tid;
    shared* sh; 
} thread_ec;

typedef struct{
    pthread_t tid;
    shared* sh;
}thread_bn;

shared* init_shared(){
    shared* sh = malloc(sizeof(shared));
    sh->nlettere_ricevute = sh->nbambini_buoni = sh->nbambini_cattivi = sh->total_price = 0;
    sh->done = false;
    sh->fase = 0;

    if(pthread_mutex_init(&sh->lock, NULL) != 0){
        perror("pthread_mutex_init");
        exit(EXIT_FAILURE);
    }

    if(pthread_cond_init(&sh->cond_es, NULL) != 0){
        perror("pthread_cond_init");
        exit(EXIT_FAILURE);
    }

    if(pthread_cond_init(&sh->cond_ei, NULL) != 0){
        perror("pthread_cond_init");
        exit(EXIT_FAILURE);
    }

    if(pthread_cond_init(&sh->cond_ep, NULL) != 0){
        perror("pthread_cond_init");
        exit(EXIT_FAILURE);
    }
    
    if(pthread_cond_init(&sh->cond_ec, NULL) != 0){
        perror("pthread_cond_init");
        exit(EXIT_FAILURE);
    }
    
    if(pthread_cond_init(&sh->cond_bn, NULL) != 0){
        perror("pthread_cond_init");
        exit(EXIT_FAILURE);
    }
    
    return sh;
}

void shared_destroy(shared* sh){
    pthread_mutex_destroy(&sh->lock);
    pthread_cond_destroy(&sh->cond_es);
    pthread_cond_destroy(&sh->cond_ei);
    pthread_cond_destroy(&sh->cond_bn);
    pthread_cond_destroy(&sh->cond_ep);
    pthread_cond_destroy(&sh->cond_ec);
    free(sh);
}

void es_function(void* arg){
    thread_es* td = (thread_es*)arg;

    FILE* f;
    char buffer[BUFFER_SIZE];
    char* nome;
    char* regalo;

    if((f = fopen(td->letter_filename, "r")) == NULL){
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    printf("[ES%u] leggo le letterine dal file '%s'\n", td->thread_n, td->letter_filename);

    while(fgets(buffer, BUFFER_SIZE, f)){
        if(buffer[strlen(buffer) - 1] == '\n'){
            buffer[strlen(buffer) - 1] = '\0';
        } 
       
        if((nome = strtok(buffer, ";")) != NULL && (regalo = strtok(NULL,";")) != NULL){
            if(pthread_mutex_lock(&td->sh->lock) != 0){
                perror("pthread_mutex_lock");
                exit(EXIT_FAILURE);
            }
            
            while(td->sh->fase != 0){
                if(pthread_cond_wait(&td->sh->cond_es, &td->sh->lock) != 0){
                    perror("pthread_cond_wait");
                    exit(EXIT_FAILURE);
                }
            }

            strcpy(td->sh->child_name, nome);
            strcpy(td->sh->gift_name, regalo);
            printf("[ES%u] il bambino '%s' desidera per Natale '%s'\n", td->thread_n, nome, regalo);
            td->sh->fase = 1;
            if(pthread_cond_signal(&td->sh->cond_bn) != 0){
                perror("pthread_cond_signal");
                exit(EXIT_FAILURE);
            }

            if(pthread_mutex_unlock(&td->sh->lock) != 0){
                perror("Pthread_mutex_unlock");
                exit(EXIT_FAILURE);
            }
        }
    }
    fclose(f);
    printf("[ES%u] non ho più letterine da consegnare\n", td->thread_n);
}

void bn_function(void* arg){
    thread_bn* td = (thread_bn*)arg;

    while(1){
        if(pthread_mutex_lock(&td->sh->lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }

        while(td->sh->fase != 1 && !td->sh->done){
            if(pthread_cond_wait(&td->sh->cond_bn, &td->sh->lock) != 0){
                perror("Pthread_cond_wait");
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

        printf("[BN]: come si è comportato il bambino '%s'?\n", td->sh->child_name);
        td->sh->fase = 2;

        if(pthread_cond_signal(&td->sh->cond_ei) != 0){
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

        while(td->sh->fase != 3 && !td->sh->done){
            if(pthread_cond_wait(&td->sh->cond_bn, &td->sh->lock) != 0){
                perror("pthread_cond_wait");
                exit(EXIT_FAILURE);
            }
        }

        if(td->sh->comportamento == 1){ //bambino buono
            printf("[BN]  il bambino '%s' riceverà il suo regalo '%s'\n", td->sh->child_name, td->sh->gift_name);
            td->sh->fase = 4;
            if(pthread_cond_signal(&td->sh->cond_ep) != 0){
                perror("pthread_cond_signal");
                exit(EXIT_FAILURE);
            }
        }
        else if(td->sh->comportamento == -1){
            printf("[BN]  il bambino '%s' non riceverà alcun  regalo quest'anno\n", td->sh->child_name);
            td->sh->fase = 5;
            if(pthread_cond_signal(&td->sh->cond_ec) != 0){
                perror("pthread_cond_signal");
                exit(EXIT_FAILURE);
            }
        }

        if(pthread_mutex_unlock(&td->sh->lock) != 0){
            perror("pthread_mutex_unlock");
            exit(EXIT_FAILURE);
        }
    }
    printf("[BN]  non ci sono più bambini da esaminare\n");
}


void ei_function(void* arg){
    thread_ei* td = (thread_ei*)arg;

    FILE* f;
    char buffer[BUFFER_SIZE];
    int size = 0;

    if((f = fopen(td->filename_good_bad, "r")) == NULL){
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    while(fgets(buffer, BUFFER_SIZE, f)){
        size++;
    }
    rewind(f);

    td->child_names = malloc(sizeof(char*) * size);
    td->child_status = malloc(sizeof(int) * size);
    int i = 0;
    char* name, *comportamento;

    while(fgets(buffer, BUFFER_SIZE, f)){
        if(buffer[strlen(buffer) - 1] == '\n'){
            buffer[strlen(buffer) - 1] = '\0';
        }
        if((name = strtok(buffer, ";")) != NULL && (comportamento = strtok(NULL, ";")) != NULL){
            td->child_names[i] = malloc(sizeof(char) * strlen(name) + 1);
            strcpy(td->child_names[i], name);

            if(strcmp(comportamento, "buono") == 0){
                td->child_status[i] = 1;
            }else{
                td->child_status[i] = -1;
            }
        }
        i++;
    }
    td->n_child = i;
    fclose(f);
    printf("[EI] Pronto, letti %d elementi\n", td->n_child);

    while(1){
        if(pthread_mutex_lock(&td->sh->lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }

        while(td->sh->fase != 2 && !td->sh->done){
            if(pthread_cond_wait(&td->sh->cond_ei, &td->sh->lock) != 0){
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

        int found = 0;
        for(int i = 0; i < td->n_child; i++){
            if(!strcmp(td->child_names[i],  td->sh->child_name)){
                td->sh->comportamento = td->child_status[i];
                found = 1;
                break;
            }
        }

        if(found){
            if(td->sh->comportamento > 0){
                printf("[EI] il bambino '%s' è stato BUONO\n", td->sh->child_name);
            } else {
                printf("[EI] il bambino '%s' è stato cattivo\n", td->sh->child_name);
            }
        }else{
            printf("[EI] il bambino '%s' non è stato trovato\n", td->sh->child_name);
            td->sh->comportamento = 0;
        }
        td->sh->fase = 3;

        if(pthread_cond_signal(&td->sh->cond_bn) != 0){
            perror("Pthread_cond_signal");
            exit(EXIT_FAILURE);
        }

        if(pthread_mutex_unlock(&td->sh->lock) != 0){
            perror("pthread_mutex_unlock");
            exit(EXIT_FAILURE);
        }
    }

    for(int i = 0; i < td->n_child; i++){
        free(td->child_names[i]);
    }
    free(td->child_names);
    free(td->child_status);
}

void ep_function(void* arg){
    thread_ep* td = (thread_ep*)arg;

    FILE* f;
    char buffer[BUFFER_SIZE];
    int size = 0;

    if((f = fopen(td->present_filename, "r")) == NULL){
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    while(fgets(buffer, BUFFER_SIZE, f)){
        size++;
    }
    rewind(f);

    td->presents = malloc(sizeof(char*) * size);
    td->costs = malloc(sizeof(int) * size);
    int i = 0;
    char* regalo, *costo;

    while(fgets(buffer, BUFFER_SIZE, f)){
        if(buffer[strlen(buffer) - 1] == '\n'){
            buffer[strlen(buffer) - 1] = '\0';
        }
        if((regalo = strtok(buffer, ";")) != NULL && (costo = strtok(NULL, ";")) != NULL){
            td->presents[i] = malloc(sizeof(char) * strlen(regalo) + 1);
            strcpy(td->presents[i], regalo);
            td->costs[i] = atoi(costo);
        }   
        i++;
    }
    td->n_presents = i;
    fclose(f);
    printf("[EP] Caricati %d regali dal file presents\n", td->n_presents);

    while(1){
        if(pthread_mutex_lock(&td->sh->lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }

        while(td->sh->fase != 4 && !td->sh->done){
            if(pthread_cond_wait(&td->sh->cond_ep, &td->sh->lock) != 0){
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

        int found = 0;
        for(int i = 0; i < td->n_presents; i++){
            if(!strcmp(td->presents[i], td->sh->gift_name)){
                td->sh->costo = td->costs[i];
                found = 1;
                break;
            }
        }
        if(found){
            td->sh->fase = 5;
            printf("[EP]: creo il regalo '%s' per il bambino '%s' al costo di %d euro\n", td->sh->gift_name, td->sh->child_name, td->sh->costo);
        }else{
            printf("[EP]: non è stato trovato alcun regalo\n");
            td->sh->costo = 0;
        }

        if(pthread_cond_signal(&td->sh->cond_ec) != 0){
            perror("pthread_Cond_signal");
            exit(EXIT_FAILURE);
        }

        if(pthread_mutex_unlock(&td->sh->lock) != 0){
            perror("pthread_mutex_unlock");
            exit(EXIT_FAILURE);
        }
    }   

    for(int i = 0; i < td->n_presents; i++){
        free(td->presents[i]);
    }
    free(td->presents);
    free(td->costs);
}

void ec_function(void* arg){
    thread_ec* td = (thread_ec*)arg;

    printf("[EC] Pronto\n");
    while(1){
        if(pthread_mutex_lock(&td->sh->lock) != 0){
            perror("Pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }

        while(td->sh->fase != 5 && !td->sh->done){
            if(pthread_cond_wait(&td->sh->cond_ec, &td->sh->lock) != 0){
                perror("pthread_Cond_wait");
                exit(EXIT_FAILURE);
            }
        }

        if(td->sh->done){
            printf("[EC] quest'anno abbiamo ricevuto %d richieste da %d bambini buoni e da %d cattivi con un costo totale di produzione di %d €\n", td->sh->nlettere_ricevute, td->sh->nbambini_buoni, td->sh->nbambini_cattivi, td->sh->total_price);
            if(pthread_mutex_unlock(&td->sh->lock) != 0){
                perror("pthread_mutex_unlock");
                exit(EXIT_FAILURE);
            }
            break;
        }

        td->sh->nlettere_ricevute++;

        if(td->sh->comportamento > 0){
            td->sh->nbambini_buoni++;
            td->sh->total_price += td->sh->costo;
            printf("[EC] aggiornate le statistiche dei bambini buoni (%d) e dei costi totali (%d €)\n", td->sh->nbambini_buoni, td->sh->total_price);
        }else{
            td->sh->nbambini_cattivi++;
            printf("[EC] aggiorno le statistiche dei bambini cattivi (%d)\n", td->sh->nbambini_cattivi);
        }
        td->sh->fase = 0;

        if(pthread_cond_signal(&td->sh->cond_es) != 0){
            perror("pthread_cond_signal");
            exit(EXIT_FAILURE);
        }

        if(pthread_mutex_unlock(&td->sh->lock) != 0){
            perror("pthread_mutex_unlock");
            exit(EXIT_FAILURE);
        }
    }
}

int main(int argc, char** argv){
    if(argc < 4){
        fprintf(stderr, "Usage: %s <present-file> <good-bads-file> <letters-file-1> [... letters-file-n]\n", argv[0]);
        exit(EXIT_FAILURE);
    }
   
    int nlettersfiles = argc - 3;
    shared* sh = init_shared();

    thread_es* es = malloc(sizeof(thread_es) * nlettersfiles);
    for(int i = 0; i <nlettersfiles; i++){
        es[i].thread_n = i + 1;
        es[i].sh = sh;
        es[i].letter_filename = argv[i + 3];
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
    ei.filename_good_bad = argv[2];
    if(pthread_create(&ei.tid, NULL, (void*)ei_function, &ei) != 0){
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }
    
    thread_ep ep;
    ep.sh = sh;
    ep.present_filename = argv[1];
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

    if(pthread_mutex_lock(&sh->lock) != 0){
        perror("pthread_mutex_lock");
        exit(EXIT_FAILURE);
    }

    sh->done = true;

    if(pthread_cond_broadcast(&sh->cond_bn) != 0){
        perror("pthread_Cond_broadcast");
        exit(EXIT_FAILURE);
    }

    if(pthread_cond_broadcast(&sh->cond_ep) != 0){
        perror("pthread_Cond_broadcast");
        exit(EXIT_FAILURE);
    }

    if(pthread_cond_broadcast(&sh->cond_ei) != 0){
        perror("pthread_Cond_broadcast");
        exit(EXIT_FAILURE);
    }

    if(pthread_cond_broadcast(&sh->cond_ec) != 0){
        perror("pthread_Cond_broadcast");
        exit(EXIT_FAILURE);
    }

    if(pthread_mutex_unlock(&sh->lock) != 0){
        perror("pthread_mutex_unlock");
        exit(EXIT_FAILURE);
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