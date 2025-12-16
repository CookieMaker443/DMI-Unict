#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#define KEY_SIZE 4096
#define HASH_FUNCTION_MULTI 97
#define BUFFER_SIZE 4096

typedef struct _item{
    char key[KEY_SIZE];
    int value;
    struct _item* next;
}item;

typedef struct{
    unsigned long size;
    unsigned long n;
    item** table;

    pthread_rwlock_t lock;
}hash_table;

hash_table* init_hash_table(unsigned long size){
    hash_table* h = malloc(sizeof(hash_table));

    h->n = 0;
    h->size = size;
    h->table = calloc(size, sizeof(item*));

    if(pthread_rwlock_init(&h->lock, NULL) != 0){
        perror("pthread_rwlock_init");
        exit(EXIT_FAILURE);
    }

    return h;
}

unsigned long hash_function(const char* key){
    unsigned const char* us;
    unsigned long h = 0;

    for(us = (const char* )key; *us; us++){
        h = h * HASH_FUNCTION_MULTI + *us;
    }

    return h;
}

void hash_table_insert(hash_table* h, const char* key, const int value){
    item* i = malloc(sizeof(item));
    strncpy(i->key, key, KEY_SIZE);
    i->value = value;
    unsigned long hindex = hash_function(key) % h->size;

    if(pthread_rwlock_wrlock(&h->lock) != 0){
        perror("pthread_rwlock_wrlock");
        exit(EXIT_FAILURE);
    }

    i->next = h->table[hindex];
    h->table[hindex] = i;
    h->n++;

    if(pthread_rwlock_unlock(&h->lock) != 0){
        perror("ptrhead_rwlock_unlock");
        exit(EXIT_FAILURE);
    }
}

bool hash_table_search(hash_table* h, const char* key, int* value){
    item* ptr;
    bool found_value = 0;
    unsigned long hindex = hash_function(key) % h->size;

    if(pthread_rwlock_rdlock(&h->lock) != 0){
        perror("pthread_rwlock_rdlock");
        exit(EXIT_FAILURE);
    }

    ptr = h->table[hindex];

    while(ptr != NULL && strcmp(key, ptr->key)){
        ptr = ptr->next;
    }

    if(ptr != NULL){
        found_value = 1;
        *value = ptr->value;
    }

    if(pthread_rwlock_unlock(&h->lock) != 0){
        perror("pthread_rwlock_unlock");
        exit(EXIT_FAILURE);
    }

    return found_value;
}

unsigned long get_n(hash_table* h){
    unsigned long n;

    if(pthread_rwlock_rdlock(&h->lock) != 0){
        perror("Ptrehad_rwlock_rdlock");
        exit(EXIT_FAILURE);
    }

    n = h->n;

    if(pthread_rwlock_unlock(&h->lock) != 0){
        perror("pthread_rwlock_unlock");
        exit(EXIT_FAILURE);
    }

    return n;
}

void item_destroy(item* i){
    item* ptr = i;
    item* temp;

    while(ptr != NULL){
        temp = ptr;
        ptr = ptr->next;
        free(temp);
    }
}

void hash_table_destroy(hash_table* h){
    if(pthread_rwlock_wrlock(&h->lock) != 0){
        perror("ptrhead_rwlock_wrlock");
        exit(EXIT_FAILURE);
    }

    for(unsigned i = 0; i < h->size; i++){
        item_destroy(h->table[i]);
    }

    free(h->table);
    pthread_rwlock_destroy(&h->lock);
    free(h);
}

typedef struct{
    hash_table* h;
    bool done;
    pthread_mutex_t lock;
}shared;

typedef struct{
    pthread_t tid;
    unsigned thread_n;
    char* filename;

    shared* sh;
}thread_data;

shared* init_shared(){
    shared* sh = malloc(sizeof(shared));

    if(pthread_mutex_init(&sh->lock, NULL) != 0){
        perror("pthread_mutex_init");
        exit(EXIT_FAILURE);
    }

    sh->done = 0;
    sh->h = init_hash_table(1024);

    return sh;
}

void destroy_shared(shared* sh){
    hash_table_destroy(sh->h);
    free(sh);
}

