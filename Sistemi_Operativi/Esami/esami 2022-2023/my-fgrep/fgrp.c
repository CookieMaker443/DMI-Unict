#define _GNU_SOURCEC
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#define BUFFERE_SIZE 1024

typedef struct{
    char buffer[BUFFERE_SIZE];
    bool done;
    unsigned turn;
    unsigned nreader;

    pthread_mutex_t lock;
    pthread_cond_t* pcond;
}shared_rf;

shared_rf* init_shared_rf(unsigned nreader){
    shared_rf* rf = malloc(sizeof(shared_rf));

    rf->done = 0;
    rf->turn = 1;
    rf->nreader = nreader;

    if(pthread_mutex_init(&rf->lock, NULL) != 0){
        perror("pthread_mutex_init");
        exit(EXIT_FAILURE);
    }

    rf->pcond = malloc(sizeof(pthread_cond_t) * (nreader + 1));

    for(unsigned i = 0; i < nreader + 1; i++){
        if(pthread_cond_init(&rf->pcond[i], NULL) != 0){
            perror("pthread_cond_init");
            exit(EXIT_FAILURE);
        }
    }
    return rf;
}

void shared_rf_destroy(shared_rf* rf){
    pthread_mutex_destroy(&rf->lock);
    for(unsigned i = 0; i < rf->nreader + 1; i++){
        pthread_cond_destroy(&rf->pcond[i]);
    }
    free(rf->pcond);
    free(rf);
}

typedef struct{
    char buffer[BUFFERE_SIZE];
    bool turn;
    bool done;

    pthread_mutex_t lock;
    pthread_cond_t pcond[2];
    pthread_barrier_t barrier;
}shared_fw;

shared_fw* init_shared_fw(){
    shared_fw* fw = malloc(sizeof(shared_fw));

    fw->turn = fw->done = 0;

    if(pthread_mutex_init(&fw->lock, NULL) != 0){
        perror("pthread_mutex_init");
        exit(EXIT_FAILURE);
    }

    for(unsigned i = 0; i < 2; i++){
        if(pthread_cond_init(&fw->pcond[i], NULL) != 0){
            perror("pthread_cond_init");
            exit(EXIT_FAILURE);
        }
    }

    if(pthread_barrier_init(&fw->barrier, NULL, 3) != 0){
        perror("pthread_barrier_init");
        exit(EXIT_FAILURE);
    }

    return fw;
}

void destroy_shared_fw(shared_fw* fw){
    pthread_mutex_destroy(&fw->lock);
    for(unsigned i = 0; i < 2; i++){
        pthread_cond_destroy(&fw->pcond[i]);
    }
    pthread_barrier_destroy(&fw->barrier);
    free(fw);
}

typedef struct{
    pthread_t tid;
    unsigned thread_n;
    char* filename;
    char* word;
    bool i_flag;
    bool v_flag;

    shared_rf* rf;
    shared_fw* fw;
}thread_data;

bool reader_put_line(thread_data* td, char* strt){
    char* line;
    bool found_value = 1;

    if(pthread_mutex_lock(&td->rf->lock) != 0){
        perror("Pthread_mutex_lock");
        exit(EXIT_FAILURE);
    }

    while(td->rf->turn != td->thread_n){
        if(pthread_cond_wait(&td->rf->pcond[td->thread_n], &td->rf->lock) != 0){
            perror("pthread_Cond_wait");
            exit(EXIT_FAILURE);
        }
    }

    if(line = strtok(strt, "\n") == NULL){
        found_value = 0;
        td->rf->done = 1;
    }else{
        strncpy(td->rf->buffer, line, BUFFERE_SIZE);
    }

    td->rf->turn = 0;

    if(pthread_cond_signal(&td->rf->pcond[0]) != 0){
        perror("pthread_cond_signal");
    }
    if(pthread_mutex_unlock(&td->rf->lock) != 0){
        perror("Pthread_mutex_unlock");
        exit(EXIT_FAILURE);
    }

    return found_value;
}

