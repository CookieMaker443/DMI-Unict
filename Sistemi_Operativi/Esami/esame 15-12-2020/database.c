#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <sys/sem.h>
#include <unistd.h>
#define BUFFER_SIZE 2048

enum {S_IN, S_DB, S_OUT};

typedef struct{
    char key[BUFFER_SIZE];
    char id;
    char done;
}shm_query;

typedef struct{
    char key[BUFFER_SIZE];
    int val;
}entry;

typedef struct{
    entry e;
    char id;
    char done;
}shm_out;

int* init_shm(){
    int* shm_des = malloc(sizeof(int) * 2);

    if((shm_des[0] = shmget(IPC_PRIVATE, sizeof(shm_query), IPC_CREAT | 0600)) == NULL){
        perror("shmget");
        exit(1);
    }

    if((shm_des[1] = shmget(IPC_PRIVATE, sizeof(shm_out), IPC_CREAT | 0600)) == NULL){
        perror("shmget");
        exit(1);
    }

    return shm_des;
}

int init_sem(){
    int sem_des;

    if((sem_des = semget(IPC_PRIVATE, 3, IPC_CREAT | 0600)) == NULL){
        perror("semget");
        exit(1);
    }

    if(semctl(sem_des, S_IN, SETVAL, 1) == -1){
        perror("semctl IN");
        exit(1);
    }

    if(semctl(sem_des, S_DB, SETVAL, 0) == -1){
        perror("semctl DB");
        exit(1);
    }

    if(semctl(sem_des, S_OUT, SETVAL, 0) == -1){
        perror("semctl OUT");
        exit(1);
    }

    return sem_des;
}

int WAIT(int sem_id, int sem_num){
    struct sembuf ops[1] = {{sem_num, - 1, 0}};
    return semop(sem_id, ops, 1);
}

int SIGNAL(int sem_id, int sem_num){
    struct sembuf ops[1] = {{sem_num, + 1, 0}};
    return semops(sem_id, ops, 1)
}

void in_chil(char id, int shm_id, int sem_des, char* path){
    FILE* f;
    shm_query* data;
    char buffer[BUFFER_SIZE];
    unsigned counter = 0;

    if((data = (shm_query*)shmat(shm_id, NULL, 0)) == (shm_query*)-1){
        perror("shmat");
        exit(1);
    }

    if((f = fopen(path, "r")) == NULL){
        perror("fopen");
        exit(1);
    }

    while(fgets(buffer, BUFFER_SIZE, f)){
        counter++;

        if(buffer[strlen(buffer) - 1] == '\n'){
            buffer[strlen(buffer) - 1] = '\0';
        }

        WAIT(sem_des, S_IN);
        data->id = id;
        strncpy(data->key, buffer, BUFFER_SIZE);
        data->done = 0;
        SIGNAL(sem_des, S_DB);

        printf("IN%d: inviati query n.%d '%s'\n", id, counter, buffer);
    }

    WAIT(sem_des, S_IN);
    data->done = 1;
    SIGNAL(sem_des, S_DB);

    fclose(f);
    exit(0);
}

typedef struct{
    entry* e;
    struct node* next;
} node;

typedef node* list;

list insert(list l, entry* key){
    node* n = malloc(sizeof(node));
    n->e = key;
    n->next = NULL;

    if(l == NULL){
        l = n;
    }
    else{
        n->next = (struct node*)l;
        l = n;
    }

    return l;
}

entry* search(list l, char* key){
    node* ptr = l;

    while(ptr != NULL){
        if(!strcmp(ptr->e->key, key)){
            return ptr->e;
        }
        ptr = (node*)ptr->next;
    }

    return NULL;
}

void print(list l){
    node* ptr = l;

    while(ptr != NULL){
        printf("Entry -> key: %s, value %d\n", ptr->e->key, ptr->e->val);
        ptr = (node*)ptr->next;
    }
}

void destroy(list l){
    node* ptr = l;
    node* temp;

    while(ptr != NULL){
        temp = ptr;
        ptr = (node*)ptr->next;
        free(temp->e);
        free(temp);
    }
}

