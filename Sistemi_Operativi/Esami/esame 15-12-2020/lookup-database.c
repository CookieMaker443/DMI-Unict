#include <ctype.h> //funzioni per manipolare i caratteri
#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
#include <sys/ipc.h> // funzioni inter process comunication 
#include <sys/sem.h> //funzioni semafori
#include <sys/shm.h> //funzioni memoria condivisa
#include <sys/wait.h> 
#include <unistd.h> //definisce costanti e tipi simbolici vari e dichiara funzioni varie (POSIX). 
#define BUFFER_SIZE 2048

enum SEM_TYPE { S_IN, S_DB, S_OUT }; 
//semafori: S_IN sincronizza i process IN1 e IN2, S_DB controlla l'accesso alla memoria condivisa da parte del DB, S_OUT comunicazione tra DB e S_OUT

typedef struct {
  char key[BUFFER_SIZE]; //valore da cercare
  char id; //id del processo che ha inviato la query
  char done; //completato
} shm_query; //query condivisa

typedef struct {
  char key[BUFFER_SIZE]; //nome
  int value; //valore associato al nome
} entry; //database elementi

typedef struct {
  entry e; //la coppia trovata nel database (nome, valore)
  char id; //id del processo che ha richiesto la risposta
  char done; //completamento
} shm_out; //risposta che il processo DB invia ad OUT

typedef struct {
  entry *e; //puntatore alla coppia trovata nel database
  struct node *next; //puntatore al nodo sucessivo della lista
} node; //nodo della double linked list
 
typedef node *list; //alias puntatore alla struttura ndoe

list insert(list l, entry *e) {
  node *n = malloc(sizeof(node)); //allocazione dinamica della memoria per un nuovo nodo della lista
  n->e = e; //imposto la entry del nodo con quella passata come argomento
  n->next = NULL; //imposto il nodo successivo a null

  if (l == NULL) //controllo se la lista è vuota
    l = n; //se lo è imposto come testa della lista, n
  else {
    n->next = (struct node *)l; //imposto il nodo successivo al elemento vecchio 
    l = n; //imopsto l al nuovo nodo 
  }

  return l; 
}

entry *search(list l, char *key) {
  node *ptr = l; //puntatore temporaneo

  while (ptr != NULL) { //scorro tutto la lista
    if (!strcmp(ptr->e->key, key)) //confronto la chiave del nodo corrente, con la chiave ricercata
      return ptr->e; //ci restituisce tale chiave

    ptr = (node *)ptr->next; //puntiamo al nodo successivo
  }

  return NULL; //la chiave non è stata trovata
}

void print(list l) {
  node *ptr = l; //puntatore temporaneo

  while (ptr != NULL) {
    printf("Entry -> key: %s, value: %d\n", ptr->e->key, ptr->e->value);
    ptr = (node *)ptr->next; // puntiamo al nodo successivo
  }
}

void destroy(list l) {
  node *ptr = l; //puntatore temporaneo
  node *tmp; //puntatore da eleminare

  while (ptr != NULL) {
    tmp = ptr; //nodo da rimuovere
    ptr = (node *)ptr->next; //impostiamo ptr al nodo successivo
    free(tmp->e); //libera la memoria allocata alla entry del nodo
    free(tmp); //libera la memoria allocata del nodo
  }
}

int WAIT(int sem_id, int sem_num) {
  struct sembuf ops[1] = {{sem_num, -1, 0}}; 
  return semop(sem_id, ops, 1);
}
int SIGNAL(int sem_id, int sem_num) {
  struct sembuf ops[1] = {{sem_num, +1, 0}};
  return semop(sem_id, ops, 1);
}

