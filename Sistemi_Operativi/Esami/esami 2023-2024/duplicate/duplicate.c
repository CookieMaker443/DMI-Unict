#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/stat.h>
#include <libgen.h>
#include <dirent.h>
#include <sys/mman.h>

#define BLOCK_SIZE 1024
#define STACK_SIZE 10
#define PATH_SIZE 100

typedef struct{
    char buffer[BLOCK_SIZE];
    char filename[PATH_SIZE];
    long long size;
    long long offset;
    long long dim_buffer;
    int done;
}record;

typedef struct{
    record stack[STACK_SIZE];
    int stack_ptr;
    bool done;

    pthread_mutex_t lock;
    sem_t full, empty;
}shared;

typedef struct{
    pthread_t tid;
    unsigned thread_n;
    char* filename;
    shared* sh;
}thread_data_reader;

typedef struct{
    pthread_t tid;
    char* dir;
    shared* sh;
}thread_data_writer;

shared* init_shared(){
    shared* sh = malloc(sizeof(shared));

    sh->done = 0;
    sh->stack_ptr = -1;

    if(pthread_mutex_init(&sh->lock, NULL) != 0){
        perror("pthread_mutex_init");
        exit(EXIT_FAILURE);
    }

    if(sem_init(&sh->empty, 0, STACK_SIZE) != 0){
        perror("sem_init");
        exit(EXIT_FAILURE);
    }

    if(sem_init(&sh->empty, 0, 0) != 0){
        perror("sem_init");
        exit(EXIT_FAILURE);
    }

    return sh;
}

void shared_destroy(shared* sh){
    pthread_mutex_destroy(&sh->lock);
    sem_destroy(&sh->full);
    sem_destroy(&sh->empty);
    free(sh);
}

