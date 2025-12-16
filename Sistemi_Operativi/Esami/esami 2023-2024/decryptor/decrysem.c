#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <semaphore.h>
#include <stdbool.h>
#include <ctype.h>
#include <sys/types.h>

#define KEY_SIZE 30
#define BUFFER_SIZE 100

typedef struct{
    char buffer[BUFFER_SIZE];
    sem_t buffer_sem;
    sem_t* read, *write;
    bool done;
}shared;

typedef struct{
    char key[KEY_SIZE];
    pthread_t tid;
    unsigned thread_n;
    shared* sh;
}thread_data;

shared* init_shared(int nkeys){
    shared* sh = malloc(sizeof(shared));

    sh->done = false;

    if(sem_init(&sh->buffer_sem, 0, 1) != 0){
        perror("sem_init");
        exit(EXIT_FAILURE);
    }

    sh->read = malloc(sizeof(sem_t) * nkeys);
    sh->write = malloc(sizeof(sem_t) * nkeys);

    for(int i = 0; i < nkeys; i++){
        if(sem_init(&sh->read[i], 0, 0) != 0){
            perror("sem_init");
            exit(EXIT_FAILURE);
        }

        if(sem_init(&sh->write[i], 0, 0) != 0){
            perror("sem_init");
            exit(EXIT_FAILURE);
        }
    }

    return sh;
}

void shared_destroy(shared* sh, int nkeys){
    sem_destroy(&sh->buffer_sem);
    for(int i = 0; i <nkeys; i++){
        sem_destroy(&sh->read[i]);
        sem_destroy(&sh->write[i]);
    }
    free(sh->read);
    free(sh->write);
    free(sh);
}

char* decrypt(char* text, char* key){
    char alfabeto[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    int n = strlen(text);
    char* text_dec = malloc(sizeof(char) * (n + 1));
    for(int i = 0; i < n; i++){
        char* carattere_trovato = strchr(key, text[i]);
        if(carattere_trovato != NULL){
            int index = carattere_trovato - key;
            text_dec[i] = alfabeto[index];
        }else{
            text_dec[i] = text[i];
        }
    }
    text_dec[n] = '\0';
    return text_dec;
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
        return - 1;
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
    *key = atoi(index_text);
    free(index_text);
    return 0;
}

void k_function(void* arg){
    thread_data* td = (thread_data*)arg;
    printf("[K%u] chiave assegnata: %s\n", td->thread_n + 1, td->key);
    char buffer[BUFFER_SIZE];

    while(!td->sh->done){
        if(sem_wait(&td->sh->read[td->thread_n]) != 0){
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

        int len = strlen(buffer);
        printf("[K%u] sto decifrando la frase di %d caratteri passati dal main\n", td->thread_n +1, len);
    
        char* decryp = decrypt(buffer, td->key);


        strncpy(td->sh->buffer, decryp, BUFFER_SIZE - 1);
        td->sh->buffer[BUFFER_SIZE - 1] = '\0';

        if(sem_post(&td->sh->buffer_sem) != 0){
            perror("sem_post");
            exit(EXIT_FAILURE); 
        }

        free(decryp);

        if(sem_post(&td->sh->write[td->thread_n]) != 0){
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

    char* input_key = argv[1];
    char* cifrato = argv[2];
    char* output = argc > 3 ? argv[3] : "output.txt";
    FILE* f;
    
    if((f = fopen(input_key, "r")) == NULL){
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    int nkeys = 0;
    char temp[KEY_SIZE];
    while(fgets(temp, KEY_SIZE, f)){
        if(temp[0] != '\n' && temp[0] != '\0'){
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
            key[strlen(key)- 1] = '\0';
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

        if(parse_text(buffer, text,  &key_index) != 0){
            perror("file cifrato non valido");
            exit(EXIT_FAILURE);
        }

        if(key_index < 0 || key_index >= nkeys){
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

        if(sem_post(&td->sh->read[key_index]) != 0){
            perror("sem_post");
            exit(EXIT_FAILURE);
        }

        if(sem_wait(&td->sh->write[key_index]) != 0){
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
        fprintf(out, "%s\n", buffer);
    }
    fclose(c);
    fclose(out);

    printf("[M] Termino i thread\n");
    sh->done = true;

    for(int i = 0; i <nkeys; i++){
        if(sem_post(&td->sh->read[i]) != 0){
            perror("sem_post");
            exit(EXIT_FAILURE);
        }
        
        if(pthread_join(td[i].tid, NULL) != 0){
            perror("pthread_join");
            exit(EXIT_FAILURE);
        }
    }

    shared_destroy(sh, nkeys);
    free(td);
}