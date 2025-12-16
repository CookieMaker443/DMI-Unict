#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <pthread.h>
#include <unistd.h>
#include <semaphore.h>
#include <ctype.h>

#define KEYS_SIZE 30     // lunghezza massima della chiave da decifrare
#define BUFFER_SIZE 100  // dimensione massima del buffer per memorizzare una riga cifrata

typedef struct shared_data {
    char buffer[BUFFER_SIZE];   // memorizza la riga cifrata da decifrare
    sem_t buffer_sem;           // gestisce l'accesso al buffer
    int exit;
} shared_data_t;

typedef struct thread_data {
    char key[KEYS_SIZE];  // memorizza la chiave assegnata al thread
    int index;            // identificatore del thread (0-indexed)
    shared_data_t *data;
    sem_t read_sem;       // blocca il thread finché non c'è una riga da decifrare
    sem_t write_sem;      // blocca il thread principale finché la decifratura non è completa
} thread_data_t;

char *decrypt(char *text, char *keys) { // funzione di decifratura
    char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    int n = strlen(text);  // lunghezza del testo cifrato
    char *dec_text = (char *)malloc(sizeof(char) * (n + 1)); // array che contiene il testo decifrato
    for (int i = 0; i < n; i++) {
        // strchr trova la prima occorrenza di un carattere, restituisce un puntatore a tale char
        char *index_c = strchr(keys, text[i]); //cerco il carattere di text, nella chiave 
        if (index_c != NULL) {
            int index = index_c - keys;
            dec_text[i] = alphabet[index]; // sostituisce il carattere cifrato con quello corrispondente
        } else {
            dec_text[i] = text[i]; // il carattere non è stato trovato, lo lascio invariato
        }
    }
    dec_text[n] = '\0';
    return dec_text;
}

int parse_text(char *text, char *cifar_text, int *index_key) { // estrae indice della chiave e il testo cifrato
    int sep_index = -1;
    int len = strlen(text);
    for (int i = 0; i < len; i++) {
        if (text[i] == ':') {  // cerca il separatore ':'
            sep_index = i;
            break;
        }
    }
    if (sep_index < 0)
        return -1;

    // Alloca spazio per memorizzare l'indice del testo (0,1,2,3) (con terminatore)
    char *index_text = (char *)malloc(sizeof(char) * (sep_index + 1));
    for (int i = 0; i < sep_index; i++) {
        index_text[i] = text[i];
    }
    printf("%s\n", index_text);
    index_text[sep_index] = '\0';

    // Copia il testo cifrato: dal carattere successivo al ':' fino a prima del terminatore di linea
    int j = 0;
    for (int i = sep_index + 1; i < len && text[i] != '\n'; i++, j++) {
        cifar_text[j] = text[i];
    }
    cifar_text[j] = '\0';

    // Converte l'indice; si assume che nel file l'indice sia 1-indexed, quindi si decrementa per ottenere 0-indexed
    *index_key = atoi(index_text) - 1;
    free(index_text);
    return 0;
}

