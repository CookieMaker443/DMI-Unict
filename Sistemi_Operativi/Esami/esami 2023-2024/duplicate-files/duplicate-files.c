#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <pthread.h>
#include <semaphore.h>

#define PATH_MAX_SIZE 100
#define BUFFER_SIZE 1024
#define STACK_CAPACITY 10

typedef struct{
    char buffer[BUFFER_SIZE];
    char filename[PATH_MAX_SIZE]; 
    long file_size; //dimensione del file totale
    long offset; //posizione del blocco all'interno del file
    int buffer_size; //dimensione del contenuto del buffer
    int end_of_file; //flag di fine lavori
} record_t;

typedef struct{
    record_t stack[STACK_CAPACITY]; //memoria condivisa con gli altri thread
    int stack_ptr; //indice dell’ultimo elemento inserito nello stack
    int exit;

    pthread_mutex_t mutex; // accesso allo stack
    sem_t empty;           // semaforo per bloccare il writer se non c'è nulla nello stack
    sem_t full;            //  semaforo per bloccare il reader se lo stack è pieno

} shared_data_t;

typedef struct{
    int id; //id del thread
    char filename[PATH_MAX_SIZE]; //contiene la directory da analizzare
    shared_data_t *data;
} reader_thread_arg_t;

typedef struct{
    char dest_folder[PATH_MAX_SIZE]; //directory di destinazione
    shared_data_t *data;

} writer_thread_arg_t;

void path_join(char *dir, char *name, char *dest){ //funzione che costruisce un percorso completo contanenado una directoru e un nome di file
    strcpy(dest, dir); //copio la directory in destinazione
    size_t len = strlen(dest);
    if (len > 0 && dest[len - 1] == '/'){ //rimuoviamo il carattere / per evitare duplicazioni
        dest[len - 1] = '\0';
    }
    if (name[0] != '/'){ //se name non inizia con / aggiunge un separatore
        strcat(dest, "/");
    }
    strcat(dest, name); //concateno la stringa name nella destinazione
}

void *reader_thread(void *args){
    reader_thread_arg_t *dt = (reader_thread_arg_t *)args;
    shared_data_t *shared_data = dt->data;

    // apro il file
    int file_id = open(dt->filename, O_RDONLY);
    if (file_id < 0){
        printf("[READER-%d] impossibile leggere il file '%s'\n", dt->id + 1, dt->filename);
        return NULL;
    }

    // recupero le statisitiche per recuperarmi la dimensione
    struct stat stat_file;
    if (fstat(file_id, &stat_file) < 0){
        printf("[READER-%d] fstat error\n", dt->id + 1);
        close(file_id);
        return NULL;
    }
    printf("[READER-%d] lettura del file '%s di %ld byte\n", dt->id + 1, dt->filename, stat_file.st_size);

    // creo la mappatura in memoria
    char *file_map = mmap(NULL, stat_file.st_size, PROT_READ, MAP_SHARED, file_id, 0);
    if (file_map == MAP_FAILED){
        printf("[READER-%d] nmap error\n", dt->id + 1);
        close(file_id);
        return NULL;
    }

    long offset = 0;
    while (offset < stat_file.st_size){
        // STACK PUSH
        sem_wait(&shared_data->empty);
        pthread_mutex_lock(&shared_data->mutex);

        record_t *record = &shared_data->stack[++shared_data->stack_ptr]; //Incrementa il puntatore dello stack (inizialmente -1 diventa 0) e ottiene l’indirizzo in cui salvare il nuovo record.

        // verifico quanti byte leggere e scrivo il blocco nel record
        int len = 0;
        if (offset + BUFFER_SIZE > stat_file.st_size){ //se il blocco da legge  eccede la diemsnione del file
            len = stat_file.st_size - offset; //calcoliamo la dimensione residua
            record->end_of_file = 1; 
        }
        else{ //altrimenti leggo il blocco completo
            len = BUFFER_SIZE;
            record->end_of_file = 0;
        }
        record->buffer_size = len;
        record->file_size = stat_file.st_size;
        record->offset = offset;
        strcpy(record->filename, dt->filename); //copio il nome del file
        strncpy(record->buffer, file_map + offset, len); //copio il contenuto del blocco
        printf("[READER-%d] lettura del blocco di offset %ld di %d byte\n", dt->id + 1, record->offset, len);

        pthread_mutex_unlock(&shared_data->mutex);
        sem_post(&shared_data->full); //sveglio il writer
 
        offset += len; //aggiorno l'offeset per leggere il blocco successivo
    }

    // chiudo il file
    munmap(file_map, stat_file.st_size);
    close(file_id);

    printf("[READER-%d] lettura del file '%s' completata\n", dt->id + 1, dt->filename);

    return NULL;
}

