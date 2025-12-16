#include <ctype.h> //manipolazione caratteri
#include <stdio.h> 
#include <stdlib.h> //gestione memoria
#include <string.h> 
#include <sys/msg.h> //permette di gestire code di messaggi IPC
#include <sys/wait.h> //Attesa di terminazione dei processi figli 
#include <unistd.h> //funzioni di sistema POSIX
#define BUFFER_SIZE 32 //ogni riga è lunga 32 byte
#define MSG_SIZE sizeof(msg) - sizeof(long)

typedef struct {
    long type; //Tipo del messaggio (obbiglatorio per la coda dei msg)
    char buffer[BUFFER_SIZE]; //Contenuto del messaggio (una stringa di 32 carattero)
    char done; //indica se l'elaborazione è stata completata
} msg;

typedef struct {
    char word[BUFFER_SIZE]; //Contiene una parola o stringa di massimo 32 caratteri
    struct node *next; //Puntatore al prossimo nodo nella lista collegata
} node;

typedef node *list; //inserito per evitare di scrivere a ogni funzione node * insert ecc quindi per renderlo più leggibile

list insert(list l, char *word) {
    node *n = malloc(sizeof(node));  // Alloca memoria per un nuovo nodo. perchè la lista è dinamica
    strncpy(n->word, word, BUFFER_SIZE);  // Copia la stringa `word` nel campo `word` del nodo (massimo 32 caratteri).
    n->next = NULL;  // Il nuovo nodo punta a NULL.

    if (l == NULL)  // Se la lista è vuota:
        return n;    // Il nuovo nodo diventa la lista stessa.

    node *ptr = l;   // Puntatore temporaneo per attraversare la lista.

    while (ptr->next != NULL)   // Itera fino all'ultimo nodo (dove `next == NULL`).
        ptr = (node *)ptr->next;

    ptr->next = (struct node *)n; // Collega l'ultimo nodo al nuovo nodo.

    return l;  // Ritorna la lista aggiornata.
}

char search(list l, char *word) {
    node *ptr = l;  // Puntatore temporaneo per attraversare la lista.

    while (ptr != NULL) {  // Itera attraverso i nodi della lista.
        if (!strcasecmp(ptr->word, word)) // Confronta la parola del nodo con `word` senza distinzione tra maiuscole e minuscole.
            return 1;  // Se trovata, ritorna 1.

        ptr = (node *)ptr->next;   // Passa al nodo successivo.
    }

    return 0;  // Ritorna 0 se la parola non è trovata.
}

void destroy(list l) {
    node *ptr = l;  // Puntatore temporaneo che parte dall'inizio della lista.
    node *tmp;

    while (ptr != NULL) {  // Finché ci sono nodi nella lista:
        tmp = (node *)ptr->next;  // Memorizza il nodo successivo.
        free(ptr);  // Libera la memoria del nodo corrente. fa parte della libreria <stdlib.h>
        ptr = tmp;  // Passa al nodo successivo.
    }
}

void print(list l) {
    node *ptr = l;  // Puntatore temporaneo per attraversare la lista.

    while (ptr != NULL) {  // Itera attraverso i nodi della lista.
        printf("%s\n", ptr->word); // Stampa il campo `word` del nodo corrente.
        ptr = (node *)ptr->next;  // Passa al nodo successivo.
    }
}

//path è il percorso del file input da leggere
void reader_child(int queue, char *path) {
    FILE *f; //puntatore al file assegnato in input
    msg m; //creazione di un elemento messagge
    m.done = 0; //impostiamo il valore di done a zero perchè deve iniziare a lavorare
    m.type = 1; //indica un messaggio standard 

 //controlla se il file f è uguale all'apertura al file di path, in modalità di lettura 'read' è uguale a NULL 
    if ((f = fopen(path, "r")) == NULL) { 
        perror("fopen"); //ERRORE
        exit(1);
    }
//scorriamo il file riga per riga e tramite la funzione fgets leggiamo una riga dal file e la memorizziamo in m.buffer, di dimensione BUFFER_SIZE 
    while (fgets(m.buffer, BUFFER_SIZE, f)) { //fgets(char* string, int n, FILE* f)
        if (isspace(m.buffer[0])) //controlliamo se vi sono dei caratterri di whitespace (spazio)
            memmove(m.buffer, m.buffer + 1, strlen(m.buffer)); //Sposta la stringa di un carattere a sinistra, eliminando lo spazio iniziale.

        if (isspace(m.buffer[strlen(m.buffer) - 1])) //controllo se ci sono spazi nell'ultimo carattere della stringa
            m.buffer[strlen(m.buffer) - 1] = '\0'; //se ci sono impostare l'ultimo carattere della stringa con il carattere di terminazione 

        if (msgsnd(queue, &m, MSG_SIZE, 0) == -1)  { //funzione che permette di inviare i messaggi  alla coda dei messaggi specificata
//int msgsnd(int msgid, const void *msgp, size_t msgsz, int msgflg) questa funzione prende come parametri, la coda dei messaggi, il puntatore alla struttura del messaggio, la dimenzione e l'operazione (0 operazione bloccante)
            perror("msgsnd");
            exit(1);
        }
    }

    m.done = 1; //diciamo che il lavoro è finito e inviamo i messaggi
    if (msgsnd(queue, &m, MSG_SIZE, 0) == -1) {
        perror("msgsnd");
        exit(1);
    }

    fclose(f); //chiudiamo il file
    exit(0);
}

