#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <libgen.h>
#include <fcntl.h>

#define STACK_CAPACITY 10
#define BUFFER_SIZE 1024
#define SIZE 100

typedef struct{
    char buffer[BUFFER_SIZE];
    char* filename;
    unsigned long file_dim;
    unsigned long offset;
    unsigned long buffer_dim;
    bool done;
}record;

typedef struct{
    record stack[STACK_CAPACITY];
    int stack_ptr;
    unsigned ndone;
    bool done;

    pthread_mutex_t lock;
    sem_t full, empty;
}shared;

typedef struct{
    char* filename;
    pthread_t tid;
    unsigned thread_n;
    shared* sh;
    int nfile;
}thread_reader;

typedef struct{
    char* dir;
    pthread_t tid;
    shared* sh;
}thread_writer;

shared* init_shared(){
    shared* sh = malloc(sizeof(shared));

    sh->stack_ptr = -1;
    sh->ndone = 0;
    sh->done = false;

    if(pthread_mutex_init(&sh->lock, NULL) != 0){
        perror("pthread_muteX_init");
        exit(EXIT_FAILURE);
    }

    if(sem_init(&sh->empty, 0, STACK_CAPACITY) != 0){
        perror("sem_init");
        exit(EXIT_FAILURE);
    }

    if(sem_init(&sh->full, 0, 0) != 0){
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
    thread_reader* td = (thread_reader*)arg;

    int file_id;
    struct stat statbuf;

    if((file_id = open(td->filename, O_RDONLY)) == -1){
        perror("open");
        exit(EXIT_FAILURE);
    }

    if(fstat(file_id, &statbuf) == -1){
        perror("fstat");
        exit(EXIT_FAILURE);
    }

    printf("[READER-%u] lettura del file '%s' di %llu byte\n", td->thread_n + 1, td->filename, statbuf.st_size);

    char* file_map;
    if((file_map = mmap(NULL, statbuf.st_size, PROT_READ, MAP_SHARED, file_id, 0)) == MAP_FAILED){
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    long long offset = 0;
    while(offset < statbuf.st_size){
        if(sem_wait(&td->sh->empty) != 0){
            perror("sem_wait");
            exit(EXIT_FAILURE);
        }

        if(pthread_mutex_lock(&td->sh->lock) != 0){
            perror("pthraed__mutex_lock");
            exit(EXIT_FAILURE);
        }

        record* rec = td->sh->stack[++td->sh->stack_ptr];

        int len = 0;
        if(offset + BUFFER_SIZE > statbuf.st_size){
            len = statbuf.st_size - offset;
            rec->done = true;
        }else{
            len = BUFFER_SIZE;
            rec->done = false;
        }

        strncpy(rec->buffer, file_map + offset, len);
        strcpy(rec->filename, td->filename);
        rec->file_dim = statbuf.st_size;
        rec->offset = offset;
        rec->buffer_dim = len;
        printf("[READER-%u] lettura del blocco di offset %lld di %lld byte\n", td->thread_n + 1, rec->offset, len);


        if(pthread_mutex_unlock(&td->sh->lock) != 0){
            perror("pthread_mutex_unlock");
            exit(EXIT_FAILURE);
        }

        if(sem_post(&td->sh->full) != 0){
            perror("sem_post");
            exit(EXIT_FAILURE);
        }

        offset+= len;
    }
    munmap(file_map, statbuf.st_size);
    close(file_id);
    printf("[READER-%u] lettura del file '%s' completata\n", td->thread_n + 1, td->filename);

    if(pthread_mutex_lock(&td->sh->lock) != 0){
        perror("pthread_mutex_lock");
        exit(EXIT_FAILURE);
    }

    td->sh->ndone++;
    if(td->sh->ndone == td->nfile){
        td->sh->done = true;
        if(sem_post(&td->sh->full) != 0){
            perror("sem_post");
            exit(EXIT_FAILURE);
        }
    }
    if(pthread_mutex_unlock(&td->sh->lock) != 0){
        perror("pthread_mutex_unlock");
        exit(EXIT_FAILURE);
    }
}

void path_join(char* dir, char* name, char* dest){
    strcpy(dest, dir);
    if(dest[strlen(dest) - 1] == '/'){
        dest[strlen(dest) - 1] = '\0';
    }
    if(name[0] != '/'){
        strcat(dest, "/");
    }
    strcat(dest,name);
}

void writer_function(void* arg){
    thread_writer* td = (thread_writer*)arg;
    struct stat statbuf;

    if(stat(td->dir, &statbuf) && S_ISDIR(statbuf.st_mode)){
        printf("[WRITER] la cartella è gia presente\n");
    }else{
        mkdir(td->dir, 0755);
        printf("[WRITER] creo la cartella\n");
    }

    char path[SIZE];
    while(1){
        if(sem_wait(&td->sh->full) != 0){
            perror("sem_wait");
            exit(EXIT_FAILURE);
        }

        if(pthread_mutex_lock(&td->sh->lock) != 0){
            perrro("pthraed_mutex_lock");
            exit(EXIT_FAILURE);
        }

        if(td->sh->done && td->sh->stack_ptr == -1){
            if(pthread_mutex_unlock(&td->sh->lock) != 0){
                perror("pthread_mutex_unlock");
                exit(EXIT_FAILURE);
            }
            break;
        }

        record* rec = td->sh->stack[td->sh->stack_ptr--];

        path_join(td->dir, basename(rec->filename), path);
        int file_id;
        if(rec->offset == 0){
            if((file_id = open(path, O_RDWR | O_CREAT | O_TRUNC, 0666)) == -1){
                printf("[WRITER] errore nella creazione del file '%s'\n", path);
            }else{
                lseek(file_id, rec->file_dim - 1, SEEK_SET);
                write(file_id, "", 1);
                close(file_id);
                printf("[WRITER] creazione del file '%s' di dimensione %llu byte\n", path, rec->file_dim);
            }
        }

        if((file_id = open(path, O_RDWR)) == -1){
            printf("[WRITE] errore nella apertura del file '%s' per la scrittura del blocco di offset %ld di %ld byte\n", path, rec->offset, rec->buffer_size);
        }else{
            char* file_map;
            if((file_map = mmap(NULL, rec->file_dim, PROT_WRITE, MAP_SHARED, file_id, 0)) == MAP_FAILED){
                perror("mmap");
                exit(EXIT_FAILURE);
            }

            strncpy(file_map + rec->offset, rec->buffer, rec->buffer_dim);
            if(mysnc(file_map, rec->file_dim, MS_SYNC) == -1){
                printf("[WRITER] errore nella scrittura nel blocco di offset %ld di %ld byte sul file '%s'\n", rec->offset, rec->buffer_size, path);
            }else{
                printf("[WRITER] scrittura nel blocco di offset %ld di %ld byte sul file '%s'\n", rec->offset, rec->buffer_size, path);
            }

            munmap(file_map, rec->file_dim);
            close(file_id);
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
    if(argc < 4){
        fprintf(stderr, "Usage: %s <file-1> <file-2> <...> <file-n> <destination-dir>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int nfile = argc -2;
    printf("[MAIN] duplicazione di %d file\n", nfile);
    shared* sh = init_shared();
    thread_reader* read = malloc(sizeof(thread_reader) * nfile);
    for(int i = 0; i < nfile; i++){
        read[i].thread_n = i;
        read[i].sh = sh;
        read[i].nfile = nfile;
        strcpy(read[i].filename, argv[i + 1]);
        if(pthread_create(&read[i].tid, NULL, (void*)reader_function, &read[i]) != 0){
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }

    thread_writer writer;
    writer.sh = sh;
    if(pthread_create(&writer.tid, NULL, (void*)writer_function, &writer) != 0){
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    for(int i = 0; i <nfile; i++){
        if(pthread_join(read[i].tid, NULL) != 0){
            perror("pthread_join");
            exit(EXIT_FAILURE);
        }
    }

    if(pthread_join(writer.tid, NULL) != 0){
        perror("pthread_join");
        exit(EXIT_FAILURE);
    }
    printf("[MAIN] duplicazione dei %d file completata\n", sh->ndone);
    free(read);
    shared_destroy(sh);
}