#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <libgen.h>

#define PATH_MAX_SIZE 100
#define BUFFER_SIZE 1024
#define STACK_CAPACITY 10

typedef struct{
    char buffer[BUFFER_SIZE];
    char filename[PATH_MAX_SIZE];
    long file_size;
    long offset;
    int buffer_size;
    int end_of_file;
}record_t;

typedef struct{
    record_t stack[STACK_CAPACITY];
    int stack_ptr;
    int exit;

    pthread_mutex_t lock;
    sem_t empty, full;
}shared;

typedef struct{
    char filename[PATH_MAX_SIZE];
    pthread_t tid;
    unsigned thread_n;
    shared* sh;
}thread_data_reader;

typedef struct{
    char dest_folder[PATH_MAX_SIZE];
    unsigned thread_n;
    shared* sh;
}thread_data_writer;

shared* init_shared(){
    shared* sh = malloc(sizeof(shared));
    sh->stack_ptr = -1;
    sh->exit = 0;

    if(pthread_mutex_init(&sh->lock, NULL) != 0){
        perror("pthread_mutex_init");
        exit(EXIT_FAILURE);
    }

    if(sem_init(&sh->full, 0, 0) != 0){
        perror("sem_init");
        exit(EXIT_FAILURE);
    }

    if(sem_init(&sh->empty, 0, STACK_CAPACITY) != 0){
        perror("sem_init");
        exit(EXIT_FAILURE);
    } 
    return sh;
}

void share_destroy(shared* sh){
    pthread_mutex_destroy(&sh->lock);
    sem_destroy(&sh->full);
    sem_destroy(&sh->empty);
    free(sh);
}


void path_join(char* dir, char* name, char* dest){
    strcpy(dest, dir);
    if(dest[strlen(dest) - 1] == '/'){
        dest[strlen(dest)  - 1] = '\0';
    }
    if(name[0] != '/'){
        strcat(dest, "/");
    }
    strcat(dest, name);
}