void writer_child(int piped) {
    char buffer[BUFFER_SIZE]; //creiamo un array dove veranno scritte le parole
    FILE *f;

//controlla se il file f è uguale all'apertura al file di path, in modalità di lettura 'read' è uguale a NULL
    if ((f = fdopen(piped, "r")) == NULL) {
        perror("fdopen");
        exit(1);
    }

    while (fgets(buffer, BUFFER_SIZE, f)) //leggiamo linea per linea le parole del file
        printf("%s", buffer); //e le scriviamo

    exit(0);
}

int main(int argc, char **argv) {
    if (argc != 3) { //Controllo del numero di argomenti 
        fprintf(stderr, "Usage: %s <file-1> <file-2>\n", argv[0]);
        exit(1);
    }

    int queue;
    int pipe_fds[2]; //array per i file 1 e file 2
    msg m;
    char done_counter = 0; //conta i processi di lettura terminaati
    list l;
    FILE *f;
//mssget è una funzione che serve a ottenere la coda dei messaggi.
//int msgget(key_t key, int msgflg) prende come parametri come primo elemento IPC_PRIVATE (SEMPRE), e il secondo dice l'operazione di creazione della coda dei messaggi con il codice | 0600
    if ((queue = msgget(IPC_PRIVATE, IPC_CREAT | 0600)) == -1) {
        perror("msgget");
        exit(1);
    }

    // creazione processo Reader 1 
    if (!fork())
        reader_child(queue, argv[1]);

    // creazione processo Reader 2
    if (!fork())
        reader_child(queue, argv[2]);
//la pipe è una system call è una connessione tra due processi, tale che l'output standard di un processo diventi l'input standard dell'altro processo.
//creare una pipe e posizionare due descrittori di file, uno ciascuno negli 
//argomenti fildes [0] e fildes [1], che fanno riferimento alle descrizioni del/i file aperto/i per le estremità di lettura fildes[0] e scrittura fildes[1] della pipe.
    if (pipe(pipe_fds) == -1) {
        perror("pipe");
        exit(1);
    }

    // creazione processo di scrittura Writer
    if (!fork()) {
        close(pipe_fds[1]); //Chiudiamo la lettura  
        writer_child(pipe_fds[0]);  
    }

    close(pipe_fds[0]); //chiudiamo la scrittura

    while (1) {
//controlliamo se il messaggio è stato ricevuto grazie alla funzione msgrcv
//int msgrcv(int msgid, void *msgp, size_t msgsz, long int msgtyp, int msgflg) ha come paraemtri la coda, il puntatore al messaggio, la dimenzione del messaggio, 
//il primo zero vuol dire che riceve il primo messaggio della coda, > 0 viene ricevuto il primo messaggio di tipo msgtyp, < 0 riceve il messaggio di tipo più basso
//secondo zero tipo di messaggio che vuole ricevere
        if (msgrcv(queue, &m, MSG_SIZE, 0, 0) == -1) {
            perror("msgrcv");
            exit(1);
        }

        if (m.done) { //se i messaggi sono finiti
            done_counter++; //aumentiamo il counter di conclusione 

            if (done_counter > 1) //se il counter è 1 interrompo 
                break;
            else
                continue;
        }

        if (!search(l, m.buffer)) { // Controlla se il messaggio è già nella lista
            l = insert(l, m.buffer); // Inserisce il messaggio nella lista se non c'è
            dprintf(pipe_fds[1], "%s\n", m.buffer); // Scrive il messaggio sul pipe
        }
    }

    destroy(l); //distruggiamo la lista
    msgctl(queue, IPC_RMID, NULL); //funzione che permette di effettuare operazioni di controllo dei messaggi
//in questo caso la funzione int msgctl(int msgid, int cmd, struct msqid_ds *buf) prende come parametri, la coda, il cmd è IPC_RMID che rimuove l'identificatore della coda dei messaggi
    close(pipe_fds[1]); //chiude la scrittura della pipe
}