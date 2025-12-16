#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>
#include <ctype.h>
#include <sys/types.h>

#define KEY_SIZE 30
#define BUFFER_SIZE 100

typedef struct{
    char buffer[BUFFER_SIZE];
    pthread_mutex_t lock;
    pthread_cond_t* read, *write;
    bool done; //Flag di fine
    bool* ready, *compleate; //flag di gestione delle variabili di condizione
}shared;

typedef struct{
    pthread_t tid;
    unsigned thread_n;
    char key[KEY_SIZE];
    shared* sh;
}thread_data;

shared* init_shared(int nkeys){
    shared* sh = malloc(sizeof(shared));

    sh->done = 0;

    if(pthread_mutex_init(&sh->lock, NULL) != 0){
        perror("pthread_mutex_init");
        exit(EXIT_FAILURE);
    }

    sh->read = malloc(sizeof(pthread_cond_t) * nkeys);
    sh->write = malloc(sizeof(pthread_cond_t) * nkeys);
    sh->ready = malloc(sizeof(bool) * nkeys);
    sh->compleate = malloc(sizeof(bool) * nkeys);


    for(int i = 0; i < nkeys; i++){
        if(pthread_cond_init(&sh->read[i], NULL) != 0){
            perror("Pthread_cond_init");
            exit(EXIT_FAILURE);
        }
        if(pthread_cond_init(&sh->write[i], NULL) != 0){
            perror("Pthread_cond_init");
            exit(EXIT_FAILURE);
        }

        sh->ready[i] = 0;
        sh->compleate[i] = 0;
    }

    return sh;
}
 
void shared_destroy(shared* sh, int nkeys){
    pthread_mutex_destroy(&sh->lock);
    for(int i = 0; i < nkeys; i++){
        pthread_cond_destroy(&sh->read[i]);
        pthread_cond_destroy(&sh->write[i]);
    }
    free(sh->ready);
    free(sh->compleate);
    free(sh->read);
    free(sh->write);
    free(sh);
}

char* decrypt(char* text, char* key){ //text contiene il testo cifrato, key è la chiave da analizzare 
    char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    int n = strlen(text);
    char* text_dec = malloc(sizeof(char) * (n + 1)); //buffer che contiene il testo decifrato
    for(int i = 0; i < n; i++){
        char* carattere_trovato = strchr(key, text[i]);
        if(carattere_trovato != NULL){
            int index = carattere_trovato - key;
            text_dec[i] = alphabet[index];
        }else{
            text_dec[i] = text[i];
        }
    }
    text_dec[n] = '\0';
    return text_dec;
}

int parse_text(char* text, char* testo_cifrato, int* key_index){
    int index = -1;
    int len = strlen(text);
    for(int i = 0; i < len; i++){
        if(text[i] == ':'){
            index = i;
            break;
        }
    }

    if(index < 0){
        return -1;
    }

    char* index_text = malloc(sizeof(char) * (index + 1));
    for(int i = 0; i < index; i++){
        index_text[i] = text[i];
    }
    index_text[index] = '\0';
    int j = 0;
    for(int i = index + 1; i < len && text[i] != '\n'; i++, j++){
        testo_cifrato[j] = text[i];
    }
    testo_cifrato[j] = '\0';

    *key_index = atoi(index_text);
    free(index_text);
    return 0;
}