void reader_thread(void* arg){
    thread_data_reader* td = (thread_data_reader*)arg;
    
    int file_id;
    if((file_id = open(td->filename, O_RDONLY)) < 0){
        printf("[READER-%u] impossibile leggere il file '%s'\n", td->thread_n + 1, td->filename);
        return NULL;
    }

    struct stat stat_file;
    if(fstat(file_id, &stat_file) < 0){
        perror("fstat");
        exit(EXIT_FAILURE);
    }
    printf("[READER-%d] lettura del file '%s' di %ld byte\n", td->thread_n + 1, td->filename, stat_file.st_size);

    char* file_map;
    if((file_map = mmap(NULL, stat_file.st_size, PROT_READ, MAP_SHARED, file_id, 0)) == MAP_FAILED){
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    long offset = 0;
    while(offset < stat_file.st_size){
        if(sem_wait(&td->sh->empty) != 0){
            perror("sem_wait");
            exit(EXIT_FAILURE);
        }


        if(pthread_mutex_lock(&td->sh->lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }

        record_t* record = &td->sh->stack[++td->sh->stack_ptr];

        int len = 0;
        if(offset + BUFFER_SIZE > stat_file.st_size){
            len = stat_file.st_size - offset;
            record->end_of_file = 1;
        }else{
            len = BUFFER_SIZE;
            record->end_of_file = 0;
        }

        record->buffer_size = len;
        record->file_size = stat_file.st_size;
        record->offset = offset;
        strcpy(record->filename, td->filename);
        strncpy(record->buffer, file_map + offset, len);
        printf("[READE-%d] lettura del blocco di offset %ld di byte %ld\n", td->thread_n +1, record->offset, len);
    
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

    munmap(file_map, stat_file.st_size);
    close(file_id);

    printf("[READER-%u] lettura del file '%s' completata\n", td->thread_n + 1, td->filename);
}

void writer_thread(void* arg){
    thread_data_writer* td = (thread_data_writer*)arg;
    struct stat folder_stat;
    //verifico o creo la cartella di destinazione 
    if(stat(td->dest_folder, &folder_stat) && S_ISDIR(folder_stat.st_mode)){ //controllo se la cartella esiste ed è una directory
        printf("[WRITER] la carte è già presente\n");
    }else{
        mkdir(td->dest_folder, 0755); //altirmenti la creo con mkdir
        printf("[WRITER] la cartella è stata creata\n")
    }

    char temp_path[PATH_MAX_SIZE];
    while(1){
        if(sem_wait(&td->sh->full) != 0){
            perror("Sem_wait");
            exit(EXIT_FAILURE);
        }

        if(pthread_mutex_lock(&td->sh->lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }

        if(td->sh->exit != 0 && td->sh->stack_ptr < 0){
            break;
        }

        record_t* record = &td->sh->stack[td->sh->stack_ptr--]; //estraggo il record dallo stack 

        path_join(td->dest_folder, basename(record->filename), temp_path);

        int file_id;
        if(record->offset == 0){ //creazione o troncamento del file duplicato
            if((file_id = open(temp_path, O_RDWR | O_CREAT | O_TRUNC, 0666)) < 0){
                printf("[WRITER] errore nella creazione del file '%s'\n", temp_path);
            }else{
                lseek(file_id, record->file_size - 1, SEEK_SET);
                write(file_id, "", 1);
                close(file_id);
                printf("[WRITER] creazione del file '%s' di dimensione %ld bytes\n", temp_path, record->file_size);
            }   
        }
        
        if((file_id = open(temp_path, O_RDWR)) < 0){
            printf("[WRITER] errore nella apertura del file '%s' per la scrittura del blocco di offset %ld di %d byte\n", temp_path, record->offset, record->buffer_size);
        }else{ //anche la scrittura come la lettura deve essere mappata in memoria 
            char* file_map;
            if((file_map = mmap(NULL, record->file_size, PROT_WRITE, MAP_SHARED, file_id, 0)) == MAP_FAILED){
                perror("mmap");
                exit(EXIT_FAILURE);
            }

            strncpy(file_map + record->offset, record->buffer, record->buffer_size); //copio il blocco di dati nella posizione corretta del file mappato
            if(mysnc(file_map, record->file_size, MS_SYNC) < 0){ //chiamo mysnc per sincronizzare la mappatura
                printf("[WRITER] errore nella scrittura nel blocco di offset %ld di %d byte sul file '%s'\n", record->offset, record->buffer_size, temp_path);
            }else{
                printf("[WRITER] scrittura nel blocco di offset %ld di %d byte sul file '%s'\n", record->offset, record->buffer_size, temp_path);
            }
            
            munmap(file_map, record->file_size);
            close(file_id);
        }
        
        if(pthread_mutex_unlock(&td->sh->lock) != 0){
            perror("Pthread_mutex_unlock");
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
        fprintf(stderr, "Usage: %s <file-1> <file-2> <...> <file-n> <destination-dir>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int n_files = argc - 2;
    printf("[MAIN] duplicazione di %d file\n", n_files);

    shared* sh = init_shared();
    pthread_t writer_thread_id;
    thread_data_writer* writer = malloc(sizeof(thread_data_writer));
    writer->sh = sh;
    strcpy(writer->dest_folder, argv[argc - 1]);
    if(pthread_create(&writer_thread_id, NULL, (void*)writer_thread, writer) != 0){
        perror("pthread_creaete");
        exit(EXIT_FAILURE);
    }

    pthread_t* reader_thread_ids = (pthread_t*)malloc(sizeof(pthread_t) * n_files);
    thread_data_reader** readers = (thread_data_reader** )malloc(sizeof(thread_data_reader*) * n_files);
    for(int i = 0; i < n_files; i++){
        thread_data_reader* reader = malloc(sizeof(thread_data_reader));
        reader->sh = sh;
        reader->thread_n = i;
        strcpy(reader->filename, argv[i + 1]);
        if(pthread_creaete(&reader_thread_ids[i], NULL, (void*)reader_thread, reader) != 0){
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }

        printf("Thread creato %s\n", reader->filename);

        readers[i] = reader;
    }

    for(int i = 0; i < n_files; i++){
        pthread_join(reader_thread_ids[i], NULL);
        free(readers[i]);
    }
    free(readers);

    sh->exit = 1;
    if(sem_post(&sh->full) != 0){
        perror("sem_post");
        exit(EXIT_FAILURE);
    }

    if(pthread_join(writer_thread_id, NULL) != 0){
        perror("pthread_join");
        exit(EXIT_FAILURE);
    }

    free(writer);
    share_destroy(sh);
}