entry* create_entry(char* data){
    entry* e = malloc(sizeof(entry));
    char* key;
    char* value;

    if(data[strlen(data) - 1] == '\n'){
        data[strlen(data) - 1] = '0';
    }

    if(key = strtok(data, ":") != NULL){
        if((value = strtok(NULL, ":")) != NULL){
            strncpy(e->key, key, BUFFER_SIZE);
            e->val = atoi(value);
            return e;
        }
    }

    free(e);
    return NULL;
}

list load_database(char* path){
    list l = NULL;
    FILE* f;
    entry* e;
    char buffer[BUFFER_SIZE];
    unsigned counter = 0;

    if((f = fopen(path, "r")) == NULL){
        perror("fopen");
        exit(1);
    }

    while(fgets(buffer, BUFFER_SIZE, f)){
        e = create_entry(buffer);
        if(e != NULL){
            l = insert(l, e);
            counter++;
        }
    }

    printf("DB: letti n.%d record da file\n", counter);
    fclose(f);
    return l;
}


void db_child(int shm_query_id, int shm_out_id, int sem_id, char* path){
    shm_query* q_data;
    shm_out* o_data;
    char done_counter = 0;
    list l = load_database(path);
    entry* e;

    if((q_data = (shm_query*)shmat(shm_query_id, NULL, 0)) == (shm_query*)-1){
        perror("shmat");
        exit(1);
    }

    if((o_data = (shm_out*)shmat(shm_out_id, NULL, 0)) == (shm_out*)-1){
        perror("shmat");
        exit(1);
    } 

    while(1){
        WAIT(sem_id, S_DB);

        if(q_data->done){
            done_counter++;
            if(done_counter > 1){
                break;
            }else{
                SIGNAL(sem_id, S_IN);
                continue;
            }
        }
        
        e = search(l, q_data->key);

        if(e != NULL){
            printf("DB: query '%s' da IN%d trovato con valore %d\n", e->key, q_data->id, q_data->key);
            o_data->e = *e;
            o_data->id = q_data->id;
            o_data->done = 0;
            SIGNAL(sem_id, S_OUT);
        }else{
            printF("DB: query '%s' da IN%d non trovata\n", q_data->key, q_data->id);
            SIGNAL(sem_id, S_IN);
        }
    }

    o_data->done = 1;
    SIGNAL(sem_id, S_OUT);
    
    destroy(l);
    exit(0);
}


void out_child(int shm_id, int sem_id){
    shm_out* data;
    unsigned record_in1 = 0, record_in2 = 0;
    int val1 = 0, val2 = 0;

    if((data = (shm_out*)shmat(shm_id, NULL, 0)) == (shm_out*)-1){
        perror("shmat");
        exit(1);
    }

    while(1){
        WAIT(sem_id, S_OUT);

        if(data->done){
            break;
        }

        if(data->id == 1){
            record_in1++;
            val1 += data->e.val;
        }
        else if(data->id == 2){
            record_in2++;
            val2 += data->e.val;
        }

        SIGNAL(sem_id, S_IN);
    }

    printf("OUT: ricevuti n:%d valori validi di IN1 con totale %d\n", record_in1, val1);
    print("OUT: ricevuti n:%d valori validi di IN2 con totale %d\n", record_in2, val2);

    exit(0);
}

int main(int argc, char** argv){
    if(argc < 4){
        fprintf(stderr, "Usage: %s <db-file> <query-file-1> <query-file-2>\n", argv[0]);
        exit(1);
    }

    int* shm_des = init_shm();
    int sem_des = init_sem();

    if(!fork()){
        in_chil(1, shm_des[0], sem_des, argv[2]);
    }

    if(!fork()){
        in_chil(2, shm_des[0], sem_des, argv[3]);
    }

    if(!fork()){
        db_child(shm_des[0], shm_des[1], sem_des, argv[1]);
    }

    if(!fork()){
        out_child(shm_des[1], sem_des);
    }

    for(int i = 0; i < 4; i++){
        wait(NULL);
    }

    shmctl(shm_des[0], IPC_RMID, NULL);
    shmctl(shm_des[1], IPC_RMID, NULL);
    free(shm_des);
    semctl(sem_des, 0, IPC_RMID);
}