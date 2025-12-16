#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#define TO_MOVE 3

typedef enum{P_1, P_2, P_G, P_T}thread_n;
typedef enum{CARTA, FORBICE, SASSO}mossa;
char* nome_mosse[3] = {"carta", "forbice", "sasso"};

typedef struct{
    mossa mosse[2];
    char vincitore;
    bool done;
    unsigned n_matches;

    pthread_mutex_t lock;
    pthread_cond_t pcond[4];
}match;

typedef struct{
    pthread_t tid;
    thread_n thread_n;

    match* m;
}thread_data;

match* init_match(unsigned short n_match){
    match* m = malloc(sizeof(match));

    m->n_matches = n_match;
    m->done = 0;
    m->vincitore = 0;
    m->mosse[P_1] = m->mosse[P_2] = TO_MOVE;

    if(pthread_mutex_init(&m->lock, NULL) != 0){
        perror("pthread_mutex_init");
        exit(EXIT_FAILURE);
    }

    for(int i = 0; i < 4; i++){
        if(pthread_cond_init(&m->pcond[i], NULL) != 0){
            perror("pthread_cond_init");
            exit(EXIT_FAILURE);
        }
    }

    return m;
}

void destroy_match(match* m){
    pthread_mutex_destroy(&m->lock);
    for(int i = 0; i < 4; i++){
        pthread_cond_destroy(&m->pcond[i]);
    }
    free(m);
}

void player(void* arg){
    thread_data* td = (thread_data*)arg;

    while(1){
        if(pthread_mutex_lock(&td->m->lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }

        while(td->m->mosse[td->thread_n] != TO_MOVE && !td->m->done){
            if(pthread_cond_wait(&td->m->pcond[td->thread_n], &td->m->lock) != 0){
                perror("pthread_cond_wait");
                exit(EXIT_FAILURE);
            }
        }

        if(td->m->done){
            if(pthread_mutex_unlock(&td->m->lock) != 0){
                perror("pthread_mutex_unlock");
                exit(EXIT_FAILURE);
            }
            break;
        }

        td->m->mosse[td->thread_n] = rand() % 3;

        printf("P%d: mossa '%s'\n", td->thread_n + 1, nome_mosse[td->m->mosse[td->thread_n]]);

        if(pthread_cond_signal(&td->m->pcond[P_G]) != 0){
            perror("pthread_cond_signal");
            exit(EXIT_FAILURE);
        }

        if(pthread_mutex_unlock(&td->m->lock) != 0){
            perror("pthread_mutex_unlock");
            exit(EXIT_FAILURE);
        }
    }

    pthread_exit(NULL);
}

char whowins(mossa* m){
    if(m[P_1] == m[P_2]){
        return 0;
    }

    if(m[P_1] == CARTA && m[P_2] == SASSO || m[P_1] == FORBICE && m[P_2] == CARTA || m[P_1] == SASSO && m[P_2] == FORBICE){
        return 1;
    }
    return 2;
}

void giudice(void* arg){
    thread_data* td = (thread_data*)arg;
    char winner;
    unsigned match_complete  = 0;

    while(1){
        if(pthread_mutex_lock(&td->m->lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }

        while((td->m->mosse[P_1] == TO_MOVE || td->m->mosse[P_2] == TO_MOVE) && !td->m->done){
            if(pthread_cond_wait(&td->m->pcond[P_G], &td->m->lock) != 0){
                perror("pthread_cond_wait");
                exit(EXIT_FAILURE);
            }
        }

        if(td->m->done){
            if(pthread_mutex_unlock(&td->m->lock) != 0){
                perror("pthread_mutex_unlock");
                exit(EXIT_FAILURE);
            }
            break;
        }

        printf("G: mossa P1: %s\t", nome_mosse[td->m->mosse[P_1]]);
        printf("G: mossa P2: %s\t", nome_mosse[td->m->mosse[P_2]]);

        winner = whowins(td->m->mosse);

        td->m->mosse[P_1] = TO_MOVE;
        td->m->mosse[P_2] = TO_MOVE;

        if(!winner){
            if(pthread_cond_signal(&td->m->pcond[P_1])!= 0){
                perror("pthread_cond_signal");
                exit(EXIT_FAILURE);
            }
            if(pthread_cond_signal(&td->m->pcond[P_2]) != 0){ 
                perror("pthread_cond_signal");
                exit(EXIT_FAILURE);                
            }
        }else{
            match_complete++;
            td->m->vincitore = winner;
            printf("G: partita n.%d vinta da P%d\n", match_complete, winner);
            if(pthread_cond_signal(&td->m->pcond[P_T]) != 0){
                perror("pthread_cond_signal");
                exit(EXIT_FAILURE);
            }
        }

        if(pthread_mutex_unlock(&td->m->lock) != 0){
            perror("pthread_mutex_unlock");
            exit(EXIT_FAILURE);
        }
    }
    pthread_exit(NULL);
}

void tabellone(void* arg){
    thread_data* td = (thread_data*)arg;
    unsigned score[2] = {0, 0};

    for(unsigned i = 0; i < td->m->n_matches; i++){ //scorro in base al numero di match
        if(pthread_mutex_lock(&td->m->lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }

        while(td->m->vincitore == 0){
            if(pthread_cond_wait(&td->m->pcond[P_T], &td->m->lock) != 0){
                perror("pthread_cond_wait");
                exit(EXIT_FAILURE);
            }
        }

        score[td->m->vincitore - 1]++;
        if(i < td->m->n_matches - 1){
            printf("T: classifica temporanea: P1:%d P2:%d\n", score[0], score[1]);
            td->m->vincitore = 0;
        }else{
            printf("T: classifica finale: %d %d\n",score[0], score[1]);

            if(score[0] == score[1]){
                printf("T: il torneo è finito il parità\n");
            }else{
                if(score[0] > score[1]){
                    printf("T: il vincitore del torneo è P1\n");
                }else{
                    printf("T: il vincitore del torneo è P2\n");
                }
            }

            td->m->done = 1;
            if(pthread_cond_signal(&td->m->pcond[P_G]) != 0){
                perror("pthread_cond_signal");
                exit(EXIT_FAILURE);
            }
        }

        if(pthread_cond_signal(&td->m->pcond[P_1]) != 0){
            perror("pthread_cond_siganl");
            exit(EXIT_FAILURE);
        }

        if(pthread_cond_signal(&td->m->pcond[P_2]) != 0){
            perror("pthread_cond_siganl");
            exit(EXIT_FAILURE);
        }

        if(pthread_mutex_unlock(&td->m->lock) != 0){
            perror("pthread_mutex_unlock");
            exit(EXIT_FAILURE);
        }
    }
    pthread_exit(NULL);
}

int main(int argc, char** argv){
    if(argc < 2){
        fprintf(stderr, "Usage %s <numero-partite>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    unsigned n_matche = atoi(argv[1]);

    if(n_matche == 0){
        fprintf(stderr, "Usage %s <numero-partite>\n", argv[0]);
        exit(EXIT_FAILURE);    
    }
    
    thread_data td[4];

    match* m = init_match(n_matche);

    for(int i = 0; i < 4; i++){
        td[i].thread_n = i;
        td[i].m = m;
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
    destroy_match(m);
    exit(EXIT_SUCCESS);
}