void reader_function(void* arg){
    thread_data_reader* td = (thread_data_reader*)arg;

    int file_id;
    struct stat stat_buf;
    char* file_map;

    if((file_id = open(td->filename, O_RDONLY)) != 0){
        perror("open");
        exit(EXIT_FAILURE);
    }

    if(fstat(file_id, &stat_buf) == -1){
        perror("fstat");
        exit(EXIT_FAILURE);
    }

    printf("[READER-%u] lettura del file '%s' di %lld byte\n", td->thread_n, td->filename, stat_buf.st_size);

    if((file_map = mmap(NULL, stat_buf.st_size, PROT_READ, MAP_SHARED, file_id, 0)) == MAP_FAILED){
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    long long offset = 0;
    while(offset < stat_buf.st_size){
        if(sem_wait(&td->sh->empty) != 0){
            perror("sem_wait");
            exit(EXIT_FAILURE);
        }

        if(pthread_mutex_lock(&td->sh->lock) != 0){
            perrro("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }

        record* rec = &td->sh->stack[++td->sh->stack_ptr];

        long long len = 0;
        if(offset + BLOCK_SIZE > stat_buf.st_size){
            len = stat_buf.st_size - offset;
            rec->done = 1;
        }else{
            len = BLOCK_SIZE;
            rec->done = 0;
        }

        strncpy(rec->buffer, file_map + offset, len);
        strcpy(rec->filename, td->filename);
        rec->offset = offset;
        rec->size = stat_buf.st_size;
        rec->dim_buffer = len;
        printf("[READER-%u] lettura del blocco di offset %lld di %lld byte\n", td->thread_n, rec->offset, len);

        if(pthread_mutex_unlock(&td->sh->lock) != 0){
            perror("Pthread_mutex_unlock");
            exit(EXIT_FAILURE);
        }

        if(sem_post(&td->sh->full) != 0){
            perror("sem_post");
            exit(EXIT_FAILURE);
        }

        offset += len;
    }

    munmap(file_map, stat_buf.st_size);
    close(file_id);
    printf("[READER-%u] lettura del file '%s' completata\n", td->thread_n, td->filename);
}

void path_join(char* dir, char* name, char* dest){
    strcpy(dest, dir);
    if(dest[strlen(dest) - 1] == '/'){
        dest[strlen(dest) - 1] = '\0';
    }

    if(name[0] != '/'){
        strcat(dest, "/");
    }
    strcat(dest, name);
}

void writer_function(void* arg){
    thread_data_writer* td = (thread_data_writer*)arg;

    struct stat statbuf;

    if(stat(td->dir, &statbuf) && S_ISDIR(statbuf.st_mode)){
        printf("[WRITER] la cartella è già")
    }else{
        mkdir(td->dir, 0755);
        printf("[WRITER] creo la cartella\n");
    }

    char temp_path[PATH_SIZE];
    while(1){
        if(sem_wait(&td->sh->full) != 0){
            perror("sem_wait");
            exit(EXIT_FAILURE);
        }

        if(pthread_mutex_lock(&td->sh->lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }

        if(td->sh->done && td->sh->stack_ptr < 0){
            break;
        }

        record* rec = &td->sh->stack[td->sh->stack_ptr--];

        path_join(td->dir, basename(rec->filename), temp_path);
    
        int file;
        if(rec->offset == 0){
            if((file = open(temp_path, O_RDWR | O_CREAT | O_TRUNC, 0666)) == -1){
                printf("[WRITER] errore nella creazione del file '%s'\n", temp_path);
            }else{
                lseek(file, rec->size - 1, SEEK_SET);
                write(file, "", 1);
                close(file);
                printf("[WRITER] creazione del file '%s' di dimensione %lld bytes\n", temp_path, rec->size);
            }
        }

        if((file = open(temp_path, O_RDWR)) != 0){
            perror("open");
            exit(EXIT_FAILURE);
        }
        else{
            char* file_map;
            if((file_map = mmap(NULL, rec->size, PROT_WRITE, MAP_SHARED, file, 0)) == MAP_FAILED){
                perror("MMAP");
                exit(EXIT_FAILURE);
            }

            strncpy(file_map + rec->offset, rec->buffer, rec->dim_buffer);
            if(mysnc(file_map, rec->size, MY_SYNC) == -1){
                printf("[WRITER] errore nella scirttura nel blocco di offset %ld di %ld byte sul file '%s'\n", rec->offset, rec->dim_buffer, temp_path);
            }else{
                printf("[WRITER] scrittura nel blocco di offset %ld di %ld byte sul file '%s'\n", rec->offset, rec->dim_buffer, temp_path);
            }
            munmap(file_map, rec->size);
            close(file);
        }   

        if(pthread_mutex_unlock(&td->sh->lock) != 0){
            perror("pthread_mutex_unlock");
            exit(EXIT_FAILURE);
        }
        
        if(sem_post(&td->sh->empty) != 0){
            perror("sem_post");
            exit(EXIT_FAILURE);
        }
    }
}

int main(int argc, char** argv){
    if(argc < 3){
        fprintf(stderr, "Usage: %s <file-1> <file-2> ... <file-n> <destination-dir>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int n_file = argc - 2;
    shared* sh = init_shared();
    thread_data_writer* writer = malloc(sizeof(thread_data_writer));
    if(pthread_create(&writer->tid, NULL, (void*)writer_function, writer) != 0){
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    thread_data_reader* read = malloc(sizeof(thread_data_reader) * n_files);
    for(int i = 0; i < n_files; i++){
        read[i].thread_n = i;
        read[i].sh = sh;
        strcpy(read->filename, argv[i + 1]);
        if(pthread_create(&read[i].tid, NULL, (void*)reader_function, &read[i]) != 0){
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }

    for(int i = 0; i < n_files; i++){
        if(pthread_join(read[i].tid, NULL) != 0){
            perror("pthread_join");
            exit(EXIT_FAILURE);
        }
    }
    free(read);

    sh->exit = 1;
    if(sem_post(&sh->full) != 0){
        perror("sem_post");
        exit(EXIT_FAILURE);
    }

    if(pthread_join(writer->tid, NULL) != 0){
        perror("pthread_join");
        exit(EXIT_FAILURE);
    }

    free(writer);
    shared_destroy(sh);
}