void *writer_thread(void *args){
    writer_thread_arg_t *dt = (writer_thread_arg_t *)args;
    shared_data_t *shared_data = dt->data;

    struct stat folder_stat;
    //utilizzo stat per verificare se la cartella di destinazione esiste e se è una directory
    if (stat(dt->dest_folder, &folder_stat) && S_ISDIR(folder_stat.st_mode)){
        printf("[WRITER] la cartella è gia presente\n");
    }
    else{ //se non esiste viene creata con i permessi utilizzando mkdir
        mkdir(dt->dest_folder, 0755);
        printf("[WRITER] la cartella è stata creata\n");
    }

    char temp_path[PATH_MAX_SIZE];
    while (1){
        // STACK POP
        sem_wait(&shared_data->full);
        pthread_mutex_lock(&shared_data->mutex);
        if (shared_data->exit != 0 && shared_data->stack_ptr < 0){
            break;
        }

        record_t *record = &shared_data->stack[shared_data->stack_ptr--]; //estriamo il record dallo stack

        // get path
        path_join(dt->dest_folder, basename(record->filename), temp_path);
        //path_join permette di costruire il percorso compelto del file duplicato nella cartella di destinazione
        //basename estare solo il nome del file deriva dalla libreria libgen.h


        // se il file non esiste (controllo se devo scrivere il primo blocco) lo creo e
        // riservo il blocco di memoria, se esiste lo apro
        int file_id;
        if (record->offset == 0){ //se il record ha offeset il file duplicato non esiste quindi
            // creo un file con una dimensione pari al file da copiare.
            file_id = open(temp_path, O_RDWR | O_CREAT | O_TRUNC, 0666);
            if (file_id < 0){
                printf("[WRITER] errore nella creazione del file '%s'\n", temp_path);
            }
            else{
                lseek(file_id, record->file_size - 1, SEEK_SET); // sposta l'iffset di lettura/scrittura, il primo parametro è il file aperto , il secondo può essere: 
                //seek_set: offset del file impostato sul offset byte, SEEK_CUR: offset del file deve essere impostato sulla sua posizione corrente, SEEK_END: l'offset del file deve esseere impostato sulla dimensione
                write(file_id, "", 1);
                //la chiamata a lseek e write viene usata per riservare lo spazio nel file imopsstando la dimensione totale corretta
                close(file_id);
                printf("[WRITER] creazione del file '%s' di dimensione %ld byte\n", temp_path, record->file_size);
            }
        }

        file_id = open(temp_path, O_RDWR); //viene aperto il file duplicato in modalità lettura/scrittura
        if (file_id < 0){
            printf("[WRITER] errore nella apertura del file '%s' per la scrittura del blocco di offset %ld di %d byte\n", temp_path, record->offset, record->buffer_size);
        }
        else{
            // scrivo il blocco
            char *file_map = mmap(NULL, record->file_size, PROT_WRITE, MAP_SHARED, file_id, 0); 
            if (file_map == MAP_FAILED){
                printf("[WRITER] nmap error\n");
                close(file_id);
                return NULL;
            }

            strncpy(file_map + record->offset, record->buffer, record->buffer_size); ////il contenuto del blocco viene copiato nel mapping a partire dall'offeset specificato
            if (msync(file_map, record->file_size, MS_SYNC) < 0){ //mysync forza la sincronizzazione del mapping con il file fisico 
                printf("[WRITER] errore nella scrittura nel blocco di offset %ld di %d byte sul file '%s'\n", record->offset, record->buffer_size, temp_path);
            }else{
                printf("[WRITER] scrittura nel blocco di offset %ld di %d byte sul file '%s'\n", record->offset, record->buffer_size, temp_path);
            }

            munmap(file_map, record->file_size);
            close(file_id);
        }

        pthread_mutex_unlock(&shared_data->mutex);
        sem_post(&shared_data->empty);
    }

    return NULL;
}

void main(int argc, char **argv){
    // Controllo gli argomenti
    if (argc <= 2){
        perror("Argomenti non validi");
        exit(EXIT_FAILURE);
    }

    int n_files = argc - 2;
    printf("[MAIN] duplicazione di %d file\n", n_files);

    // creo le strutture dati condivise
    shared_data_t *shared_data = (shared_data_t *)malloc(sizeof(shared_data_t));
    shared_data->stack_ptr = -1;
    shared_data->exit = 0;
    if (pthread_mutex_init(&shared_data->mutex, NULL) < 0){
        perror("Errore nella creazione del mutex");
        exit(EXIT_FAILURE);
    }
    if (sem_init(&shared_data->empty, 0, STACK_CAPACITY) < 0 || sem_init(&shared_data->full, 0, 0) < 0){
        perror("Errore nella creazione del sem");
        exit(EXIT_FAILURE);
    }

    // creo il writer
    pthread_t writer_thread_id;
    writer_thread_arg_t *writer = (writer_thread_arg_t *)malloc(sizeof(writer_thread_arg_t));
    writer->data = shared_data;
    strcpy(writer->dest_folder, argv[argc - 1]);
    if (pthread_create(&writer_thread_id, NULL, writer_thread, writer) < 0){
        perror("Errore nella creazione del thread");
        exit(EXIT_FAILURE);
    }

    // creo i readers
    pthread_t *reader_thread_ids = (pthread_t *)malloc(sizeof(pthread_t) * n_files);
    reader_thread_arg_t **readers = (reader_thread_arg_t **)malloc(sizeof(reader_thread_arg_t *) * n_files);
    for (int i = 0; i < n_files; i++){
        reader_thread_arg_t *reader = (reader_thread_arg_t *)malloc(sizeof(reader_thread_arg_t));
        reader->data = shared_data;
        reader->id = i;
        strcpy(reader->filename, argv[i + 1]);
        if (pthread_create(&reader_thread_ids[i], NULL, reader_thread, reader) < 0){
            perror("Errore nella creazione del thread");
            exit(EXIT_FAILURE);
        }
        printf("Thread creato %s\n", reader->filename);

        readers[i] = reader;
    }

    // aspetto che i thread terminano
    for (int i = 0; i < n_files; i++){
        pthread_join(reader_thread_ids[i], NULL);
        free(readers[i]);
    }
    free(readers);

    shared_data->exit = 1;
    sem_post(&shared_data->full);
    pthread_join(writer_thread_id, NULL);
    free(writer);

    pthread_mutex_destroy(&shared_data->mutex);
    sem_destroy(&shared_data->empty);
    sem_destroy(&shared_data->full);
    free(shared_data);
}