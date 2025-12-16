#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#define BUFFER_SIZE 2048
#define MSG_SIZE sizeof(query_msg) - sizeof(long)
#define OUT_MSG sizeof(query_out) - sizeof(long)

typedef struct{
    long type;
    char id;
    char key[BUFFER_SIZE];
    char done;
}query_msg;

typedef struct{
    int value;
    char key[BUFFER_SIZE];
}entry;


typedef struct{
    long type;
    char id;
    char done;
    entry e;
}query_out;

typedef struct{
    entry* e;
    struct node* next;
}node;

typedef node* list;

list insert(list l, entry* e){
    node* ptr = malloc(sizeof(node));
    ptr->e = e;
    ptr->next = NULL;

    if(l == NULL){
        l = ptr;
    }else{
        ptr->next = (struct node*)l;
        l = ptr;
    }

    return l;
}

entry* search(list l ,char* key){
    node* ptr = l;

    if(ptr != NULL){    
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
        printf("Entry-> key: %s, value: %d\n", ptr->e->key, ptr->e->value);
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


void in_child(char id, int queue, char* path){
    FILE* f;
    query_msg msg;
    msg.type = 1;
    msg.done = 0;
    msg.id = id;
    unsigned counter = 0;

    if((f = fopen(path, "r")) == NULL){
        perror("fopen");
        exit(1);
    }

    while(fgets(msg.key, BUFFER_SIZE, f)){
        counter++;
        if(msg.key[strlen(msg.key) - 1] == '\n'){
            msg.key[strlen(msg.key) - 1] = '\0';
        }

        if(msgsnd(queue, &msg, MSG_SIZE, 0) == -1){
            perror("msgsnd");
            exit(1);
        }

        printf("IN%d: inviate query n.%d, '%s'\n", id, counter, msg.key);
    }

    msg.done = 1;

    if(msgsnd(queue, &msg, MSG_SIZE, 0) == -1){
        perror("msgsnd");
        exit(1);
    }

    fclose(f);
}

entry* create_entry(char* data){
    entry* e = malloc(sizeof(entry));
    char* key;
    char* value;

    if(data[strlen(data)-1] == '\n'){
        data[strlen(data) - 1 ] = '\0';
    }

    if((key = strtok(data, ":")) != NULL){
        if((value = strtok(NULL, ":")) != NULL){
            strncpy(e->key, key, BUFFER_SIZE);
            e->value = atoi(value);
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

void db_child(int in_queue, int out_queue, char* path){
    query_msg q_msg;
    query_out o_msg;
    o_msg.type = 1;
    o_msg.done = 0;
    char done_counter = 0;
    list l = load_database(path);
    entry* e;

    while(1){
        if(msgrcv(in_queue, &q_msg, MSG_SIZE, 0, 0) == -1){
            perror("msgrcv");
            exit(1);
        }

        if(q_msg.done){
            done_counter++;

            if(q_msg.done < 2){
                continue;
            }else{
                break;
            }
        }

        e = search(l, q_msg.key);

        if(e != NULL){
            o_msg.e = *e;
            o_msg.id = q_msg.id;
            if(msgsnd(out_queue, &o_msg, OUT_MSG, 0) == -1){
                perror("msgsnd");
                exit(1);
            }

            printf("DB: query '%s' da IN%d trovata con valore %d\n", q_msg.key, q_msg.id, e->value);    
        }
        else{
            printf("DB: query '%s' da IN%d non trovata\n", q_msg.key, q_msg.id);
        }
    }

    o_msg.done = 1;

    if(msgsnd(out_queue, &o_msg, OUT_MSG, 0) == -1){
        perror("msgsnd");
        exit(1);
    }

    destroy(l);
    exit(0);
}

void out_child(int out_queue){
    query_out o_msg;
    unsigned record1 = 0, record2 = 0;
    int value1 = 0, value2 = 0;

    while(1){
        if((msgrcv(out_queue, &o_msg, OUT_MSG, 0, 0)) == -1){
            perrro("msgrcv");
            exit(1);
        }

        if(o_msg.done){
            break;
        }

        if(o_msg.id == 1){
            record1++;
            value1 += o_msg.e.value;
        }
        else if(o_msg.id == 2){
            record2++;
            value2 += o_msg.e.value;
        }
    }

    printf("OUT: ricevuti n.%d valori validi di IN1 con totale %d\n", record1, value1);
    printf("OUT: ricevuti n:%d valori validi di IN2 con totale %d\n", record2, value2);

    exit(0);
}


int main(int argc, char** argv){
    if(argc < 4){
        fprintf(stderr, "Usage: %s <db-file> <query-1> <query-2>\n", argv[0]);
        exit(1);
    }

    int queue1, queue2;

    if((queue1 = msgget(IPC_PRIVATE, IPC_CREAT | 0600)) == -1){
        perror("msgget");
        exit(1);
    }

    if((queue2 = msgget(IPC_PRIVATE, IPC_CREAT | 0600)) == -1){
        perror("msgget");
        exit(1);
    }

    if(!fork()){
        in_child(1, queue1, argv[2]);
    }

    if(!fork()){
        in_child(2, queue2, argv[3]);
    }

    if(!fork()){
        db_child(queue1, queue2, argv[1]);
    }

    if(!fork()){
        out_child(queue2);
    }

    for(int i = 0; i < 4; i++){
        wait(NULL);
    }

    msgctl(queue1, IPC_RMID, NULL);
    msgctl(queue2, IPC_RMID, NULL);
}