void k_function(void* arg){
    thread_data* td = (thread_data*)arg;
    printf("[K%u] chiave assegnata: %s\n", td->thread_n + 1, td->key);
    char buffer[BUFFER_SIZE];

    while(1){
        if(pthread_mutex_lock(&td->sh->lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }

        while(!td->sh->ready[td->thread_n] && !td->sh->done){
            if(pthread_cond_wait(&td->sh->read[td->thread_n], &td->sh->lock) != 0){
                perror("Pthread_cond_wait");
                exit(EXIT_FAILURE);
            }
        }

        if(td->sh->done){
            if(pthread_mutex_unlock(&td->sh->lock) != 0){
                perror("Pthread_mutex_unlock");
                exit(EXIT_FAILURE);
            }
            break;
        }

        strcpy(buffer, td->sh->buffer);

        td->sh->ready[td->thread_n] = 0;
        int len = strlen(buffer);
        printf("[K%u] sto decifrando la frase di %d caratteri passata dal main\n", td->thread_n + 1, len);

        char* dec = decrypt(buffer, td->key);
        strcpy(td->sh->buffer, dec);
        free(dec);
        td->sh->compleate[td->thread_n] = 1;

        if(pthread_cond_signal(&td->sh->write[td->thread_n]) != 0){
            perror("Pthread_cond_signal");
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
        fprintf(stderr, "Usage: %s <key.txt> <cifrato.txt> <output.txt>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    FILE* f;
    char* keys = argv[1];
    char* cifrato = argv[2];
    char* output = argc > 3 ? argv[3] : "output.txt";
    
    if((f = fopen(keys, "r")) == NULL){
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    int nkeys = 0;
    char tmp[KEY_SIZE];
    while(fgets(tmp, KEY_SIZE, f) != NULL){
        // Se la riga non è vuota, incrementa il conteggio
        if(tmp[0] != '\n' && tmp[0] != '\0'){
            nkeys++;
        }
    }
    rewind(f);
    printf("[M] trovate %d chiavi, creo i thread k-i necessari\n", nkeys);

    shared* sh = init_shared(nkeys);
    thread_data* td = malloc(sizeof(thread_data) * nkeys);
    int j = 0;
    char key[KEY_SIZE];

    while(fgets(key, KEY_SIZE, f)){
        if(key[strlen(key) - 1] == '\n'){
            key[strlen(key) - 1] = '\0';
        }

        td[j].sh = sh;
        td[j].thread_n = j;
        strcpy(td[j].key, key);
        j++;
    }
    fclose(f);

    for(int i = 0; i < nkeys; i++){
        if(pthread_create(&td[i].tid, NULL, (void*)k_function, &td[i]) != 0){
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }

    FILE* out;
    
    if((out = fopen(output, "w")) == NULL){
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    printf("[M] Processo il file cifrato\n");
    FILE* c;

    if((c = fopen(cifrato, "r")) == NULL){
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    char buffer[BUFFER_SIZE];
    char text[BUFFER_SIZE];
    int key_index;

    while(fgets(buffer, BUFFER_SIZE, c)){
        if(buffer[strlen(buffer) - 1] == '\n'){
            buffer[strlen(buffer) - 1] = '\0';
        }

        if(parse_text(buffer, text, &key_index) != 0){
            perror("file cifrato non valido");
            exit(EXIT_FAILURE);
        } 

        if(key_index < 0 || key_index >= nkeys){
            fprintf(stderr, "[M] indici della chiave fuori range: %d\n", key_index + 1);
            exit(EXIT_FAILURE);
        }

        printf("[M] la riga '%s' deve essere decifrata con la chiave n.%d\n", text, key_index + 1);
        
        if(pthread_mutex_lock(&td->sh->lock) != 0){
            perror("Pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }

        strcpy(sh->buffer, text);
        sh->ready[key_index] = 1;
        sh->compleate[key_index] = 0;

        if(pthread_cond_signal(&td->sh->read[key_index])){
            perror("pthread_cond_signal");
            exit(EXIT_FAILURE);
        }

        while(!td->sh->compleate[key_index]){
            if(pthread_cond_wait(&td->sh->write[key_index], &td->sh->lock) != 0){
                perror("Pthread_cond_wait");
                exit(EXIT_FAILURE);
            }
        }

        strcpy(buffer, td->sh->buffer);
        
        if(pthread_mutex_unlock(&td->sh->lock) != 0){
            perror("pthread_mutex_unlock");
            exit(EXIT_FAILURE);
        }

        printf("[M] la riga è stata decifrata in %s\n", buffer);
        fprintf(out, "%s\n", buffer);
    }
    fclose(c);
    fclose(out);

    if(pthread_mutex_lock(&sh->lock) != 0){
        perror("pthread_mutex_lock");
        exit(EXIT_FAILURE);
    }

    sh->done = 1;

    for(int i = 0; i < nkeys; i++){
        if(pthread_cond_signal(&td->sh->read[i]) != 0){
            perror("pthread_cond_signal");
            exit(EXIT_FAILURE);
        }
    }

    if(pthread_mutex_unlock(&td->sh->lock) != 0){
        perror("pthread_mutex_unlock");
        exit(EXIT_FAILURE);
    }

    printf("[M] Termino i thread\n");
    for(int i = 0; i < nkeys; i++){
        if(pthread_join(td[i].tid, NULL) != 0){
            perror("pthread_join");
            exit(EXIT_FAILURE);
        }
    }

    shared_destroy(sh, nkeys);
    free(td);
}