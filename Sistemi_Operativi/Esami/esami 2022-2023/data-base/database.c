#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdbool.h>
#include <unistd.h>
#define KEY_SIZE 4096
#define BUFFER_SIZE 4096
#define SLEEP_S 8

typedef struct _node{
    char key[KEY_SIZE];
    int value;
    struct _node* next;
}node;

typedef struct{
    node* head;
    pthread_rwlock_t lock;
}list;

void init_list(list* l){
    l->head = NULL;

    if(pthread_rwlock_init(&l->lock, NULL) != 0){
        perror("ptrehad_rwlock_init");
        exit(EXIT_FAILURE);
    }
}

void list_insert(list* l, const char* key, const int value){
    node* n = malloc(sizeof(node));
    strncpy(n->key, key, KEY_SIZE);
    n->value = value;

    if(pthread_rwlock_wrlock(&l->lock) != 0){
        perror("pthread_rwlock_wrlock");
        exit(EXIT_FAILURE);
    }

    n->next = l->head;
    l->head = n;

    if(pthread_rwlock_unlock(&l->lock) != 0){
        perror("pthread_rwlock_unlock");
        exit(EXIT_FAILURE);
    }
}

void print_list(list* l){
    if(pthread_rwlock_rdlock(&l->lock) != 0){
        perror("pthread_rwlock_rdlock");
        exit(EXIT_FAILURE);
    }

    node* ptr = l->head;

    while(ptr != NULL){
        printf("(%s, %d)", ptr->key, ptr->value);
        ptr = ptr->next;
    }

    if(pthread_rwlock_unlock(&l->lock) != 0){
        perror("Pthread_rwlock_unlock");
        exit(EXIT_FAILURE);
    }

    printf("\n");
}

bool list_search(list* l, const char* key, int* value){
    bool found_value = 0;

    if(pthread_rwlock_rdlock(&l->lock) != 0){
        perror("pthread_mutex_rdlock");
        exit(EXIT_FAILURE);
    }

    node* ptr = l->head;

    while(ptr != NULL && (strcmp(ptr->key, key)) != 0){
        ptr = ptr->next;
    }

    if(ptr != NULL){
        found_value = 1;
        *value = ptr->value;
    }

    if(pthread_rwlock_unlock(&l->lock) != 0){
        perror("pthread_rwlock_unlock");
        exit(EXIT_FAILURE);
    }

    return found_value;
}

unsigned list_count(list* l){
    unsigned counter = 0;

    if(pthread_rwlock_rdlock(&l->lock) != 0){
        perror("pthraed_rwlock_rdlock");
        exit(EXIT_FAILURE);
    }

    node* ptr = l->head;

    while(ptr != NULL){ 
        counter++;
        ptr = ptr->next;
    }

    if(pthread_rwlock_unlock(&l->lock) != 0){
        perror("pthread_rwlock_unlock");
        exit(EXIT_FAILURE);
    }

    return counter;
}

void destroy_list(list* l){
    if(pthread_rwlock_wrlock(&l->lock) != 0){
        perror("pthread_rwlock_wrlock");
        exit(EXIT_FAILURE);
    }

    node* ptr = l->head;
    node* temp;

    while(ptr != NULL){
        temp = ptr;
        ptr = ptr->next;
        free(temp);
    }

    pthread_rwlock_destroy(&l->lock);
    free(l);
}

typedef struct{
    list* l;
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

    sh->l = malloc(sizeof(list));

    if(pthread_mutex_init(&sh->lock, NULL) != 0){
        perror("pthread_mutex_init");
        exit(EXIT_FAILURE);
    }

    sh->done = 0;
    init_list(sh->l);

    return sh;
}

void destroy_shared(shared* sh){
    destroy_list(sh->l);
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

    while((fgets(buffer, BUFFER_SIZE, f))){
        if((key = strtok(buffer, ":")) != NULL && (s_value = strtok(NULL, ":")) != NULL){
            value = atoi(s_value);

            if(pthread_mutex_lock(&td->sh->lock) != 0){
                perror("pthread_mutex_lock");
                exit(EXIT_FAILURE);
            }

            if(td->sh->done){
                printf("R%d esco \n", td->thread_n);
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

            list_insert(td->sh->l, key, value);

            printf("R%d: inserimento in lista dell'elemento (%s, %d)\n", td->thread_n, key, value);

            sleep(SLEEP_S);
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
        if((fgets(query, BUFFER_SIZE, stdin))){
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

                printf("Q: Chiusura dei thread ...\n");
                break;
            }else{
                ret_value = list_search(td->sh->l, query, &result);

                if(ret_value){
                    printf("Q: occorrenza trovata (%s, %d)\n", query, result);
                }else{
                    printf("Q: non è stata trovata nessuna ooccorrenza con chiave: %s\n", query);
                }
            }
        }
    }
}

void counter(void* arg){
    thread_data* td = (thread_data*)arg;
    int counter = 0;
    
    while(1){
        if(pthread_mutex_lock(&td->sh->lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }

        if(td->sh->done){
            if(pthread_mutex_unlock(&td->sh->lock) != 0){
                perror("pthread_mutex_lock");
                exit(EXIT_FAILURE);
            }

            printf("C: esco.\n");
            break;
        }

        if(pthread_mutex_unlock(&td->sh->lock) != 0){
            perror("pthread_mutex_unlock");
            exit(EXIT_FAILURE);
        }

        counter = list_count(td->sh->l);

        printf("C: sono presenti %d elementi all'interno della lista\n", counter);

        sleep(SLEEP_S);
    }
}

int main(int argc, char** argv){
    if(argc < 2){
        fprintf(stderr, "Usage: %s <db-file-1> <db-file-2> <...> <db-file-n>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    thread_data td[argc + 1];
    shared* sh = init_shared();

    for(int i = 0; i < argc - 1; i++){
        td[i].sh = sh;
        td[i].thread_n = i + 1;
        td[i].filename = argv[i + 1];

        if(pthread_create(&td[i].tid, NULL, (void*)reader, &td[i]) != 0){
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }

    td[argc - 1].sh = sh;
    if(pthread_create(&td[argc -1].tid, NULL, (void*)query, &td[argc -1]) != 0){
        perror("pthread_Create");
        exit(EXIT_FAILURE);
    }

    td[argc].sh = sh;
    if(pthread_create(&td[argc].tid, NULL, (void*)counter, &td[argc]) != 0){
        perror("pthread_Create");
        exit(EXIT_FAILURE);
    }

    for(int i = 0; i < argc + 1; i++){
        if(pthread_join(td[i].tid, NULL) != 0){
            perror("pthread_join");
            exit(EXIT_FAILURE);
        }
    }
    
    destroy_shared(sh);
}