int *init_shm() { //inizializza due segmenti di memoria condivisa 
  int *shm_des = malloc(sizeof(int) * 2); //alloco dinamicamente un array di due interi 

  if ((shm_des[0] = shmget(IPC_PRIVATE, sizeof(shm_query), IPC_CREAT | 0600)) == -1) { //creazione del primo segmento di memoria condivisa di dimensione shm_query
      perror("shmget");
      exit(1);
    }

  if ((shm_des[1] = shmget(IPC_PRIVATE, sizeof(shm_out), IPC_CREAT | 0600)) == -1) { //creazione del secondo segmento di memoria condivisa di dimensione shm_out
      perror("shmget");
      exit(1);
    }

  return shm_des;
}

int init_sem() { //inizializzazione dei semafori
  int sem_des; //id del semaforo
  if ((sem_des = semget(IPC_PRIVATE, 3, IPC_CREAT | 0600)) == -1) { //Creazione dei semafori utilizzati in questo caso 3 
    perror("semget");
    exit(1);
  }

  if (semctl(sem_des, S_IN, SETVAL, 1) == -1) { //inizializzazione del semaforo S_IN
    perror("semctl SETVAL S_IN");
    exit(1);
  }

  if (semctl(sem_des, S_DB, SETVAL, 0) == -1) { //inizializzazione del semaforo S_DB
    perror("semctl SETVAL S_DB");
    exit(1);
  }

  if (semctl(sem_des, S_OUT, SETVAL, 0) == -1) { //inizializzazione del semaforo S_OUT
    perror("semctl SETVAL S_OUT");
    exit(1);
  }

  return sem_des;
}

//char* path è il percorso del file contenente la query
void in_child(char id, int shm_id, int sem_des, char *path) { //legge query da un file riga per riga e le invia al processo DB tramite la memoria condivisa, utilizzando i semafori per sincronizzazione
  shm_query *data; //puntatore alla struttura della memoria condivisa
  FILE *f; //puntatore al file da cui leggere la query
  char buffer[BUFFER_SIZE]; //buffer temporaneo per memorizzare ogni riga letta dal file
  unsigned counter = 0; //contatore di query inviate
//funzione che collega il segmento di memoria condivisa al processo chiamante o all’identificatore di memoria condivisa
  if ((data = (shm_query *)shmat(shm_id, NULL, 0)) == (shm_query *)-1) {
    perror("shmat");
    exit(1);
  }

  if ((f = fopen(path, "r")) == NULL) { //apertura del file in modalita di lettura
    perror("fopen");
    exit(1);
  }
//char *fgets (char *string, int n, FILE *stream), legge una stringa del file, i parametro sono: 
//dove viene memorizza le stringhe, la dimenzione del buffer e il file che viene aperto
  while (fgets(buffer, BUFFER_SIZE, f)) {
    counter++;
//questo if serve perchè fgets legge una riga e la memorizza nel buffer quando viene letta viene incluso anche il carattere \n (newline) che si trova alla fine
//il carattere \n potrebbe creare dei problemi con varie operazioni (es strcmp) o durante il trasferimento dei dati per questo conviene sostuire il carattere dell'ultima riga con \0
    if (buffer[strlen(buffer) - 1] == '\n')
      buffer[strlen(buffer) - 1] = '\0';

    WAIT(sem_des, S_IN); //ingresso sezione critica del semaforo S_IN
    data->id = id; // Scriviamo l'identificatore del processo se è S_IN1 o S_IN2
    strncpy(data->key, buffer, BUFFER_SIZE); //effettuiamo una copia della query del buffer alla memoria condivisa
    data->done = 0; //impostiamo il completamento a zero per dire che la query non è stata elaborata dal DB
    SIGNAL(sem_des, S_DB); //risvegliamo il S_DB per elaborare la query

    printf("IN%d: inviata query n.%d '%s'\n", id, counter, buffer);
  }
//quando DB finirà risvegliera S_IN
  WAIT(sem_des, S_IN); //utilizziamo la wait per far entrare nella sezione critisa s_in
  data->done = 1; //e dire di aver concluso di inviare la query
  SIGNAL(sem_des, S_DB); //risvegliamo il database

  fclose(f); //chiudiamo il file
  exit(0);
}