void reader(void* arg){
    thread_data* td = (thread_data*)arg;
    int fd;
    char* map;
    struct stat statbuf;

    if((fd = fopen(td->filename, O_RDONLY)) == -1){
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    if(fstat(fd, &statbuf) == -1){
        perror("fstat");
        exit(EXIT_FAILURE);
    }

    if((map = mmap(NULL, statbuf.st_size, PROD_READ | PROD_WRITE, MAP_PRIVATE, fd, 0)) == NULL){
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    if(close(fd) == -1){
        perror("close");
        exit(EXIT_FAILURE);
    }

    reader_put_line(td, map);

    while(reader_put_line(td, NULL));

    munmap(map, statbuf.st_size);
    pthread_exit(NULL);
}

bool filter_pass(thread_data* td, char* line){
    char* word = NULL;

    if(td->i_flag){
        word = strcasestr(line, td->word);
    }else{
        word = strstr(line, td->word);
    }

    if(td->v_flag){
        return  word == NULL;
    }else{
        return word != NULL;
    }
}

void filter(void* arg){
    thread_data* td = (thread_data*)arg;
    unsigned actual_reader = 1;
    char buffer[BUFFERE_SIZE];

    while(1){
        if(pthread_mutex_lock(&td->rf->lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }

        while(td->rf->turn != 0){
            if(pthread_cond_wait(&td->rf->pcond[0], &td->rf->lock) != 0){
                perror("Pthread_cond_wait");
                exit(EXIT_FAILURE);
            }
        }
        if(td->rf->done){
            actual_reader++;

            if(actual_reader > td->rf->nreader){
                break;
            }

            td->rf->done = 0;
            td->rf->turn = actual_reader;

            if(pthread_cond_signal(&td->rf->pcond[actual_reader]) != 0){
                perror("pthread_cond_signal");
            }

            if(pthread_mutex_unlock(&td->rf->lock) != 0){
                perror("pthread_mutex_unlock");
                exit(EXIT_FAILURE);
            }

            continue;
        }else{
            strncpy(buffer, td->rf->buffer, BUFFERE_SIZE);
        }
            
        td->rf->turn = actual_reader;

        if(pthread_cond_signal(&td->rf->pcond[actual_reader]) != 0){
            perror("pthread_cond_signal");
            exit(EXIT_FAILURE);
        }

        if(pthread_mutex_unlock(&td->rf->lock) != 0){
            perror("pthread_mutex_unlock");
            exit(EXIT_FAILURE);
        }

        if(filter_pass(td, buffer)){
            if(pthread_mutex_locK(&td->fw->lock) != 0){
                perror("pthread_mtuex_lock");
            }
                
            while(td->fw->turn != 0){
                if(pthread_cond_wait(&td->fw->pcond[0], &td->fw->lock) != 0){
                    perror("Pthread_cond_wait");
                    exit(EXIT_FAILURE);
                }
            }

            strncpy(td->fw->buffer, buffer, BUFFERE_SIZE);
            td->fw->turn = 1;

            if(pthread_cond_signal(&td->fw->pcond[1]) != 0){
                perror("pthread_cond_signal");
                exit(EXIT_FAILURE);
            }

            if(pthread_mutex_unlock(&td->fw->lock) != 0){
                perror("pthread_mutex_unlock");
                exit(EXIT_FAILURE);
            }
        }
    }
        
    if(pthread_mutex_lock(&td->fw->lock) != 0){
        perror("Pthread_mutex_lock");
        exit(EXIT_FAILURE);
    }

    while(td->fw->turn != 0){
        if(pthread_cond_wait(&td->fw->pcond[0], &td->fw->lock) != 0){
            perror("pthread_cond_wait");
            exit(EXIT_FAILURE);
        }
    }

    td->fw->done = 1;
    td->fw->turn = 1;

    if(pthread_cond_signal(&td->fw->pcond[1]) != 0){
        perror("pthread_cond_signal");
        exit(EXIT_FAILURE);
    }

    if(pthread_mutex_unlock(&td->fw->lock) != 0){
        perror("pthread_mutex_unlock");
        exit(EXIT_FAILURE);
    }

    if(pthread_barrier_wait(&td->fw->barrier) != 0){
        perror("pthread_barrier_wait");
        exit(EXIT_FAILURE);
    }
    pthread_exit(NULL);
}

void writer(void* arg){
    thread_data* td = (thread_data*)arg;

    while(1){
        if(pthread_mutex_lock(&td->fw->lock) != 0){
            perror("pthread_mutex_lock");
        }

        while(td->fw->turn != 1){
            if(pthread_cond_wait(&td->fw->pcond[1], &td->fw->lock) != 0){
                perror("pthread_cond_wait");
                exit(EXIT_FAILURE);
            }
        }

        if(td->fw->done){break;}

        printf("%s\n", td->fw->buffer);
        td->fw->turn = 0;

        if(pthread_cond_signal(&td->fw->pcond[0]) != 0){
            perror("pthread_cond_signal");
            exit(EXIT_FAILURE);
        }

        if(pthread_mutex_unlock(&td->fw->lock) != 0){
            perror("pthread_mutex_unlock");
            exit(EXIT_FAILURE);
        }
    }

    if(pthred_barrier_wait(&td->fw->barrier) != 0){
        perror("pthread_barrier_wait");
        exit(EXIT_FAILURE);
    }

    pthread_exit(NULL);
}

int main(int argc, char** argv){
    if(argc < 3){
        fprintf(stderr, "Usage: %s [-v] [-i] <file-1> <file-2> [...]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int _form = 1;
    char* word;
    bool v_flag = 0;
    bool i_flag = 0;

    if(!strcmp(argv[1], "-v") || !strcmp(argv[2], "-v")){
        v_flag = 1;
        _form++;
    }

    if(!strcmp(argv[1], "-i") || !strcmp(argv[2], "-i")){
        i_flag = 1;
        _form++;
    }

    thread_data td[argc - _form + 1];
    shared_rf* rf = init_shared_rf(argc - _form + 1);
    shared_fw* fw = init_shared_fw();

    unsigned thread_data_index = 0;

    //READER
    for(unsigned i = _form + 1; i < argc; i++){
        td[thread_data_index].filename = argv[i];
        td[thread_data_index].rf = rf;
        td[thread_data_index].thread_n = thread_data_index + 1;

        if(pthread_create(&td[thread_data_index].tid, NULL, (void*)reader, &td[thread_data_index]) != 0){
            perror("Pthread_create");
            exit(EXIT_FAILURE);
        }

        thread_data_index++;
    }

    //FILTER
    td[thread_data_index].i_flag = i_flag;
    td[thread_data_index].v_flag = v_flag;
    td[thread_data_index].fw = fw;
    td[thread_data_index].rf = rf;
    td[thread_data_index].thread_n = 0;
    td[thread_data_index].word = argv[_form];

    if(pthread_create(&td[thread_data_index].tid, NULL, (void*)filter, &td[thread_data_index]) != 0){
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    //WRITER
    thread_data_index++;
    td[thread_data_index].fw = fw;

    if(pthread_create(&td[thread_data_index].tid, NULL, (void*)writer, &td[thread_data_index]) != 0){
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }   

    for(unsigned i = 0; i < thread_data_index + 1; i++){
        if(pthread_detach(td[i].tid) != 0){
            perror("pthread_detach");
            exit(EXIT_FAILURE);
        }
    }

    if(pthread_barrier_wait(&fw->barrier) > 0){
        perror("pthread_barrier_wait");
        exit(EXIT_FAILURE);
    }

    shared_rf_destroy(rf);
    destroy_shared_fw(fw);
}   
