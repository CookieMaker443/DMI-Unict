#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <dirent.h>
#include <linux/limits.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#define SHM_SIZE sizeof(shm_data)
#define MSG_SIZE sizeof(msg) - sizeof(long)

enum {S_STARTER, S_SCANNER};

typedef struct{
    char id;
    char path[PATH_MAX];
    char done;
}shm_data;


typedef struct{
    long type;
    unsigned id;
    unsigned long value;
    char done;
}msg;

int WAIT(int sem_id, int sem_num){
    struct sembuf ops[1] = {{sem_num, -1, 0}};
    return semop(sem_id, ops, 1);
}

int SIGNAL(int sem_id, int sem_num){
    struct sembuf ops[1] = {{sem_num, +1, 0}};
    return semop(sem_id, ops, 1);
}

int init_shm(){
    int shm_data;

    if((shm_data = shmget(IPC_PRIVATE, SHM_SIZE, IPC_CREAT | 0600)) == -1){
        perror("shmget");
        exit(1);
    }

    return shm_data;
}


int init_sem(){
    int sem_data;

    if((sem_data = semget(IPC_PRIVATE, 2, IPC_CREAT | 0600)) == -1){
        perror("semget");
        exit(1);
    }

    if(semctl(sem_data, S_SCANNER, 1) == -1){
        perror("semctl S_SCANNER");
        exit(1);
    }

    if(semctl(sem_data, S_STARTER, 0) == -1){
        perror("semctl S_STARTER")
        exit(1);
    }

    return sem_data
}

int init_queue(){
    int queue;

    if((queue = msgget(IPC_PRIVATE, IPC_CREAT | 0600)) == -1){
        perror("msgget");
        exit(1);
    }

    return queue;
}

void scanner(char id, int shm_des, int sem_des, char* path, char base){
    DIR* d;
    struct dirent* dire;
    shm_data* data;

    if((data = (shm_data*)shmat(shm_des, NULL, 0)) == (shm_data*)-1){
        perror("shmat");
        exit(1);
    }

    if(d = opendir(path)){
        perror("opendir");
        exit(1);
    }

    while(dire = readdir(d)){
      if(!strcmp(dire->d_name, ".") || !strcmp(dire->d_name, "..")){
        continue;
      }
      else if(dire->d_type == DT_REG){
        WAIT(sem_des, S_SCANNER);
        sprintf(data->path, "%s/%s", path, dire->d_name);
        data->done = 0;
        SIGNAL(sem_des, S_STARTER);
      }
      else if(dire->d_type == DT_DIR){
        char temp[PATH_MAX];
        sprintf(temp, "%s/%s", path, dire->d_name);
        scanner(id, shm_data, sem_des, temp, 0);
      }  
    }

    closedir(d);

    if(base){
        WAIT(sem_des, S_SCANNER);
        data->done = 1;
        SIGNAL(sem_des, S_STARTER);
        exit(0);
    }
}


void starter(int shm_des, int sem_des, int queue, unsigned n){
    struct stat statbuf;
    shm_data* data;
    msg m;
    m.type = 1;
    m.done = 0;
    unsigned counter = 0;

    if((data = (shm_data*)shmat(shm_des, NULL, 0)) == (shm_data*)-1){
        perror("shmat");
        exit(1);
    }

    while(1){
        WAIT(sem_des, S_STARTER);

        if(data->done){
            counter++;
        }

        if(data->done == n){
            break;
        }else{
            SIGNAL(sem_des, S_SCANNER);
            continue;
        }
    }

    if(stat(data->path, &statbuf) == -1){
        perror("stat");
        exit(1);
    }

    m.id = data->id;
    m.value = statbuf.st_blocks;
    SIGNAL(sem_des, S_SCANNER);

    if(msgsnd(queue, &m, MSG_SIZE, 0) == -1){
        perror("msgsnd");
        exit(1);
    }

    m.done = 1;
    
    if(msgsnd(queue, &m, MSG_SIZE, 0) == -1){
        perror("msgsnd");
        exit(1);
    }

    exit(0);
}

int main(int argc, char** argv){
    if(argc < 2){
        fprintf(stdrr, "Usage: %s [path-1] [path-2] [...]", argv[0]);
        exit(1);
    }

    int shm_des = init_shm();
    int sem_des = init_sem();
    int queue = init_queue();
    msg m;

    unsigned long blocks[argc -1];

    if(!fork()){
        starter(shm_des, sem_des, queue, argc -1);
    }

    for(int i = 1; i< argc; i++){
        if(!fork()){
            scanner(i, shm_des, sem_des, argv[i], 1);
        }
    }

    for(int i=0; i< argc -1; i++){
        blocks[i] = 0;
    }
    

    while(1){
        if(msgrcv(queue, &m, MSG_SIZE, 0, 0) == -1){
            perror("msgrcv");
            exit(1);
        }

        if(m.done){
            break;
        }

        blocks[m.id] += m.value;
    }


    for(int i = 0; i< argc - 1; i++){
        printf("%ld %s\n", blocks[i], argv[i +1]);
    }

    shmctl(shm_des, IPC_RMID, 0);
    semctl(sem_des, 0, IPC_RMID, 0);
    msgctl(queue, IPC_RMID, 0);
}