entry *create_entry(char *data) { //converte una riga del file in una struttura entry (elemento del database)
  entry *e = malloc(sizeof(entry)); //allocazione di memoria per un oggetto di tipo entry
  char *key; //prende una key e un  valore
  char *value; //value è in char perchè il file database è composto da un unica stringa con key:value
//Se l'ultima riga contiene un newline (\n), lo rimuove, in modo che non interferisca con la successiva elaborazione.
  if (data[strlen(data) - 1] == '\n')
    data[strlen(data) - 1] = '\0';

//questo controllo serve per il file database.txt essendo una combinazione nome:value
//la funzione strtok separata la stringa in base al carattere ":" restituendo la key (la parole)
  if ((key = strtok(data, ":")) != NULL) {
//questo controllo restituisce la parte dopo i ":" che rappresenta il valore numerico    
    if ((value = strtok(NULL, ":")) != NULL) {
      strncpy(e->key, key, BUFFER_SIZE); //copiamo il contenuto di key, nel campo key della struttura entry
      e->value = atoi(value); //converto la stringa value in un numero intero con atoi
      return e; //se key e value sono validi restituisco la entry
    }
  }

  free(e); //altrimenti la libero 
  return NULL;
}
//carica tutte le entry del database in una lista
list load_database(char *path) {
  list l = NULL; //creo una lista vuota
  FILE *f;
  entry *e; //serve a contenere tutte le entry create per ogni riga letta
  char buffer[BUFFER_SIZE]; //array che memorizza la riga letta del file
  unsigned counter = 0; //contatore di righe inserite nella lista

  if ((f = fopen(path, "r")) == NULL) { //apriamo il file in lettura
    perror("fopen");
    exit(1);
  }

//char *fgets (char *string, int n, FILE *stream), legge il file riga per riga
  while (fgets(buffer, BUFFER_SIZE, f)) { //ogni riga viene memorizzata nel buffer, secondo parametro dimensione del buffer, terzo file che viene letto
    e = create_entry(buffer); //per ogni riga letta chiamo la create entry per analizzarla 
    if (e != NULL) { //se la riga del db è valida quindi != NULL 
      l = insert(l, e); //la inserisco nella lista 
      counter++; //aumenta il contatore di righe
    }
  }

  printf("DB: letti n.%d record da file\n", counter);
  fclose(f); //chiudio il file f 
  return l;
}

void db_child(int shm_query_id, int shm_out_id, int sem_id, char *path) {
  shm_query *q_data;
  shm_out *o_data;
  char done_counter = 0;
  list l = load_database(path); //creiamo una lista contenente tutte le entry del database nel file specificato in path
  entry *e;

  if ((q_data = (shm_query *)shmat(shm_query_id, NULL, 0)) == (shm_query *)-1) { //collegamento dei processi IN1 e IN2 alla query 
    perror("shmat");
    exit(1);
  }

  if ((o_data = (shm_out *)shmat(shm_out_id, NULL, 0)) == (shm_out *)-1) { //collegamento dei processi (OUT) alla memoria condivisa out
    perror("shmat");
    exit(1);
  }

  while (1) {
    WAIT(sem_id, S_DB); //entra nella sezione critca il semaforo S_DB

    if (q_data->done) { //Se un processo della query ha terminato
      done_counter++; //aumenta il counter

      if (done_counter > 1) //se il counter è > 1 quindi hanno terminato entrambi i processi esce dal ciclo
        break;
      else {
        SIGNAL(sem_id, S_IN); //risveglia il semaforo S_IN e continua ad ettendere altre query
        continue;
      }
    }
//ricercherà il nome nella struttura del database e, se riscontrato un 
//match (di tipo  case sensitive), allora invierà al processo  OUT 
    e = search(l, q_data->key); //cerchiamo la chiave specificata nella query all'interno della lista che contiene il

    if (e != NULL) { //se l'èlemento è stato trovato
      printf("DB: query '%s' da IN%d trovata con valore %d\n", e->key,
             q_data->id, e->value);
      o_data->e = *e; //copia il risultato entry in o_data 
      o_data->id = q_data->id; //copia l'id del richiedente
      o_data->done = 0; 
      SIGNAL(sem_id, S_OUT); //segnala il process out
    } else {//se la chiave non viene trovata risveglia il semaforo S_IN che continuera a cercare l'elemento nei file specificati
      printf("DB: query '%s' da IN%d non trovata\n", q_data->key, q_data->id);
      SIGNAL(sem_id, S_IN);
    }
  }

  o_data->done = 1; // segnala ad out che non ci sono più processi da elaborare
  SIGNAL(sem_id, S_OUT); //risveglia out

  destroy(l); //distrugge la memoria della lista collegata
  exit(0);
}

