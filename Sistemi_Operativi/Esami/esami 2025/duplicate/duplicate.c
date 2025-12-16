#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <libgen.h>

#define BUFFER_SIZE 1024 //dimensione buffer del blocco dei file nei record
#define STACK_CAPACITY 10 //numero di record presenti nello stack
#define SIZE 100 

typedef struct{
    char buffer[BUFFER_SIZE];
    char* filename;
    long file_size;
    long offset;
    long buffer_size; 
    int exit;
}record;

typedef struct{
    record stack[STACK_CAPACITY];
    int stack_ptr; //questa è la testa che all'inizio è vuota
    int exit;

    pthread_mutex_t lock;
    sem_t empty, full; 
}shared;

typedef struct{
    char filename[SIZE];
    unsigned thread_n;
    pthread_t tid;
    shared* sh;
}thread_data_read;

typedef struct{
    char dir[SIZE]; //directory di destinazione
    pthread_t tid;
    shared* sh;
}thread_data_write;


shared* init_shared(){
    shared* sh = malloc(sizeof(shared));

    sh->stack_ptr = -1;
    sh->exit = 0;

    if(pthread_mutex_init(&sh->lock, NULL) != 0){
        perror("pthread_mutex_init");
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
    sem_destroy(&sh->empty);
    sem_destroy(&sh->full);
    free(sh);
}

void reader_thread(void* arg){
    thread_data_read* td = (thread_data_read*)arg;
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

    char* file_map; //variabile per mappare in memoria
    if((file_map = mmap(NULL, statbuf.st_size, PROT_READ, MAP_SHARED, file, 0)) == MAP_FAILED){
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    long offset = 0;
    while(offset < statbuf.st_size){
        if(sem_wait(&td->sh->empty) != 0){
            perror("sem_wait");
            exit(EXIT_FAILURE);
        }

        if(pthread_mutex_lock(&td->sh->lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }

        record* rec = &td->sh->stack[++td->sh->stack_ptr]; // creo un record che sarà uguale al primo elemeneto dello stack (sto facendo tipo inserimento)

        int len = 0;
        if(offset + BUFFER_SIZE > statbuf.st_size){ //calcolo la dimensione del buffer
            len = statbuf.st_size - offset; 
            rec->exit = 1;
        }else{
            len = BUFFER_SIZE;
            rec->exit = 0;
        }

        rec->buffer_size = len;
        rec->file_size = statbuf.st_size;
        rec->offset = offset;
        strcpy(rec->filename, td->filename);
        strncpy(rec->buffer, file_map + offset, len); //copio il contenuto del blocco
        printf("[READER-%d] lettura del blocco di offset %ld di byte %d\n", td->thread_n+  1, rec->offset, len);

        if(pthread_mutex_unlock(&td->sh->lock) != 0){
            perror("pthread_mutex_unlock");
            exit(EXIT_FAILURE);
        }

        if(sem_post(&td->sh->full) != 0){
            perror("sem_post");
            exit(EXIT_FAILURE);
        }

        offset += len;
    }

    munmap(file_map, statbuf.st_size);
    close(file);

    printf("[READER-%u] lettura del file '%s' completata\n", td->thread_n + 1, td->filename);
}

void path_join(char* dir, char* name, char* dest){
    strcpy(dest, dir); //copio la directory nella destinazione 
    if(dest[strlen(dest) - 1] == '/'){ //se finisce con /
        dest[strlen(dest) - 1] = '\0'; //lo rimuovo per evitare altre aperture
    }
    if(name[0] != '/'){ //se name non inizia con un separatone viene aggiunto 
        strcat(dest, "/");  //es: /usr/share/dict dopo strcat /usr/share/dict/
    }
    strcat(dest, name); //e poi inserisco il nome.
}

void write_thread(void* arg){
    thread_data_write* td = (thread_data_write*)arg;
    struct stat statbuf;

    if(stat(td->dir, &statbuf) && S_ISDIR(statbuf.st_mode)){
        printf("[WRITER] la cartella è già presente\n");
    }else{
        mkdir(td->dir, 0755);
        printf("[WRITER] la cartella è stata creata\n");
    }

    char temp_path[SIZE];
    while (1){
        if(sem_wait(&td->sh->full) != 0){
            perror("sem_wait");
            exit(EXIT_FAILURE);
        }

        if(pthread_mutex_lock(&td->sh->lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }

        if(td->sh->exit != 0 && td->sh->stack_ptr == -1){
            break;
        }

        record* rec = &td->sh->stack[td->sh->stack_ptr--];

        path_join(td->dir, basename(rec->filename), temp_path);
        int file_id;
        if(rec->offset == 0){
            if((file_id = open(temp_path, O_RDWR | O_CREAT | O_TRUNC, 0666)) == -1){
                printf("[WRITER] errore nella creazione del file '%s'\n", temp_path);
            }else{
                lseek(file_id, rec->file_size - 1, SEEK_SET);
                write(file_id, "", 1);
                close(file_id);
                printf("[WRITE] creazione del file '%s' di dimesnione di %ld bytes\n", temp_path, rec->file_size);
            }
        }

        if((file_id = open(temp_path, O_RDWR)) == -1){
            printf("[WRITE] errore nella apertura del file '%s' per la scrittura del blocco di offset %ld di %ld byte\n", temp_path, rec->offset, rec->buffer_size);
        }else{
            char* file_map;
            if((file_map = mmap(NULL, rec->file_size, PROT_WRITE, MAP_SHARED, file_id, 0)) == MAP_FAILED){
                perror("mmap");
                exit(EXIT_FAILURE);
            }

            strncpy(file_map + rec->offset ,rec->buffer, rec->buffer_size);
            if(msync(file_map, rec->file_size, MS_SYNC) == -1){
                printf("[WRITER] errore nella scrittura nel blocco di offset %ld di %ld byte sul file '%s'\n", rec->offset, rec->buffer_size, temp_path);
            }else{
                printf("[WRITER] scrittura nel blocco di offset %ld di %ld byte sul file '%s'\n", rec->offset, rec->buffer_size, temp_path);
            }

            munmap(file_map, rec->file_size);
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
    if(argc <= 2){
        fprintf(stderr, "Usage: %s <file-1> <file-2> <...> <file-n>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int n_files = argc -2;
    shared* sh = init_shared();
    thread_data_write* writer = malloc(sizeof(thread_data_write));
    if(pthread_create(&writer->tid, NULL, (void*)write_thread, writer) != 0){
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    thread_data_read* read = malloc(sizeof(thread_data_read) * n_files);
    for(int i = 0; i < n_files; i++){
        read[i].thread_n = i;
        read[i].sh = sh;
        strcpy(read->filename, argv[i + 1]);
        if(pthread_create(&read[i].tid, NULL, (void*)reader_thread, &read[i]) != 0){
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