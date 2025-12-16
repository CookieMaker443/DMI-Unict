#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>
#define BUFFER_SIZE 4096

typedef enum{R, P, W}thread_n;

typedef struct{
    char buffer[BUFFER_SIZE];
    bool done;

    sem_t sem[3];
}shared;

typedef struct{
    pthread_t tid;
    char* filename;

    shared* sh;
}thread_data;

shared* init_shared(){
    shared* sh = malloc(sizeof(shared));

    sh->done  = 0;

    if(sem_init(&sh->sem[R], 0, 1) != 0){
        perror("sem_init");
        exit(EXIT_FAILURE);
    }

    if(sem_init(&sh->sem[P], 0, 0) != 0){
        perror("sem_init");
        exit(EXIT_FAILURE);
    }

    if(sem_init(&sh->sem[W], 0, 0) != 0){
        perror("sem_init");
        exit(EXIT_FAILURE);
    }

    return sh;
}

void destroy_shared(shared* sh){
    for(int i = 0; i < 3; i++){
        sem_destroy(&sh->sem[i]);
    }
    free(sh);
}

void reader(void* arg){
    thread_data* td = (thread_data*)arg;
    char buffer[BUFFER_SIZE];
    FILE* f;

    if(f = fopen(td->filename, "r") == NULL){
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    while(fgets(buffer, BUFFER_SIZE, f)){
        if(buffer[strlen(buffer) - 1] == '\n'){
            buffer[strlen(buffer) - 1] = '\0';
        }

        if(sem_wait(&td->sh->sem[R]) != 0){
            perror("sem_wait");
            exit(EXIT_FAILURE);
        }

        strncpy(td->sh->buffer, buffer, BUFFER_SIZE);

        if(sem_post(&td->sh->sem[P]) != 0){
            perror("sem_post");
            exit(EXIT_FAILURE);
        }
    }

    if(sem_wait(&td->sh->sem[R]) != 0){
        perror("sem_Wait");
        exit(EXIT_FAILURE);
    }

    td->sh->done = 1;

    if(sem_post(&td->sh->sem[P]) != 0){
        perror("sem_post");
        exit(EXIT_FAILURE);
    }
    
    if(sem_post(&td->sh->sem[W]) != 0){
        perror("sem_post");
        exit(EXIT_FAILURE);
    }
}

bool check_palindroma(char* s){
    for(int i = 0; i < strlen(s); i++){
        if(s[i] != s[strlen(s) - 1 - i]){
            return 0;
        }
    }
    return 1;
}

void palindroma(void* arg){
    thread_data* td = (thread_data*)arg;

    while(1){
        if(sem_wait(&td->sh->sem[P]) != 0){
            perror("sem_wait");
            exit(EXIT_FAILURE);
        }

        if(td->sh->done){
            break;
        }

        if(check_palindroma(td->sh->buffer)){
            if(sem_post(&td->sh->sem[W]) != 0){
                perror("sem_post");
                exit(EXIT_FAILURE);
            }
        }else{
            if(sem_post(&td->sh->sem[R]) != 0){
                perror("sem_post");
                exit(EXIT_FAILURE);
            }
        }
    }
}

void writer(void* arg){
    thread_data* td = (thread_data*)arg;

    while(1){
        if(sem_wait(&td->sh->sem[W]) != 0){
            perror("sem_wait");
            exit(EXIT_FAILURE);
        }

        if(td->sh->done){
            break;
        }

        printf("%s\n", td->sh->buffer);

        if(sem_post(&td->sh->sem[R]) != 0){
            perror("sem_post");
            exit(EXIT_FAILURE);
        }
    }
}

int main(int argc, char** argv){
    if(argc != 2){
        fprintf(stderr, "Usage: %s <input-file>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    thread_data td[3];
    shared* sh = init_shared();
    
    for(int i = 0; i <3 ; i++){
        td[i].sh = sh;
    }

    td[R].filename = argv[1];

    if(pthread_create(&td[R].tid, NULL, (void*)reader, &td[R]) != 0){
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    if(pthread_create(&td[P].tid, NULL, (void*)palindroma, &td[P]) != 0){
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    if(pthread_create(&td[W].tid, NULL, (void*)writer, &td[W]) != 0){
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    for(int i = 0; i < 3; i++){
        if(pthread_join(td[i].tid, NULL) != 0){
            perror("pthread_join");
            exit(EXIT_FAILURE);
        }
    }

    destroy_shared(sh);
}