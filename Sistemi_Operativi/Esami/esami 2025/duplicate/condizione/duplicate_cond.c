#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <libgen.h>

#define BLOCK_SIZE 1024
#define STACK_CAPACITY 10
#define BUFFER_SIZE 100

typedef struct{
    char buffer[BLOCK_SIZE];
    char filename[BUFFER_SIZE];
    long file_size;
    long offset;
    long dim_buffer;
    int exit;
}record;

typedef struct{
    record stack[STACK_CAPACITY];
    int stack_ptr;
    int size;
    bool done;


    pthread_mutex_t lock;
    pthread_cond_t full, empty;
}shared;

typedef struct{
    shared* sh;
    unsigned thread_n;
    pthread_t tid;
    char filename[BUFFER_SIZE];
}thread_data_reader;

typedef struct{
    pthread_t tid;
    char dir[BUFFER_SIZE];
    shared* sh;
}thread_data_write; 


shared* init_shared(){
    shared* sh = malloc(sizeof(shared));
    
    sh->size = sh->done = 0;
    sh->stack_ptr = -1;

    if(pthread_mutex_init(&sh->lock, NULL) != 0){
        perror("Pthread_mutex_init");
        exit(EXIT_FAILURE);
    }

    if(pthread_cond_init(&sh->empty, NULL) != 0){
        perror("Pthread_cond_init");
        exit(EXIT_FAILURE);
    }

    if(pthread_cond_init(&sh->full, NULL) != 0){
        perror("Pthread_cond_init");
        exit(EXIT_FAILURE);
    }

    return sh;
}

void shared_destroy(shared* sh){
    pthread_mutex_destroy(&sh->lock);
    pthread_cond_destroy(&sh->empty);
    pthread_cond_destroy(&sh->full);
    free(sh);
}