void reader(void* arg){
    thread_data* td = (thread_data*)arg;
    FILE* f;
    char buffer[BUFFER_SIZE];
    char* key, *s_value;
    int value;

    if((f = fopen(td->filename, "r")) == NULL){
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    while(fgets(buffer, BUFFER_SIZE, f)){
        if((key = strtok(buffer, ":")) != NULL && (s_value = strtok(NULL, ":")) != NULL){
            value = atoi(s_value);

            if(pthread_mutex_lock(&td->sh->lock) != 0){
                perror("pthread_mutex_lock");
                exit(EXIT_FAILURE);
            }

            if(td->sh->done){
                printf("R%d: esco\n", td->thread_n);
                
                if(pthread_mutex_unlock(&td->sh->lock) != 0){
                    perror("pthread_mutex_unlock");
                    exit(EXIT_FAILURE);
                }
                break;
            }
            
            if(pthread_mutex_unlock(&td->sh->lock) != 0){
                perror("pthread_mutex_unlock");
                exit(EXIT_FAILURE);
            }

            hash_table_insert(td->sh->h, key, value);

            printf("R%d: ho inserito l'elemento (%s, %d)\n", td->thread_n, key, value);
            sleep(8);
        }
    }
    fclose(f);
}

void query(void* arg){
    thread_data* td = (thread_data*)arg;
    char query[BUFFER_SIZE];
    int result;
    bool ret_value;

    while(1){
        if(fgets(query, BUFFER_SIZE, stdin)){
            if(query[strlen(query) - 1] == '\n'){
                query[strlen(query) - 1] = '\0';
            }

            if(!strcmp(query, "quit")){
                if(pthread_mutex_lock(&td->sh->lock) != 0){
                    perror("pthread_mutex_lock");
                    exit(EXIT_FAILURE);
                }

                td->sh->done = 1;

                if(pthread_mutex_unlock(&td->sh->lock) != 0){
                    perror("pthread_mutex_unlock");
                    exit(EXIT_FAILURE);
                }

                printf("Q: chiusura dei thread \n");
                break;
            }else{
                ret_value = hash_table_search(td->sh->h, query, &result);

                if(ret_value){
                    printf("Q: Occorrenza trovata (%s, %d)\n", query, result);
                }else{
                    printf("Q_ non è stata trovata alunca occorrenza con la parola %s\n", query);
                }
            }
        }
    }
}

void counter(void* arg){
    thread_data* td = (thread_data*)arg;
    int counter;

    while(1){
        if(pthread_mutex_lock(&td->sh->lock) != 0){
            perror("Pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }

        if(td->sh->done){
            if(pthread_mutex_unlock(&td->sh->lock) != 0){
                perror("pthread_mutex_unlock");
                exit(EXIT_FAILURE);
            }

            printf("C: esco\n");
            break;
        }
        if(pthread_mutex_unlock(&td->sh->lock) != 0){
            perror("pthread_mutex_unlock");
            exit(EXIT_FAILURE);
        }

        counter = get_n(td->sh->h);

        printf("C: sono presenti %d elementi all'interno della lista\n", counter);
        sleep(8);
    }
}

int main(int argc, char** argv){
    if(argc < 2){
        fprintf(stderr, "Usage: %s <data-base-1> <data-base-2> ... <data-base-n>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    thread_data td[argc + 1];
    shared* sh = init_shared();

    for(int i = 0; i < argc - 1; i++){
        td[i].sh = sh;
        td[i].filename = argv[i + 1];
        td[i].thread_n = i + 1;

        if(pthread_create(&td[i].tid, NULL, (void*)reader, &td[i]) != 0){
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }

    td[argc - 1].sh = sh;

    if(pthread_create(&td[argc -1].tid, NULL, (void*)query, &td[argc - 1]) != 0){
        perror("pthread_Create");
        exit(EXIT_FAILURE);
    }

    td[argc].sh = sh;
    if(pthread_create(&td[argc].tid, NULL, (void*)counter, &td[argc]) != 0){
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    for(int i = 0; i < argc +1; i++){
        if(pthread_join(td[i].tid, NULL) != 0){
            perror("pthread_join");
            exit(EXIT_FAILURE);
        }
    }

    destroy_shared(sh);
}