#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>

#define KEY_SIZE 30
#define BUFFER_SIZE 100

typedef struct shared_data{
    char buffer[BUFFER_SIZE];
    sem_t sem_b;
    bool done;
}shared;

typedef struct{
    char key[KEY_SIZE];
    pthread_t tid;
    unsigned thread_n;
    shared* sh;
    sem_t read, write;
}thread_k;

shared* init_shared(){
    shared* sh = malloc(sizeof(shared));
    
    sh->done = false;

    if(sem_init(&sh->sem_b, 0, 1) != 0){
        perror("Sem_init");
        exit(EXIT_FAILURE);
    }

    return sh;
}

void shared_destroy(shared* sh){
    sem_destroy(&sh->sem_b);
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
        } else {
            text_dec[i] = text[i];
        }
    }
    text_dec[n] = '\0';
    return text_dec;
}

void k_function(void* arg){
    thread_k* td = (thread_k*)arg;
    printf("[K%u] chiave assegnata: %s\n", td->thread_n + 1, td->key);
    char buffer[BUFFER_SIZE];

    while(1){
        if(sem_wait(&td->read) != 0){
            perror("sem_wait");
            exit(EXIT_FAILURE);
        }
    
        if(td->sh->done){
            break;
        }

        if(sem_wait(&td->sh->sem_b) != 0){
            perror("sem_wait");
            exit(EXIT_FAILURE);
        }

        strcpy(buffer, td->sh->buffer);

        int len = strlen(buffer);
        printf("[K%u] sto decifrando la frase di %d caratteri passata dal main\n", td->thread_n +1, len);

        char* decryp = decrypt(buffer, td->key);

        strncpy(td->sh->buffer, decryp, BUFFER_SIZE - 1);
        td->sh->buffer[BUFFER_SIZE - 1] = '\0';
        
        if(sem_post(&td->sh->sem_b) != 0){
            perror("sem_post");
            exit(EXIT_FAILURE);
        }
        free(decryp);

        if(sem_post(&td->write) != 0){
            perror("sem_post");
            exit(EXIT_FAILURE);
        }
    }
}

int main(int argc, char** argv){
    if(argc < 3){
        fprintf(stderr, "Usage: %s <keys-file> <ciphertext-input-file> [plaintext-output-file]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char* keysfile = argv[1];
    char* ciphertext = argv[2];
    char* output_file = argc > 3 ? argv[3] : "output.txt";

    printf("[M] leggo il file delle chiavi\n");
    FILE* f_key;
    if((f_key = fopen(keysfile, "r")) == NULL){
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    int nkeys = 0;
    char temp[KEY_SIZE];
    while(fgets(temp, KEY_SIZE, f_key)){
        if(temp[strlen(temp) - 1] == '\n'){
            temp[strlen(temp) - 1] = '\0';
        }
        nkeys++;
    }
    rewind(f_key);
    printf("[M] trovate %d chiavi, creo i thread k-i necessari\n", nkeys);

    shared* sh = init_shared();
    thread_k* td = malloc(sizeof(thread_k) * nkeys);
    int i = 0;
    char key[KEY_SIZE];

    while(fgets(key, KEY_SIZE, f_key)){
        if(key[strlen(key) - 1] == '\n'){
            key[strlen(key) - 1] = '\0';
        }

        td[i].sh = sh;
        td[i].thread_n = i;
        strcpy(td[i].key, key);
        if(sem_init(&td[i].read, 0, 0) != 0){
            perror("sem_init");
            exit(EXIT_FAILURE);
        }
        if(sem_init(&td[i].write, 0, 0) != 0){
            perror("sem_init");
            exit(EXIT_FAILURE);
        }

        i++;
    }
    fclose(f_key);

    for(int i = 0; i <nkeys; i++){
        if(pthread_create(&td[i].tid, NULL, (void*)k_function, &td[i]) != 0){
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }

    FILE* out;

    if((out = fopen(output_file, "w")) == NULL){
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    printf("[M] Processo il file cifrato\n");
    FILE* c;

    if((c = fopen(ciphertext, "r")) == NULL){
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    char buffer[BUFFER_SIZE];
    char* text, *s_index;
    int index;


    while(fgets(buffer, BUFFER_SIZE, c)){
        if(buffer[strlen(buffer) - 1] == '\n'){
            buffer[strlen(buffer) - 1] = '\0';
        }

        if((s_index = strtok(buffer, ":")) != NULL && (text = strtok(NULL, ":")) != NULL){
            index = atoi(s_index);

            if(index < 0 || index >= nkeys){
                fprintf(stderr, "[M] indici della chiave fuori range: %d\n", index + 1);
                exit(EXIT_FAILURE);
            }

            printf("[M] la riga '%s' deve essere decifrata con la chiave n. %d\n", text, index + 1);

            if(sem_wait(&td->sh->sem_b) != 0){
                perror("sem_Wait");
                exit(EXIT_FAILURE);
            }

            strcpy(td->sh->buffer, text);

            if(sem_post(&td->sh->sem_b) != 0){
                perror("sem_post");
                exit(EXIT_FAILURE);
            }

            if(sem_post(&td[index].read) != 0){
                perror("sem_post");
                exit(EXIT_FAILURE);
            }

            if(sem_wait(&td[index].write) != 0){
                perror("sem_wait");
                exit(EXIT_FAILURE);
            }

            if(sem_wait(&td->sh->sem_b) != 0){
                perror("sem_wait");
                exit(EXIT_FAILURE);
            }

            strcpy(buffer, td->sh->buffer);

            if(sem_post(&td->sh->sem_b)  != 0){
                perror("sem_post");
                exit(EXIT_FAILURE);
            }
            printf("[M] la riga è stata decifrata in %s\n", buffer);
            fprintf(out, "%s\n", buffer);
        }
    }
    fclose(c);
    fclose(out);

    printf("[M] termino i thread\n");
    sh->done = true;
    printf("[M] Aspetto che tutti i thread terminano e dealloco tutte le risorse\n");
    for (int i = 0; i < nkeys; i++){
        if(sem_post(&td[i].read) != 0){
            perror("sem_post");
            exit(EXIT_FAILURE);
        }

        if(pthread_join(td[i].tid, NULL) != 0){
            perror("pthread_join");
            exit(EXIT_FAILURE);
        }

        sem_destroy(&td[i].read);
        sem_destroy(&td[i].write);
    }
    free(td);
    shared_destroy(sh);
}