void reader_function(void* arg){
    thread_data_reader* td = (thread_data_reader*)arg;

    int file;
    struct stat statbuf;
    if((file = open(td->filename, O_RDONLY)) == -1){
        perror("open");
        exit(EXIT_FAILURE);
    }

    if(fstat(file, &statbuf) == -1){
        perror("fstat");
        exit(EXIT_FAILURE);
    }

    printf("[READER-%u] lettura del file '%s' di %ld byte\n", td->thread_n +1, td->filename, statbuf.st_size);

    char* file_map;

    if((file_map = mmap(NULL, statbuf.st_size, PROT_READ, MAP_SHARED, file, 0)) == MAP_FAILED){
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    long offset = 0;
    while(offset < statbuf.st_size){
        if(pthread_mutex_lock(&td->sh->lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }

        while(td->sh->size == STACK_CAPACITY){
            if(pthread_cond_wait(&td->sh->empty, &td->sh->lock) != 0){
                perror("Pthread_cond_Wait");
                exit(EXIT_FAILURE);
            }
        }

        record* rec = &td->sh->stack[++td->sh->stack_ptr];
        td->sh->size++;

        int len = 0;
        if(offset + BLOCK_SIZE > statbuf.st_size){
            len = statbuf.st_size - offset;
            rec->exit = 1;
        }else{
            len = BLOCK_SIZE;
            rec->exit = 0;
        }


        strcpy(rec->filename, td->filename);
        rec->file_size = statbuf.st_size;
        rec->dim_buffer = len;
        rec->offset = offset;
        printf("[READER-%d] lettura del blocco di offset %ld di byte %d\n", td->thread_n+  1, rec->offset, len);

        if(pthread_cond_signal(&td->sh->full) != 0){
            perror("Pthread_cond_signal");
            exit(EXIT_FAILURE);
        }

        if(pthread_mutex_unlock(&td->sh->lock) != 0){
            perror("phtread_mutex_unlock");
            exit(EXIT_FAILURE);
        }

        offset += len;
    }

    munmap(file_map, statbuf.st_size);
    close(file);
    printf("[READER-%u] lettura del file '%s' completata\n", td->thread_n + 1, td->filename);
}

void path_join(char* dir, char* word, char* dest){
    strcpy(dest, dir);

    if(dest[strlen(dest) - 1] == '/'){
        dest[strlen(dest) - 1] = '\0';
    }

    if(word[0] != '/'){
        strcat(dest, "/");
    }
    strcat(dest, word);
}

void write_function(void* arg){
    thread_data_write* td = (thread_data_write*)arg;
    struct stat statbuf;

    if(stat(td->dir, &statbuf) && S_ISDIR(statbuf.st_mode)){
        printf("[WRITE] la cartella è gia presente\n");
    }else{
        mkdir(td->dir, 0755);
        printf("[WRITER] la cartella è stata creata\n");
    }

    char temp_path[BUFFER_SIZE];
    while(1){
        if(pthread_mutex_lock(&td->sh->lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }

        while(td->sh->size == 0 && !td->sh->done){
            if(pthread_cond_wait(&td->sh->full, &td->sh->lock) != 0){
                perror("pthread_cond_wait");
                exit(EXIT_FAILURE);
            }
        }

        if(td->sh->done && td->sh->stack_ptr == -1){
            if(pthread_mutex_unlock(&td->sh->lock) != 0){
                perror("pthread_mutex_unlock");
                exit(EXIT_FAILURE);
            }
            break;
        }

        record* rec = &td->sh->stack[td->sh->stack_ptr--];
        td->sh->size--;

        path_join(td->dir, basename(rec->filename), temp_path);
    
        int file_id;
        if(rec->offset == 0){
            if((file_id = open(temp_path, O_RDWR | O_CREAT | O_TRUNC, 0666)) == -1){
                printf("[WRITER] errore nella creazione del file '%s'\n", temp_path);
            }else{
                lseek(file_id, rec->file_size - 1, SEEK_SET);
                write(file_id, "", 1);
                close(file_id);
                printf("[WRITER] creazione del file '%s' di dimensione %ld bytes\n", temp_path, rec->file_size);  
            }
        }

        if((file_id = open(temp_path, O_RDWR)) == -1){
            printf("[WRITE] errore nella apertura del file '%s' per la scrittura del blocco di offset %ld di %ld byte\n", temp_path, rec->offset, rec->dim_buffer);
        }else{
            char* file_map;
            if((file_map = mmap(NULL, rec->file_size, PROT_WRITE, MAP_SHARED, file_id, 0)) == MAP_FAILED){
                perror("mmap");
                exit(EXIT_FAILURE);
            }

            strncpy(file_map + rec->offset, rec->buffer, rec->dim_buffer);
            if(msync(file_map, rec->file_size, MS_SYNC) == -1){
                printf("[WRITER] errore nella scrittura del blocco di offset %ld di %ld byte sul file '%s'\n", rec->offset, rec->dim_buffer, temp_path);
            }else{
                printf("[WRITER] scrittura nel blocco di offset %ld di %ld byte sul file '%s'\n", rec->offset, rec->dim_buffer, temp_path);
            }
            munmap(file_map, rec->file_size);
            close(file_id);
        }

        if(pthread_cond_signal(&td->sh->empty) != 0){
            perror("pthread_cond_signal empty");
            exit(EXIT_FAILURE);
        }

        if(pthread_mutex_unlock(&td->sh->lock) != 0){
            perror("pthread_mutex_unlock");
            exit(EXIT_FAILURE);
        }
    }
}

int main(int argc, char** argv){
    if(argc < 2){
        fprintf(stderr, "Usage: %s <file-1> <file-2> <...> <file-n>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int nfile = argc -2;
    shared* sh = init_shared();
    thread_data_write* tw = malloc(sizeof(thread_data_write));
    tw->sh = sh;
    strcpy(tw->dir, argv[argc - 1]);
    if(pthread_create(&tw->tid, NULL, (void*)write_function, tw) != 0){
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    thread_data_reader* tr = malloc(sizeof(thread_data_reader) * nfile);
    for(int i = 0; i < nfile; i++){
        tr[i].sh = sh;
        tr[i].thread_n = i;
        strcpy(tr->filename, argv[i + 1]);
        if(pthread_create(&tr[i].tid, NULL, (void*)reader_function, &tr[i]) != 0){
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }

    for(int i = 0; i < nfile; i++){
        if(pthread_join(tr[i].tid, NULL) != 0){
            perror("pthread_join");
            exit(EXIT_FAILURE);
        }
    }

    if(pthread_join(tw->tid, NULL) != 0){
        perror("pthread_join");
        exit(EXIT_FAILURE);
    }

    if(pthread_mutex_lock(&sh->lock) != 0){
        perror("pthread_mutex_lock");
        exit(EXIT_FAILURE);
    }

    sh->done = 1;

    if(pthread_cond_signal(&sh->full) != 0){
        perror("pthread_cond_signal");
        exit(EXIT_FAILURE);
    }

    if(pthread_mutex_unlock(&sh->lock) != 0){
        perror("pthread_mutex_unlock");
        exit(EXIT_FAILURE);
    }

    free(tw);
    free(tr);
    shared_destroy(sh);
}