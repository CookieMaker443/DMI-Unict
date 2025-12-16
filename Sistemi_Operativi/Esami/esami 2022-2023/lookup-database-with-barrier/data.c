#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>
#include <linux/limits.h>
#define HASH_FUNCTION_MULTI 97
#define KEY_SIZE 2048
#define BUFFER_SIZE 4096

typedef struct _item{
    char key[KEY_SIZE];
    int value;
    struct _item* next;
}item;

typedef struct{
    unsigned long n;
    unsigned long size;
    item** table;

    pthread_mutex_t lock;
}hash_table;

hash_table* init_hash_tabele(unsigned long size){
    hash_table* h = malloc(sizeof(hash_table));
    h->size = size;
    h->n = 0;
    h->table = calloc(size, sizeof(item*));

    if(pthread_mutex_init(&h->lock, NULL) != 0){
        perror("pthread_mutex_init");
        exit(EXIT_FAILURE);
    }
    
    return h;
}

void hash_table_lock(hash_table* h){
    if(pthread_mutex_lock(&h->lock) != 0){
        perror("pthread_mutex_lock");
        exit(EXIT_FAILURE);
    }
}

void hash_table_unlock(hash_table* h){
    if(pthread_mutex_unlock(&h->lock) != 0){
        perror("pthread_mutex_unlock");
        exit(EXIT_FAILURE);
    }
}

unsigned long hash_function(const char* key){
    unsigned const char* s;
    unsigned long h = 0;

    for(s = (unsigned const char*)key; *s; s++){
        h = h * HASH_FUNCTION_MULTI + *s;
    }

    return h;
}

void hash_table_insert(hash_table* h, const char* key, const int value){
    item* i = malloc(sizeof(item));
    strncpy(i->key, key, KEY_SIZE);
    i->value = value;
    unsigned long hindex = hash_function(key) % h->size;

    hash_table_lock(h);

    i->next = h->table[hindex];
    h->table[hindex] = i;
    h->n++;

    hash_table_unlock(h);
}

bool hash_table_search(hash_table* h, const char* key, int* value){
    bool found_value = 0;
    item* ptr;
    unsigned long hindex = hash_function(key) % h->size;

    ptr = h->table[hindex];

    while(ptr != NULL && strcmp(key, ptr->key)){
        ptr = ptr->next;
    }

    if(ptr != NULL){
        found_value = 1;
        *value = ptr->value;
    }

    return found_value;
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
    hash_table_lock(h);

    for(unsigned i = 0; i < h->size; i++){
        item_destroy(h->table[i]);
    }

    free(h->table);
    pthread_mutex_destroy(&h->lock);
    free(h);
}

typedef struct{
    hash_table* h;
    pthread_barrier_t barrier;
    unsigned nthread;
}shared_db;

shared_db* init_shared_db(unsigned nthread){
    shared_db* sdb = malloc(sizeof(shared_db));

    sdb->h = init_hash_tabele(1024);
    sdb->nthread = nthread;

    if(pthread_barrier_init(&sdb->barrier, NULL, nthread) != 0){
        perror("pthread_barrier_init");
        exit(EXIT_FAILURE);
    }

    return sdb;
}

void destroy_shared_db(shared_db* sdb){
    hash_table_destroy(sdb->h);
    pthread_barrier_destroy(&sdb->barrier);
    free(sdb);
}

typedef struct{
    unsigned thread_n;
    char nome[KEY_SIZE];
    int value;
    bool done;

    sem_t sem_r, sem_w;
}shared_c;

shared_c* init_shared_c(){
    shared_c* sc = malloc(sizeof(shared_c));

    sc->done = 0;

    if(sem_init(&sc->sem_w, 0 , 1) != 0){
        perror("sem_init");
        exit(EXIT_FAILURE);
    }

    if(sem_init(&sc->sem_r, 0, 0) != 0){
        perror("sem_init");
        exit(EXIT_FAILURE);
    }

    return sc;
}

void destroy_shared_c(shared_c* sc){
    sem_destroy(&sc->sem_w);
    sem_destroy(&sc->sem_r);
    free(sc);
}

typedef struct{
    pthread_t tid;
    unsigned thread_n;
    char* db_file;
    char* query_file;

    shared_db* sdb;
    shared_c* sc;
}thread_data;

