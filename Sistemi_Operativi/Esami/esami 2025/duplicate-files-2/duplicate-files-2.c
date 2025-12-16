#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>

#define BUFFER_SIZE 1024
#define STACK_CAPACITY 10
#define SIZE 1024

typedef struct{
    char buffer[BUFFER_SIZE];
    char filename[PATH_MAX];
    unsigned long file_dim;
    unsigned long offset;
    unsigned long buffer_dim;
    bool done;
}record;

typedef struct{
    record stack[STACK_CAPACITY];
    int stack_ptr;
    int size;
    bool done;
    int ndone;

    pthread_mutex_t lock;
    pthread_cond_t full, empty;
}shared;

typedef struct{
    pthread_t tid;
    unsigned thread_n;
    char* filename;
    int nfile;
    shared* sh;
}thread_reader;

typedef struct{
    pthread_t tid;
    shared* sh;
    char* dir;
}thread_writer;

shared* init_shared(){
    shared* sh = malloc(sizeof(shared));

    sh->ndone = sh->size = 0;
    sh->done = false;
    sh->stack_ptr = -1;
    

    if(pthread_mutex_init(&sh->lock, NULL) != 0){
        perror("pthread_mutex_init");
        exit(EXIT_FAILURE);
    }

    if(pthread_cond_init(&sh->full, NULL) != 0){
        perror("Pthread_cond_init");
        exit(EXIT_FAILURE);
    }

    if(pthread_cond_init(&sh->empty, NULL) != 0){
        perror("pthread_Cond_init");
        exit(EXIT_FAILURE);
    }

    return sh;
}

void shared_destroy(shared* sh){
    pthread_mutex_destroy(&sh->lock);
    pthread_cond_destroy(&sh->full);
    pthread_cond_destroy(&sh->empty);
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

    printf("[READER-%u] lettura del file '%s' di %llu byte\n", td->thread_n + 1, td->filename,statbuf.st_size);

    unsigned long offset = 0;
    while(offset < statbuf.st_size){
        if(pthread_mutex_lock(&td->sh->lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE); 
        }

        while(td->sh->size == STACK_CAPACITY){
            if(pthread_cond_wait(&td->sh->empty, &td->sh->lock) != 0){
                perror("pthread_cond_wait");
                exit(EXIT_FAILURE);
            }
        }

        record* rec = &td->sh->stack[++td->sh->stack_ptr];
        td->sh->size++;

        int len = 0;
        if(offset + BUFFER_SIZE > statbuf.st_size){
            len = statbuf.st_size - offset;
            rec->done = true;
        }else{
            len = BUFFER_SIZE;
            rec->done = false;  
        }

        if(lseek(file_id, offset, SEEK_SET) == -1){
            perror("lseek");
            exit(EXIT_FAILURE);
        }

        unsigned long nbytes_read = read(file_id, rec->buffer, len);

        if(nbytes_read == -1){
            perror("read");
            exit(EXIT_FAILURE);
        } 

        strcpy(rec->filename, td->filename);
        rec->file_dim = statbuf.st_size;
        rec->offset = offset;
        rec->buffer_dim = nbytes_read;
        printf("[READER-%d] lettura del blocco di offset %ld di  %llu byte\n", td->thread_n+  1, rec->offset, nbytes_read);

        if(pthread_cond_signal(&td->sh->full) != 0){
            perror("pthread_cond_signal");
            exit(EXIT_FAILURE);
        }

        if(pthread_mutex_unlock(&td->sh->lock) != 0){
            perror("pthread_mutex_unlock");
            exit(EXIT_FAILURE);
        }

        offset += nbytes_read;
    }
    close(file_id);
    printf("[READER-%u] lettura del file '%s' completata\n", td->thread_n + 1, td->filename);

    if(pthread_mutex_lock(&td->sh->lock) != 0){
        perror("pthread_mutex_lock");
        exit(EXIT_FAILURE);
    }
    
    td->sh->ndone++;
    
    if(td->sh->ndone == td->nfile){
        td->sh->done = true;

        if(pthread_cond_signal(&td->sh->full) != 0){
            perror("pthread_cond_signal");
            exit(EXIT_FAILURE);
        }
    }

    if(pthread_mutex_unlock(&td->sh->lock) != 0){
        perror("pthread_mutex_unlock");
        exit(EXIT_FAILURE);
    }
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
    thread_writer* td = (thread_writer*)arg;
    struct stat statbuf;

    if(stat(td->dir, &statbuf) && S_ISDIR(statbuf.st_mode)){
        printf("[WRITER] la cartella è già presente\n");
    }else{
        mkdir(td->dir, 0755);
        printf("[WRITER] la cartella è stata creata\n");
    }

    char temp_path[SIZE];
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

        if(td->sh->done && td->sh->size == 0){
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
            if((file_id = open(temp_path, O_RDWR | O_CREAT | O_EXCL, 0666)) == -1){
                printf("[WRITER] il file duplicato esiste\n");
                continue;
            }else{
                lseek(file_id, rec->file_dim - 1, SEEK_SET);
                write(file_id, "", 1);
                close(file_id);
                printf("[WRITER] creazione del file '%s' di dimensione %ld bytes\n", temp_path, rec->file_dim);  
            }
        }
        
        if((file_id = open(temp_path, O_RDWR)) == - 1){
            perror("open");
            exit(EXIT_FAILURE);
        }else{
            if(lseek(file_id, rec->offset, SEEK_SET) == -1){
                perror("lseek");
                exit(EXIT_FAILURE);
            }

            unsigned long bytes_writen = write(file_id, rec->buffer, rec->buffer_dim);
            if(bytes_writen != rec->buffer_dim){
                perror("write");
                exit(EXIT_FAILURE);
            }else{
                printf("[WRITER] scrittura blocco offset %ld di %d byte suL file '%s'\n", rec->offset, rec->buffer_dim, temp_path);
            }
            close(file_id);
        }

        if(pthread_cond_signal(&td->sh->empty) != 0){
            perror("pthread_cond_signal");
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

    int nfile = argc - 2;
    shared* sh = init_shared();
    thread_writer tw;
    tw.sh = sh;
    tw.dir = argv[argc - 1];
    if(pthread_create(&tw.tid, NULL, (void*)write_function, &tw) != 0){
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    thread_reader* tr = malloc(sizeof(thread_reader) * nfile);
    for(int i = 0; i < nfile; i++){
        tr[i].sh = sh;
        tr[i].thread_n = i;
        tr[i].nfile = nfile;
        tr[i].filename = argv[i + 1];
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

    if(pthread_join(tw.tid, NULL) != 0){
        perror("pthread_join");
        exit(EXIT_FAILURE);
    }

    free(tr);
    shared_destroy(sh);
}