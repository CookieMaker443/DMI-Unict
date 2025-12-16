#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#define SHM_SIZE sizeof(shm_data)

enum {S_P1, S_P2, S_G, S_T};
enum {SASSO, CARTA, FORBICE};
char* mosse[3] = {"SASSO", "CARTA", "FORBICE"};

typedef struct{
    char mossa_p1;
    char mossa_p2;
    char vincitore;
    char done;
} shm_data;

int WAIT(int sem_id, int sem_num){
    struct sembuf ops[1] = {{sem_num, -1, 0}};
    return semop(sem_id, ops, 1);
}

int SIGNAL(int sem_id, int sem_num){
    struct sembuf ops[1] = {{sem_num, +1, 0}};
    return semop(sem_id, ops, 1);
}

int init_shm(){
    int shm_des;

    if((shm_des = shmget(IPC_PRIVATE, SHM_SIZE, IPC_CREAT | 0600)) == -1){
        perror("shm_des");
        exit(1);
    }

    return shm_des;
}


int init_sem(){
    int sem_des;

    if((sem_des = semget(IPC_PRIVATE, 4, IPC_CREAT | 0600)) == -1){
        perror("sem_des");
        exit(1);
    }

    if(semctl(sem_des, S_P1, SETVAL, 1) == -1){
        perror("semctl SETVAL S_P1");
        exit(1);
    }

    if(semctl(sem_des, S_P2, SETVAL, 1) == -1){
        perror("semctl SETVAL S_P2");
        exit(1);
    }

    if(semctl(sem_des, S_G, SETVAL, 0) == -1){
        perror("semctl SETVAL S_G");
        exit(1);
    }

    if(semctl(sem_des, S_T, SETVAL, 0) == -1){
        perror("semctl SETVAL S_T");
        exit(1);
    }


    return sem_des;
}

void player(char id, int shm_des, int sem_des){
    srand(time(NULL));

    shm_data* data;
    
    if((data = (shm_data*)shmat(shm_des, NULL, 0)) == (shm_data*)-1){
        perror("shamat");
        exit(1);
    }

    while(1){
        if(id == S_P1){
            WAIT(sem_des, S_P1);

            if(data->done){
                exit(0);
            }

            data->mossa_p1 = rand()%3;
            printf("P1: mossa '%s'\n", mosse[data->mossa_p1]);
        }else{
            WAIT(sem_des,S_P2);

            if(data->done){
                exit(0);
            }

            data->mossa_p2 = rand()%3;
            printf("P2: mossa'%s'\n", mosse[data->mossa_p2]);
        }
        SIGNAL(sem_des, S_G);
    }
}

char whowins(char mossa1, char mossa2){
    if(mossa1 == mossa2){
        return 0;
    }

    if((mossa1 == CARTA && mossa2 == SASSO)||
     (mossa1 == FORBICE && mossa2 == CARTA)||
     (mossa1 == SASSO && mossa2 == FORBICE)){
        return 1;
     }
    return 2;
}

void giudice(int shm_des, int sem_des, int partite){
    shm_data* data;
    unsigned match = 0;
    char winner;

    if((data = (shm_data*)shmat(shm_des, NULL, 0)) == (shm_data*)-1){
        perror("shmat");
        exit(1);
    }

    while(1){
        WAIT(sem_des, S_G);
        WAIT(sem_des, S_G);
        
        winner = whowins(data->mossa_p1, data->mossa_p2);

        if(!winner){
            printf("Giudice: il match n.%d è patta quindi si deve rifare\n", match + 1);
            SIGNAL(sem_des, S_P1);
            SIGNAL(sem_des, S_P2);
            continue;
        }

        data->vincitore = winner;
        match++;
        printf("Giudice: match n.%d vinta da P%d\n", match, winner);
        SIGNAL(sem_des, S_T);

        if(match == partite){
            break;
        }
    }
    exit(0);
}

void tabellone(int shm_des, int sem_des, int partite){
    shm_data* data;
    unsigned score[2] = {0,0};

    if((data = (shm_data*)shmat(shm_des, NULL, 0)) == (shm_data*)-1){
        perror("shmat");
        exit(1);
    }

    for(int i = 0; i< partite - 1; i++){
        WAIT(sem_des, S_T);

        score[data->vincitore - 1]++;

        printf("Tab: classifica temporanea: P1: %d P2: %d\n", score[0], score[1]);
        SIGNAL(sem_des, S_P1);
        SIGNAL(sem_des, S_P2);
    }

    WAIT(sem_des, S_T);

    score[data->vincitore - 1]++;

    printf("Tab: classifica finale: P1:%d P2:%d\n", score[0], score[1]);

    if(score[0] > score[1]){
        printf("Tab: il vincitore è P1\n");
    }else{
        printf("Tab: il vincitore è P2\n");
    }

    data->done = 1;
    SIGNAL(sem_des, S_P1);
    SIGNAL(sem_des, S_P2);
}

int main(int argc, char** argv){
    if(argc != 2){
        fprintf(stderr, "Usage: %s <numero-partite>\n", argv[0]);
        exit(1);
    }

    int shm_des = init_shm();
    int sem_des = init_sem();

    if(!fork()){
        player(S_P1, shm_des, sem_des);
    }

    sleep(1);

    if(!fork()){
        player(S_P2, shm_des, sem_des);
    }

    if(!fork()){
        giudice(sem_des, sem_des, atoi(argv[1]));
    }

    if(!fork()){
        tabellone(shm_des, sem_des, atoi(argv[1]));
    }

    for(int i = 0; i < 4; i++){
        wait(NULL);
    }

    shmctl(shm_des, IPC_RMID, NULL);
    semctl(sem_des, 0, IPC_RMID);
}