void *thread_function(void *args) {
    thread_data_t *dt = (thread_data_t *)args;
    printf("[K%d] chiave assegnata : %s\n", dt->index + 1, dt->key);
    char buffer[BUFFER_SIZE];
    while (dt->data->exit == 0) {  // continua finché exit è 0
        // blocco il thread in attesa del segnale
        sem_wait(&dt->read_sem);
        if (dt->data->exit != 0)
            break;

        sem_wait(&dt->data->buffer_sem);

        // leggo la stringa cifrata dal buffer condiviso
        strcpy(buffer, dt->data->buffer);

        int d = strlen(buffer);
        printf("[K%d] sto decifando la frase di %d caratteri passata dal main\n", dt->index + 1, d);

        // decifra e copia il risultato nel buffer globale
        char *dec = decrypt(buffer, dt->key);
        strcpy(dt->data->buffer, dec);
        free(dec);

        sem_post(&dt->data->buffer_sem);

        // notifica al main che ha completato
        sem_post(&dt->write_sem);
    }
    return NULL;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        perror("Numero di parametri non valido");
        exit(EXIT_FAILURE);
    }

    char *keys_input_filename = argv[1];
    char *cifar_input_filename = argv[2];
    char *ouput_filename = argc > 3 ? argv[3] : "output.txt";

    printf("[M] Leggo il file delle chiavi\n");
    FILE *key_fp = fopen(keys_input_filename, "r");
    if (key_fp == NULL) {
        perror("File delle chiavi non valido");
        exit(EXIT_FAILURE);
    }

    // Recupera il numero delle chiavi
    int num_keys = 0;
    while (!feof(key_fp)) {
        if (fgetc(key_fp) == '\n')
            num_keys++;
    }
    fseek(key_fp, 0, SEEK_SET);
    printf("[M] trovate %d chiavi, creo i thread k-i necessari\n", num_keys);

    // Inizializza le strutture dati
    shared_data_t *data = (shared_data_t *)malloc(sizeof(shared_data_t));
    data->exit = 0;
    if (sem_init(&data->buffer_sem, 0, 1) < 0) {
        perror("Errore nella creazione del semaforo buffer_sem");
        exit(EXIT_FAILURE);
    }

    thread_data_t **thread_data_array = (thread_data_t **)malloc(sizeof(thread_data_t *) * num_keys);
    int j = 0;
    char key_buffer[KEYS_SIZE];
    while (fgets(key_buffer, KEYS_SIZE, key_fp) != NULL) {
        // Rimuove il newline, se presente
        key_buffer[strcspn(key_buffer, "\n")] = '\0';

        thread_data_t *item = (thread_data_t *)malloc(sizeof(thread_data_t));
        item->data = data;
        item->index = j;
        strcpy(item->key, key_buffer);
        thread_data_array[j] = item;

        // Inizializza i semafori: 0 significa che il thread parte bloccato
        if (sem_init(&item->read_sem, 0, 0) < 0) {
            perror("Errore nella creazione del semaforo read_sem");
            exit(EXIT_FAILURE);
        }
        if (sem_init(&item->write_sem, 0, 0) < 0) {
            perror("Errore nella creazione del semaforo write_sem");
            exit(EXIT_FAILURE);
        }
        j++;
    }
    fclose(key_fp);

    // Creo i thread
    pthread_t *threads = (pthread_t *)malloc(sizeof(pthread_t) * num_keys);
    for (int i = 0; i < num_keys; i++) {
        if (pthread_create(&threads[i], NULL, thread_function, thread_data_array[i]) < 0) {
            perror("Errore nella creazione del thread");
            exit(EXIT_FAILURE);
        }
    }

    printf("[M] Processo il file cifrato\n");
    FILE *cif_fp = fopen(cifar_input_filename, "r");
    if (cif_fp == NULL) {
        perror("Errore nell'apertura del file cifrato");
        exit(EXIT_FAILURE);
    }

    char buffer[BUFFER_SIZE];
    char text[BUFFER_SIZE];
    int key_index;
    while (fgets(buffer, BUFFER_SIZE, cif_fp) != NULL) {
        if (parse_text(buffer, text, &key_index) < 0) {
            perror("File cifrato non valido");
            exit(EXIT_FAILURE);
        }

        // Controllo che l'indice sia valido
        if (key_index < 0 || key_index >= num_keys) {
            fprintf(stderr, "Indice della chiave fuori range: %d\n", key_index + 1);
            exit(EXIT_FAILURE);
        }

        printf("[M] la riga '%s' deve essere decifrata con la chiave n. %d\n", text, key_index + 1);

        sem_wait(&data->buffer_sem);                        // blocco l'accesso al buffer
        strcpy(data->buffer, text);                         // inserisco il testo da decifrare
        sem_post(&data->buffer_sem);                        // libero il buffer

        sem_post(&thread_data_array[key_index]->read_sem);  // risveglio il thread corrispondente
        sem_wait(&thread_data_array[key_index]->write_sem); // attendo che il thread completi

        sem_wait(&data->buffer_sem);
        strcpy(buffer, data->buffer);
        sem_post(&data->buffer_sem);

        printf("[M] la riga e' stata decifrata in %s\n", buffer);
    }
    fclose(cif_fp);

    // Chiusura pulita del programma
    printf("[M] Termino i thread\n");
    data->exit = 1;
    printf("[M] Aspetto che tutti i thread terminano e dealloco tutte le risorse\n");
    for (int i = 0; i < num_keys; i++) {
        sem_post(&thread_data_array[i]->read_sem);
        pthread_join(threads[i], NULL);

        sem_destroy(&thread_data_array[i]->read_sem);
        sem_destroy(&thread_data_array[i]->write_sem);
        free(thread_data_array[i]);
    }
    free(thread_data_array);
    sem_destroy(&data->buffer_sem);
    free(data);
    free(threads);
}
