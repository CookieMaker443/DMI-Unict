#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include <stdbool.h>
#define MOSSA_

typedef enum{P_1, P_2, P_G, P_T} thread_n;
typedef enum{ SASSO, CARTA, FORBICI} mosse;
char* mosse_effettuabili[3] = {"sasso", "carta", "forbice"};

typedef struct{
    mosse mossa[2];
    char vincitore;
    bool done;
    unsigned n_matche;

    sem_t sem[4];
}shared;

typedef struct{
    pthread_t tid;
    thread_n nthread;

    shared* sh;
}thread_data;

shared* init_shared(unsigned n_matches){
    shared* sh = malloc(sizeof(shared));

    sh->vincitore = 0;
    sh->done = 0;
    sh->n_matche = n_matches;

    if(sem_init(&sh->sem[P_1], 0, 1) != 0){
        perror("sem_init");
        exit(EXIT_FAILURE);
    }
    if(sem_init(&sh->sem[P_2], 0, 1) != 0){
        perror("sem_init");
        exit(EXIT_FAILURE);
    }
    if(sem_init(&sh->sem[P_G], 0, 0) != 0){
        perror("sem_init");
        exit(EXIT_FAILURE);
    }
    if(sem_init(&sh->sem[P_T], 0, 0) != 0){
        perror("sem_init");
        exit(EXIT_FAILURE);
    }

    return sh;
}

void destroy_shared(shared* sh){
    for(int i=0; i<4; i++){
        sem_destroy(&sh->sem[i]);
    }
    free(sh);
}

void player(void* arg){
    thread_data* td = (thread_data*)arg;

    while(1){
        if(sem_wait(&td->sh->sem[td->nthread]) != 0){
            perror("sem_wait");
            exit(EXIT_FAILURE);
        }

        if(td->sh->done){
            break;
        }

        td->sh->mossa[td->nthread] = rand() % 3;

        printf("P%d: mossa '%s'\n", td->nthread + 1, mosse_effettuabili[td->sh->mossa[td->nthread]]);

        if(sem_post(&td->sh->sem[P_G]) != 0){
            perror("sem_post");
            exit(EXIT_FAILURE);
        }
    }
    pthread_exit(NULL);
}

char whowins(mosse* m){
    if(m[P_1] == m[P_2]){
        return 0;
    }
    if(m[P_1] == CARTA && m[P_2] == SASSO || m[P_1] == FORBICI && m[P_2] == CARTA || m[P_1] == SASSO && m[P_2] == FORBICI){
        return 1;
    }
    return 2;
}

void giudice(void* arg){
    thread_data* td = (thread_data*)arg;
    char winner;
    unsigned match_completati = 0;


    while(1){
        if(sem_wait(&td->sh->sem[P_G]) != 0){
            perror("sem_wait");
            exit(EXIT_FAILURE);
        }
        if(sem_wait(&td->sh->sem[P_G]) != 0){
            perror("sem_wait");
            exit(EXIT_FAILURE);
        }

        if(td->sh->done){
            break;
        }

        printf("G: mossa P1 %s\t", mosse_effettuabili[td->sh->mossa[P_1]]);
        printf("G: mossa P2 %s\t", mosse_effettuabili[td->sh->mossa[P_2]]);

        winner = whowins(td->sh->mossa);

        if(!winner){
            printf("G: patta la partita si deve rifare");
            if(sem_post(&td->sh->sem[P_1]) != 0){
                perror("sem_post");
                exit(EXIT_FAILURE);
            }
            if(sem_post(&td->sh->sem[P_2]) != 0){
                perror("sem_post");
                exit(EXIT_FAILURE);
            }
        }else{
            match_completati++;
            td->sh->vincitore = winner;
            printf("G: partita n%d vinta da P%d\n", match_completati, winner);

            if(sem_post(&td->sh->sem[P_T]) != 0){
                perror("sem_post");
                exit(EXIT_FAILURE);
            }
        }
    }
    pthread_exit(NULL);
}

void tabellone(void* arg){
    thread_data* td = (thread_data*)arg;
    unsigned score[2] = {0, 0};

    for(unsigned i = 0; i < td->sh->n_matche; i++){
        if(sem_wait(&td->sh->sem[P_T]) != 0){
            perror("sem_wait");
            exit(EXIT_FAILURE);
        }

        score[td->sh->vincitore - 1]++; //inserisco il punteggio nello score

        if(i < td->sh->n_matche - 1){
            printf("T: classifica temporanea: P1:%d P2:%d", score[0], score[1]);

            if(sem_post(&td->sh->sem[P_1]) != 0){
                perror("sem_post");
                exit(EXIT_FAILURE);
            }
            if(sem_post(&td->sh->sem[P_2]) != 0){
                perror("sem_post");
                exit(EXIT_FAILURE);
            }
        }
    }

    printf("T: classifica finale: %d %d\n", score[0], score[1]);

    if(score[0] == score[1]){
        printf("T: Il torneo è finito in parità");
    }else{
        if(score[0] > score[1]){
            printf("T: il vincitore è P1\n");
        }else{
            printf("T: il vincitore è P2\n");
        }
    }

    td->sh->done = 1;

    if(sem_post(&td->sh->sem[P_1]) != 0){
        perror("sem_post");
        exit(EXIT_FAILURE);
    }
    if(sem_post(&td->sh->sem[P_2]) != 0){
        perror("sem_post");
        exit(EXIT_FAILURE);
    }

    for(int i = 0; i < 2; i++){
        if(sem_post(&td->sh->sem[P_G]) != 0){
            perror("sem_post");
            exit(EXIT_FAILURE);
        }
    }
    pthread_exit(NULL);
}

int main(int argc, char** argv){
    if(argc < 2){
        fprintf(stderr, "Usage: %s <numero-partite>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    unsigned nmatches = atoi(argv[1]);

    if(nmatches == 0){
        fprintf(stderr, "Usage: %s <numero-partite>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    thread_data td[4];
    shared* sh = init_shared(nmatches);

    for(int i = 0; i < 4; i++){
        td[i].nthread = i;
        td[i].sh = sh;
    }

    srand(time(NULL));

    if(pthread_create(&td[P_1].tid, NULL, (void*)player, &td[P_1]) != 0){
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }
    if(pthread_create(&td[P_2].tid, NULL, (void*)player, &td[P_2]) != 0){
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }
    if(pthread_create(&td[P_G].tid, NULL, (void*)giudice, &td[P_G]) != 0){
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    if(pthread_create(&td[P_T].tid, NULL, (void*)tabellone, &td[P_T]) != 0){
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    for(int i =  0; i < 4; i++){
        if(pthread_join(td[i].tid, NULL) != 0){
            perror("pthread_join");
            exit(EXIT_FAILURE);
        }
    }
    
    destroy_shared(sh);
    exit(EXIT_SUCCESS);
}
