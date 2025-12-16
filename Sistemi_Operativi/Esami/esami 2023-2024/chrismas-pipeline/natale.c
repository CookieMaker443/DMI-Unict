#include <stdlib.h> 
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>

#define BUFFER_SIZE 100
#define MAX_PATH_SIZE 100
#define STRING_SIZE 20

typedef struct {
    char child_name[STRING_SIZE];
    char gift_name[STRING_SIZE];
    int comportamento; // 1 buono, -1 cattivo
    int price;       // costo del regalo singolo

    int letter_ready;
    bool done;

    int n_letters;
    int n_childs_good;
    int n_childs_bad;
    int total_price;

    pthread_mutex_t lock;
    pthread_cond_t cond_letter;   // ES segnala a Babbo Natale il nome e il regalo letti
    pthread_cond_t ei_cond;       // BN sveglia l'Elfo Indagatore per controllare il comportamento
    pthread_cond_t ep_cond;       // BN, se il bambino è buono, segnala l'Elfo Produttore
    pthread_cond_t ec_cond;       // BN, se il bambino è cattivo, segnala l'Elfo Contabile
    pthread_cond_t resp_ei_cond;  // EI segnala a BN che ha trovato il bambino e ne comunica il comportamento
    pthread_cond_t status_ec;     // EP segnala a EC il costo del regalo
} shared;

typedef struct {
    char* letter_filename;
    shared* sh;
    unsigned thread_n;
    pthread_t tid;
} thread_es; // Elfo smistatore

typedef struct {
    char* filename_good_bad;
    shared* sh;
    pthread_t tid;
    char** child_names;
    int* child_status;
    int n_child;
} thread_ei; // Elfo indagatore

typedef struct {
    pthread_t tid;
    shared* sh;
    char* present_filename;
    char** presents;
    int* costs;
    int n_presents;
} thread_ep; // Elfo produttore

typedef struct {
    pthread_t tid;
    shared* sh;
} thread_ec; // Elfo contabile

shared* init_shared(){
    shared* sh = malloc(sizeof(shared));
    sh->letter_ready = sh->n_childs_bad = sh->n_childs_good = sh->n_letters = sh->total_price = 0;
    sh->done = false;

    if(pthread_mutex_init(&sh->lock, NULL) != 0){
        perror("pthread_mutex_init");
        exit(EXIT_FAILURE);
    }
    if(pthread_cond_init(&sh->cond_letter, NULL) != 0){
        perror("pthread_cond_init");
        exit(EXIT_FAILURE);
    }
    if(pthread_cond_init(&sh->ei_cond, NULL) != 0){
        perror("pthread_cond_init");
        exit(EXIT_FAILURE);
    }
    if(pthread_cond_init(&sh->ep_cond, NULL) != 0){
        perror("pthread_cond_init");
        exit(EXIT_FAILURE);
    }
    if(pthread_cond_init(&sh->ec_cond, NULL) != 0){
        perror("pthread_cond_init");
        exit(EXIT_FAILURE);
    }
    if(pthread_cond_init(&sh->resp_ei_cond, NULL) != 0){
        perror("pthread_cond_init");
        exit(EXIT_FAILURE);
    }
    if(pthread_cond_init(&sh->status_ec, NULL) != 0){
        perror("pthread_cond_init");
        exit(EXIT_FAILURE);
    }
    return sh;  
}

void destroy_shared(shared* sh){
    pthread_mutex_destroy(&sh->lock);
    pthread_cond_destroy(&sh->cond_letter);
    pthread_cond_destroy(&sh->ei_cond);
    pthread_cond_destroy(&sh->ep_cond);
    pthread_cond_destroy(&sh->ec_cond);
    pthread_cond_destroy(&sh->resp_ei_cond);
    pthread_cond_destroy(&sh->status_ec);
    free(sh);
}

