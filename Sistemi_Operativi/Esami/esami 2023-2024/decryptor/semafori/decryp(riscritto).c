#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <semaphore.h>
#include <stdbool.h>
#include <ctype.h>
#include <sys/types.h>

#define KEY_SIZE 30     //considerando vari caratteri speciali e per evitare overflow
#define BUFFER_SIZE 100

typedef struct shared_data {
    char buffer[BUFFER_SIZE];
    sem_t buffer_sem;
    bool done;
} shared;

typedef struct {
    char key[KEY_SIZE];
    unsigned thread_n;
    pthread_t tid;
    shared* sh;
    sem_t sem_r, sem_w;
} thread_data;

shared* init_shared(){
    shared* sh = malloc(sizeof(shared));
    sh->done = false;
    if(sem_init(&sh->buffer_sem, 0, 1) != 0){
        perror("sem_init");
        exit(EXIT_FAILURE);
    }
    return sh;
}

void share_destroy(shared* sh){
    sem_destroy(&sh->buffer_sem);
    free(sh);
}

char* decrypt(char* text, char* keys){
    char alfabeto[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    int n = strlen(text);
    char* text_decifrato = malloc(sizeof(char) * (n + 1));
    for(int i = 0; i < n; i++){
        char* carattere_trovato = strchr(keys, text[i]);
        if(carattere_trovato != NULL){  
            int index = carattere_trovato - keys;
            text_decifrato[i] = alfabeto[index];
        } else {
            text_decifrato[i] = text[i];
        }
    }
    text_decifrato[n] = '\0';
    return text_decifrato;
}

int parse_text(char* text, char* testo_cifrato, int* key){
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
    // Si assume che l'indice nel file sia 1-indexed; decrementa per ottenere 0-indexed
    *key = atoi(index_text) - 1;
    free(index_text);
    return 0;
}

void thread_function(void* arg){
    thread_data* td = (thread_data*)arg;
    printf("[K%u] chiave assegnata: %s\n", td->thread_n + 1, td->key);
    char buffer[BUFFER_SIZE];

    while(!td->sh->done){
        if(sem_wait(&td->sem_r) != 0){
            perror("sem_wait");
            exit(EXIT_FAILURE);
        }
        if(td->sh->done){
            break;
        }
        if(sem_wait(&td->sh->buffer_sem) != 0){
            perror("sem_wait");
            exit(EXIT_FAILURE);
        }
        strcpy(buffer, td->sh->buffer);
        sem_post(&td->sh->buffer_sem);

        int d = strlen(buffer);
        printf("[K%u] sto decifrando la frase di %d caratteri passati dal main\n", td->thread_n + 1, d);

        char* dec = decrypt(buffer, td->key);
        if(sem_wait(&td->sh->buffer_sem) != 0){
            perror("sem_wait");
            exit(EXIT_FAILURE);
        }
        strncpy(td->sh->buffer, dec, BUFFER_SIZE-1);
        td->sh->buffer[BUFFER_SIZE-1] = '\0';
        sem_post(&td->sh->buffer_sem);
        free(dec);

        if(sem_post(&td->sem_w) != 0){
            perror("sem_post");
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
    char *keys_input_filename = argv[1];
    char *cifar_input_filename = argv[2];
    char *output_filename = argc > 3 ? argv[3] : "output.txt";
    shared* sh = init_shared();

    printf("[M] leggo il file delle chiavi\n");
    if((f = fopen(keys_input_filename, "r")) == NULL){
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    int n_keys = 0;
    while(!feof(f)){
        if(fgetc(f) == '\n'){
            n_keys++;
        }
    }
    fseek(f, 0, SEEK_SET);
    printf("[M] trovate %d chiavi, creo i thread k-i necessari\n", n_keys);

    // Allocazione di un array di thread_data
    thread_data* td = malloc(sizeof(thread_data) * n_keys);
    int j = 0;
    char key[KEY_SIZE];
    while(fgets(key, KEY_SIZE, f)){
        if(key[strlen(key) - 1] == '\n'){
            key[strlen(key) - 1] = '\0';
        }
        td[j].sh = sh;                 
        td[j].thread_n = j;            
        strcpy(td[j].key, key);        
        if(sem_init(&td[j].sem_r, 0, 0) != 0){
            perror("sem_init");
            exit(EXIT_FAILURE);
        }
        if(sem_init(&td[j].sem_w, 0, 0) != 0){
            perror("sem_init");
            exit(EXIT_FAILURE);
        }
        j++;
    }
    fclose(f);

    // Creazione dei thread
    for(int i = 0; i < n_keys; i++){
        if(pthread_create(&td[i].tid, NULL, (void*)thread_function, &td[i]) != 0){
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }
    
    printf("[M] Processo il file cifrato\n");
    FILE* c;
    if((c = fopen(cifar_input_filename, "r")) == NULL){
        perror("fopen");
        exit(EXIT_FAILURE);
    }
    char buffer[BUFFER_SIZE];
    char text[BUFFER_SIZE];
    int key_index;
    // Elaborazione di ogni riga cifrata
    while(fgets(buffer, BUFFER_SIZE, c)){
        if(parse_text(buffer, text, &key_index) != 0){
            perror("file_cifrato non valido");
            exit(EXIT_FAILURE);
        }
        if(key_index < 0 || key_index >= n_keys){
            fprintf(stderr, "[M] indici della chiave fuori range: %d\n", key_index + 1);
            exit(EXIT_FAILURE);
        }
        printf("[M] la riga '%s' deve essere decifrata con la chiave n. %d\n", text, key_index + 1);

        if(sem_wait(&sh->buffer_sem) != 0){
            perror("sem_wait");
            exit(EXIT_FAILURE);
        }
        strcpy(sh->buffer, text);
        if(sem_post(&sh->buffer_sem) != 0){
            perror("sem_post");
            exit(EXIT_FAILURE);
        }
        // Notifica al thread corrispondente
        if(sem_post(&td[key_index].sem_r) != 0){
            perror("sem_post");
            exit(EXIT_FAILURE);
        }
        // Attende che il thread completi la decifratura
        if(sem_wait(&td[key_index].sem_w) != 0){
            perror("sem_wait");
            exit(EXIT_FAILURE);
        }
        if(sem_wait(&sh->buffer_sem) != 0){
            perror("sem_wait");
            exit(EXIT_FAILURE);
        }
        strcpy(buffer, sh->buffer);
        if(sem_post(&sh->buffer_sem) != 0){
            perror("sem_post");
            exit(EXIT_FAILURE);
        }
        printf("[M] la riga è stata decifrata in %s\n", buffer);
    }
    fclose(c);
    printf("[M] Termino i thread\n");
    sh->done = true;
    for(int i = 0; i < n_keys; i++){
        if(sem_post(&td[i].sem_r) != 0){
            perror("sem_post");
            exit(EXIT_FAILURE);
        }
        if(pthread_join(td[i].tid, NULL) != 0){
            perror("pthread_join");
            exit(EXIT_FAILURE);
        }
        sem_destroy(&td[i].sem_r);
        sem_destroy(&td[i].sem_w);
    }
    share_destroy(sh);
    free(td);
}
