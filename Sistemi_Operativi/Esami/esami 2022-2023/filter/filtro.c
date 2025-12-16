#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdbool.h>
#define BUFFER_SIZE 1024

typedef enum {TOUPPER_FILTER, TOLOWER_FILTER, REPLACE_FILTER} filter_type;

typedef struct{
    char buffer[BUFFER_SIZE];
    unsigned turn;
    unsigned nfilter;
    bool done;

    pthread_mutex_t lock;
    pthread_cond_t* cond;
}shared;

typedef struct{
    pthread_t tid;
    unsigned thread_n;
    filter_type type;
    char* filter_data1;
    char* filter_data2;

    shared* sh;
}thread_data;


shared* init_shared(unsigned nfilter){
    shared* sh = malloc(sizeof(shared));

    sh->turn = sh->done = 0;
    sh->nfilter = nfilter;

    if(pthread_mutex_init(&sh->lock, NULL) != 0){
        perror("pthread_mutex_init");
        exit(EXIT_FAILURE);
    }

    sh->cond = malloc(sizeof(pthread_cond_t) * (nfilter + 1));

    for(unsigned i = 0; i < nfilter + 1; i++){
        if(pthread_cond_init(&sh->cond[i], NULL) != 0){
            perror("pthread_cond_init");
            exit(EXIT_FAILURE);
        }
    } 

    return sh;
}

void destroy_shared(shared* sh){
    pthread_mutex_destroy(&sh->lock);
    for(unsigned i = 0; i < sh->nfilter + 1; i++){
        pthread_cond_destroy(&sh->cond[i]);
    }

    free(sh->cond);
    free(sh);
}

void toupper_filter(char* buffer, char* word){
    char* s;

    while((s = strstr(buffer, word) != NULL)){
        for(int i = 0; i< strlen(word); i++){
            s[i] = toupper(s[i]);
        }
    }
}

void tolower_filter(char* buffer, char* word){
    char* s;

    while((s = strstr(buffer, word) != NULL)){
        for(int i = 0; i< strlen(word); i++){
            s[i] = tolower(s[i]);
        }
    }
}

void replace_filter(char* buffer, char* word1, char* word2){
    char* s;
    int i;
    char temp_buffer[BUFFER_SIZE];

    while((s = strstr(buffer, word1)) != NULL){
        strncpy(temp_buffer, buffer, BUFFER_SIZE);

        i = s - buffer;

        buffer[i] = '\0';

        sprintf(buffer, "%s%s%s", buffer, word2, temp_buffer + i + strlen(word1));
    }
}

void filter(void* arg){
    thread_data* td = (thread_data*)arg;

    while(1){
        if(pthread_mutex_lock(&td->sh->lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }

        while(td->sh->turn != td->thread_n){
            if(pthread_cond_wait(&td->sh->cond[td->thread_n], &td->sh->lock) != 0){
                perror("pthread_cond_wait");
                exit(EXIT_FAILURE);
            } 
        }
    
        td->sh->turn = (td->sh->turn + 1) % (td->sh->nfilter + 1);

        if(td->sh->done){
            if(pthread_cond_signal(&td->sh->cond[td->sh->turn]) != 0){
                perror("pthread_cond_signal");
                exit(EXIT_FAILURE);
            }

            if(pthread_mutex_unlock(&td->sh->lock) != 0){
                perror("pthread_mutex_unlock");
                exit(EXIT_FAILURE);
            }
            break;
        }

        switch(td->type){
            case TOUPPER_FILTER:
            toupper_filter(td->sh->buffer, td->filter_data1);
            break;
            case TOLOWER_FILTER:
            tolower_filter(td->sh->buffer, td->filter_data1);
            break;
            case REPLACE_FILTER:
            replace_filter(td->sh->buffer, td->filter_data1, td->filter_data2);
            break;
            default:
            fprintf(stderr, "Tipo di filtro non valido: %d", td->type);
            exit(EXIT_FAILURE);
            break;
        }

        if(pthread_cond_signal(&td->sh->cond[td->sh->turn]) != 0){
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
    if(argc < 3){
        fprintf(stderr, "Usage: %s <file.txt> <filter-1> [filter-2] [...]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    FILE* f;
    thread_data td[argc - 2];
    shared* sh = init_shared(argc - 2);
    char* word1, *word2;
    bool first_turn = 1;
    char buffer[BUFFER_SIZE];

    for(int i = 2; i< argc; i++){
        td[i -2].thread_n = i - 1;
        td[i -2].sh = sh;

        switch(argv[i][0]){
            case '^':
            td[i - 2].type = TOUPPER_FILTER;
            td[i - 2].filter_data1 = &argv[i][1];
            break;
            case '_':
            td[i - 2].type = TOLOWER_FILTER;
            td[i - 2].filter_data1 = &argv[i][1];
            break;
            case '%':
            td[i - 2].type = REPLACE_FILTER;

            if(!(word1 = strtok(&argv[i][1], ",")) || !(word2 = strtok(NULL, ","))){
                fprintf(stderr, "Errore nel filtreo %s: sintassi non supportata\n", argv[i]);
                exit(EXIT_FAILURE);
            }

            td[i - 2].filter_data1 = word1;
            td[i - 2].filter_data2 = word2;
            break;
            default:
            fprintf(stderr, "Errore nel filtro %s: sintassi non supportata\n", argv[i]);
            exit(EXIT_FAILURE);
        }

        if(pthread_create(&td[i -2].tid, NULL, (void*)filter, &td[i-2]) != 0){
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }

    if(f = fopen(argv[1], "r") == NULL){
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    while(fgets(buffer, BUFFER_SIZE, f)){
        if(pthread_mutex_lock(&sh->lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }

        while(sh->turn != 0){
            if(pthread_cond_wait(&sh->cond[0], &sh->lock) != 0){
                perror("pthread_cond_wait");
                exit(EXIT_FAILURE);
            }
        }

        if(!first_turn){
            printf("%s", sh->buffer);
        }else{
            first_turn = 0;
        }

        strncpy(sh->buffer, buffer, BUFFER_SIZE);
        sh->turn++;

        if(pthread_cond_signal(&sh->cond[sh->turn]) != 0){
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

    while(sh->turn != 0){
        if(pthread_cond_wait(&sh->cond[0], &sh->lock) != 0){
            perror("pthread_cond_wait");
            exit(EXIT_FAILURE);
        }
    }

    printf("%s\n", sh->buffer);

    sh->done = 1;
    sh->turn++;

    if(pthread_cond_signal(&sh->cond[sh->turn]) != 0){
        perror("pthread_cond_signal");
        exit(EXIT_FAILURE);
    }

    if(pthread_mutex_unlock(&sh->lock) != 0){
        perror("pthread_mutex_unlock");
        exit(EXIT_FAILURE);
    }

    for(int i = 0; i< argc -2 ; i++){
        if(pthread_join(td[i].tid, NULL) != 0){
            perror("pthread_join");
            exit(EXIT_FAILURE);
        }
    }

    fclose(f);
    destroy_shared(sh);
}