void es_thread_function(void* arg){
    thread_es* td = (thread_es*)arg;
    printf("[ES-%u] leggo le letterine del file '%s'\n", td->thread_n, td->letter_filename);

    FILE* f;
    char buffer[BUFFER_SIZE];
    char* name;
    char* gift;

    if((f = fopen(td->letter_filename, "r")) == NULL){
        perror("fopen");
        exit(EXIT_FAILURE); 
    }

    while(fgets(buffer, BUFFER_SIZE, f)){
        /* Rimuovo il newline se presente */
        if(buffer[strlen(buffer) - 1] == '\n'){
            buffer[strlen(buffer) - 1] = '\0';
        }
        if((name = strtok(buffer, ";")) != NULL && (gift = strtok(NULL, ";")) != NULL){
            if(pthread_mutex_lock(&td->sh->lock) != 0){
                perror("pthread_mutex_lock");
                exit(EXIT_FAILURE);
            }
            /* Attendo finché la shared è occupata */
            while(td->sh->letter_ready && !td->sh->done){
                if(pthread_cond_wait(&td->sh->cond_letter, &td->sh->lock) != 0){
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
            strcpy(td->sh->child_name, name);
            strcpy(td->sh->gift_name, gift);
            printf("[ES%u] il bambino '%s' desidera per Natale '%s'\n", td->thread_n, td->sh->child_name, td->sh->gift_name);
            td->sh->letter_ready = 1;
            if(pthread_cond_signal(&td->sh->cond_letter) != 0){
                perror("pthread_cond_signal");
                exit(EXIT_FAILURE);
            }
            if(pthread_mutex_unlock(&td->sh->lock) != 0){
                perror("pthread_mutex_unlock");
                exit(EXIT_FAILURE);
            }
        }
    }
    fclose(f);
}

void ei_thread_function(void* arg){
    thread_ei* td = (thread_ei*)arg;
    FILE* f;
    char buffer[BUFFER_SIZE];
    char* name;
    char* comportamento;
    int count = 0;

    if((f = fopen(td->filename_good_bad, "r")) == NULL){
        perror("fopen");
        exit(EXIT_FAILURE);
    }
    /* Conta il numero di righe */
    while(fgets(buffer, BUFFER_SIZE, f)){
        count++;
    }
    rewind(f);

    td->child_names = malloc(sizeof(char*) * count);
    td->child_status = malloc(sizeof(int) * count);
    td->n_child = 0;
    while(fgets(buffer, BUFFER_SIZE, f)){
        if(buffer[strlen(buffer) - 1] == '\n'){
            buffer[strlen(buffer) - 1] = '\0';
        }
        if((name = strtok(buffer, ";")) != NULL && (comportamento = strtok(NULL, ";")) != NULL){
            td->child_names[td->n_child] = malloc(strlen(name) + 1);
            strcpy(td->child_names[td->n_child], name);
            if(strcmp(comportamento, "buono") == 0){
                td->child_status[td->n_child] = 1;
            } else {
                td->child_status[td->n_child] = -1;
            }
            td->n_child++;
        }
    }
    fclose(f);
    printf("[EI] caricati %d elementi dal file goods-bads\n", td->n_child);

    while(1){
        if(pthread_mutex_lock(&td->sh->lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }
        while(!td->sh->letter_ready && !td->sh->done){
            if(pthread_cond_wait(&td->sh->ei_cond, &td->sh->lock) != 0){
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
            if(!strcmp(td->child_names[i], td->sh->child_name)){
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
        } else {
            printf("[EI] il bambino '%s' non è stato trovato\n", td->sh->child_name);
            td->sh->comportamento = 0;
        }
        if(pthread_cond_signal(&td->sh->resp_ei_cond) != 0){
            perror("pthread_cond_signal");
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

void ep_thread_function(void* arg){
    thread_ep* td = (thread_ep*)arg;
    FILE* f;
    char buffer[BUFFER_SIZE];
    char* present;
    char* s_price;
    int count = 0;

    if((f = fopen(td->present_filename, "r")) == NULL){
        perror("fopen");
        exit(EXIT_FAILURE);
    }
    /* Conta le righe */
    while(fgets(buffer, BUFFER_SIZE, f)){
        count++;
    }
    rewind(f);

    td->presents = malloc(sizeof(char*) * count);
    td->costs = malloc(sizeof(int) * count);
    td->n_presents = 0;
    while(fgets(buffer, BUFFER_SIZE, f)){
        if(buffer[strlen(buffer) - 1] == '\n'){
            buffer[strlen(buffer) - 1] = '\0';
        }
        if((present = strtok(buffer, ";")) != NULL && (s_price = strtok(NULL, ";")) != NULL){
            td->presents[td->n_presents] = malloc(strlen(present) + 1);
            strcpy(td->presents[td->n_presents], present);
            td->costs[td->n_presents] = atoi(s_price);
            td->n_presents++;
        }
    }
    fclose(f);
    printf("[EP] Caricati %d regali dal file presents\n", td->n_presents);

    while(1){
        if(pthread_mutex_lock(&td->sh->lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }
        while(!td->sh->letter_ready && !td->sh->done){
            if(pthread_cond_wait(&td->sh->ep_cond, &td->sh->lock) != 0){
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
                td->sh->price = td->costs[i];
                found = 1;
                break;
            }
        }
        if(found){
            printf("[EP] creo il regalo '%s' per il bambino '%s' al costo di %d euro\n", td->sh->gift_name, td->sh->child_name, td->sh->price);
        } else {
            printf("[EP] non è stato trovato nessun regalo\n");
            td->sh->price = 0;
        }
        if(pthread_cond_signal(&td->sh->status_ec) != 0){
            perror("pthread_cond_signal");
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

void ec_thread_function(void* arg){
    thread_ec* td = (thread_ec*)arg;
    while(1){
        if(pthread_mutex_lock(&td->sh->lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }
        while(!td->sh->letter_ready && !td->sh->done){
            if(pthread_cond_wait(&td->sh->ec_cond, &td->sh->lock) != 0){
                perror("pthread_cond_wait");
                exit(EXIT_FAILURE);
            }
        }
        if(td->sh->done){
            if(pthread_mutex_unlock(&td->sh->lock) != 0 ){
                perror("pthread_mutex_unlock");
                exit(EXIT_FAILURE);
            }
            break;
        }
        td->sh->n_letters++;
        if(td->sh->comportamento > 0){
            td->sh->n_childs_good++;
            td->sh->total_price += td->sh->price;
            printf("[EC] aggiornate le statistiche dei bambini buoni (%d) e dei costi totali (%d $)\n", td->sh->n_childs_good, td->sh->total_price);
        } else {
            td->sh->n_childs_bad++;
            printf("[EC] aggiorno le statistiche dei bambini cattivi (%d)\n", td->sh->n_childs_bad);
        }
        td->sh->letter_ready = 0;
        if(pthread_cond_broadcast(&td->sh->cond_letter) != 0){
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
    if(argc < 4){
        fprintf(stderr, "Usage: %s <presents-file> <goods-bads-file> <letter-file-1> <...> <letter-file-n>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    int n_letter = argc - 3;
    shared* sh = init_shared();

    thread_es* td_es = malloc(sizeof(thread_es) * n_letter);
    for(int i = 0; i < n_letter; i++){
        td_es[i].thread_n = i + 1;
        td_es[i].sh = sh;
        td_es[i].letter_filename = argv[i+3];
        if(pthread_create(&td_es[i].tid, NULL, (void*)es_thread_function, &td_es[i]) != 0){
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }

    thread_ei td_ei;
    td_ei.sh = sh;
    td_ei.filename_good_bad = argv[2];
    if(pthread_create(&td_ei.tid, NULL, (void*)ei_thread_function, &td_ei) != 0){
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    thread_ep td_ep;
    td_ep.sh = sh;
    td_ep.present_filename = argv[1];
    if(pthread_create(&td_ep.tid, NULL, (void*)ep_thread_function, &td_ep) != 0){
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    thread_ec td_ec;
    td_ec.sh = sh;
    if(pthread_create(&td_ec.tid, NULL, (void*)ec_thread_function, &td_ec) != 0){
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    /* Thread Babbo Natale */
    while(1){
        if(pthread_mutex_lock(&sh->lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }
        while(!sh->letter_ready && !sh->done){
            if(pthread_cond_wait(&sh->cond_letter, &sh->lock) != 0){
                perror("pthread_cond_wait");
                exit(EXIT_FAILURE);
            }
        }
        if(sh->done){
            if(pthread_mutex_unlock(&sh->lock) != 0){
                perror("pthread_mutex_unlock");
                exit(EXIT_FAILURE);
            }
            break;
        }
        printf("[BN] come si è comportato il bambino '%s'?\n", sh->child_name);
        if(pthread_cond_signal(&sh->ei_cond) != 0){
            perror("pthread_cond_signal");
            exit(EXIT_FAILURE);
        }
        while(sh->comportamento == 0 && !sh->done){
            if(pthread_cond_wait(&sh->resp_ei_cond, &sh->lock) != 0){
                perror("pthread_cond_wait");
                exit(EXIT_FAILURE);
            }
        }
        if(sh->comportamento > 0){
            printf("il bambino '%s' ricerverà il suo regalo '%s'\n", sh->child_name, sh->gift_name);
            if(pthread_cond_signal(&sh->ep_cond) != 0){
                perror("pthread_cond_signal");
                exit(EXIT_FAILURE);
            }
            while(sh->comportamento == 0 && !sh->done){
                if(pthread_cond_wait(&sh->status_ec, &sh->lock) != 0){
                    perror("pthread_cond_wait");
                    exit(EXIT_FAILURE);
                }
            }
        } else {
            printf("[BN] il bambino '%s' non riceverà alcun regalo quest'anno!\n", sh->child_name);
        }
        if(pthread_cond_signal(&sh->ec_cond) != 0){
            perror("pthread_cond_signal");
            exit(EXIT_FAILURE);
        }
        if(pthread_mutex_unlock(&sh->lock) != 0){
            perror("pthread_mutex_unlock");
            exit(EXIT_FAILURE);
        }
    }

    if(pthread_mutex_lock(&sh->lock) != 0){
        perror("pthread_mutex_lock");
        exit(EXIT_FAILURE);
    }
    sh->done = true;
    if(pthread_cond_broadcast(&sh->cond_letter) != 0){
        perror("pthread_cond_broadcast");
        exit(EXIT_FAILURE);
    }
    if(pthread_cond_broadcast(&sh->ei_cond) != 0){
        perror("pthread_cond_broadcast");
        exit(EXIT_FAILURE);
    }
    if(pthread_cond_broadcast(&sh->ep_cond) != 0){
        perror("pthread_cond_broadcast");
        exit(EXIT_FAILURE);
    }
    if(pthread_cond_broadcast(&sh->ec_cond) != 0){
        perror("pthread_cond_broadcast");
        exit(EXIT_FAILURE);
    }
    if(pthread_mutex_unlock(&sh->lock) != 0){
        perror("pthread_mutex_unlock");
        exit(EXIT_FAILURE);
    }

    for(int i = 0; i < n_letter; i++){
        if(pthread_join(td_es[i].tid, NULL) != 0){
            perror("pthread_join");
            exit(EXIT_FAILURE);
        }
    }
    if(pthread_join(td_ei.tid, NULL) != 0){
        perror("pthread_join");
        exit(EXIT_FAILURE);
    }
    if(pthread_join(td_ep.tid, NULL) != 0){
        perror("pthread_join");
        exit(EXIT_FAILURE);
    }
    if(pthread_join(td_ec.tid, NULL) != 0){
        perror("pthread_join");
        exit(EXIT_FAILURE);
    }
    free(td_es);
    destroy_shared(sh);
}