void out_child(int shm_id, int sem_id) {
  shm_out *data;
  unsigned record_in1 = 0, record_in2 = 0; //contatore dei record ricevuti da entrambi i processi lettori
  int val1 = 0, val2 = 0; //totale dei valori ricevuti da processo IN1 e totale dei valori ricevuti dal processo IN2

  if ((data = (shm_out *)shmat(shm_id, NULL, 0)) == (shm_out *)-1) {//collegamento dei processi alla memoria condivisa OUT
    perror("shmat");
    exit(1);
  }

  while (1) {
    WAIT(sem_id, S_OUT); //il semaforo S_OUT entra nella sezione critica

    if (data->done) //se ha compleato esce dal if
      break;

    if (data->id == 1) { //se l'id del processo che ha richiesto la risposta della memoria condivisa è = 1 (è IN1)
      record_in1++; //Incrementa il valore del record di IN1 
      val1 += data->e.value; //imposta il valore totale di IN1 += alla somma di tutti i valori dell'entry appartenenti a data
    } else if (data->id == 2) { //se l'id del processo che ha richiesto la risposta della memoria condivisa è = 2 (è IN2)
      record_in2++; //Incrementa il valore del record di IN2
      val2 += data->e.value; //imposta il valore totale di IN2 = alla somma dei valori dei nodi presenti in data
    }

    SIGNAL(sem_id, S_IN); //incrementa i semafori S_IN
  }

  printf("OUT: ricevuti n.%d valori validi di IN1 con totale %d\n", record_in1,
         val1);
  printf("OUT: ricevuti n.%d valori validi di IN2 con totale %d\n", record_in2,
         val2);

  exit(0);
}

int main(int argc, char **argv) {
  if (argc < 4) { //Controlla che il numero di argomenti forniti sia 3
    fprintf(stderr, "Usage: %s <db-file> <queries-file-1> <queries-file-2>\n",
            argv[0]);
    exit(1);
  }

  int *shm_des = init_shm(); //inizializzazione memoria condivisa 
  int sem_des = init_sem(); //inizializzazione semafori

  // creazione processo IN1
  if (!fork()) 
    in_child(1, shm_des[0], sem_des, argv[2]);
  //creazione processo IN2
  if (!fork())
    in_child(2, shm_des[0], sem_des, argv[3]);
  // creazione del processo DB
  if (!fork())
    db_child(shm_des[0], shm_des[1], sem_des, argv[1]);
  // creazione del processo out OUT
  if (!fork())
    out_child(shm_des[1], sem_des);

  for (int i = 0; i < 4; i++)
    wait(NULL); //il processo parent esegue questo clico di attesa per assicurarsi che i processi figli hanno terminato

  shmctl(shm_des[0], IPC_RMID, NULL); //rimozione memoria condivisa della query
  shmctl(shm_des[1], IPC_RMID, NULL); //rimozione memoria condivisa dell'IPC OUT
  free(shm_des); //libera la memoria allcoata da shm_des
  semctl(sem_des, 0, IPC_RMID); //rimuove l'array di semafori
}