void db_child(void* arg){
    thread_data* td = (thread_data*)arg;
    FILE* f;
    char buffer[BUFFER_SIZE];
    char* key;
    char* s_value;
    int value;

    if((f = fopen(td->db_file, "r")) == NULL){
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    while(fgets(buffer, BUFFER_SIZE, f)){
        if((key = strtok(buffer, ":")) != NULL && (s_value = strtok(NULL, ":")) != NULL){
            value = atoi(s_value);
            
            printf("DB-IN-%u: inserisco (%s, %d)\n", td->thread_n, key, value);
            hash_table_insert(td->sdb->h, key, value);
        }
    }

    fclose(f);

    printf("DB-IN-%u: sono arrivato alla barriera\n", td->thread_n);
    
    if(pthread_barrier_wait(&td->sdb->barrier) > 0){
        perror("pthread_barrier_wait");
        exit(EXIT_FAILURE);
    }

    if((f = fopen(td->query_file, "r")) == NULL){
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    while(fgets(buffer, BUFFER_SIZE, f)){
        if(buffer[strlen(buffer) - 1] == '\n'){
            buffer[strlen(buffer) - 1] = '\0';
        }

        if(hash_table_search(td->sdb->h, key, &value)){
            printf("DB-IN-%u: elemento trovato (%s, %d)\n", td->thread_n, buffer, value);

            if(sem_wait(&td->sc->sem_w) != 0){
                perror("sem_wait");
                exit(EXIT_FAILURE);
            }

            td->sc->thread_n = td->thread_n;
            strncpy(td->sc->nome, buffer, KEY_SIZE);
            td->sc->value = value;

            if(sem_post(&td->sc->sem_r) != 0){
                perror("sem_post");
                exit(EXIT_FAILURE);
            }
        }
    }

    if(sem_wait(&td->sc->sem_w) != 0){
        perror("sem_wait");
        exit(EXIT_FAILURE);
    }

    td->sc->done = 1;

    if(sem_post(&td->sc->sem_r) != 0){
        perror("Sem_post");
        exit(EXIT_FAILURE);
    }

    fclose(f);
}

void sc_child(void* arg){
    thread_data* td = (thread_data*)arg;
    unsigned long stats[td->thread_n];

    for(unsigned i = 0; i < td->thread_n; i++){
        stats[i] = 0;
    }

    unsigned done_counter = 0;

    while(1){
        if(sem_wait(&td->sc->sem_r) != 0){
            perror("sem_wait");
            exit(EXIT_FAILURE);
        }

        if(td->sc->done){
            td->sc->done = 0;
            done_counter++;

            if(done_counter == td->thread_n){
                break;
            }
        }else{
            printf("c: (thread_n: %u, %s, %d)\n", td->sc->thread_n, td->sc->nome, td->sc->value);
            stats[td->sc->thread_n - 1] += td->sc->value;
        }

        if(sem_post(&td->sc->sem_w) != 0){
            perror("sem_post");
            exit(EXIT_FAILURE);
        }
    }

    for(unsigned i = 0; i < td->thread_n; i++){
        printf("DB-IN-%u: totale %lu\n", i + 1, stats[i]);
    }
}

int main(int argc, char** argv){
    if(argc < 3 || (argc - 1) % 2 != 0){
        fprintf(stderr, "Usage: %s <data-base-1> <data-base-2> <query-file-1> <query-file-2> <...>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    unsigned nthread = (argc - 1)/2;
    thread_data td[nthread + 1];
    shared_db* sdb = init_shared_db(nthread);
    shared_c* sc = init_shared_c();

    for(int i = 0; i < nthread; i++){
        td[i].db_file = argv[i + 1];
        td[i].query_file = argv[(argc - 1)/2 + i + 1];
        td[i].sdb = sdb;
        td[i].sc = sc;
        td[i].thread_n = i + 1;

        if(pthread_create(&td[i].tid, NULL, (void*)db_child, &td[i]) != 0){
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }

    td[nthread].sc = sc;
    td[nthread].thread_n = nthread;

    if(pthread_create(&td[nthread].tid, NULL, (void*)sc_child, &td[nthread]) != 0){
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    for(unsigned i = 0; i <= nthread; i++){
        if(pthread_join(td[i].tid, NULL) != 0){
            perror("pthread_join");
            exit(EXIT_FAILURE);
        }
    }

    destroy_shared_c(sc);
    destroy_shared_db(sdb);
}

