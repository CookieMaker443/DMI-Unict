#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#define IN1 1
#define IN2 2
#define BUFFER_SIZE 1024

typedef struct {
  long type; //tipo
  char key[BUFFER_SIZE]; //valore
  char id; //id del processo che ha inviato la query_msg
  char done; //indica se la coda ha concluso di inviare tutti messaggi
} query_msg; //prima coda dei messaggi

typedef struct {
  char key[BUFFER_SIZE]; //elemento stringa
  int value; //elemento valore
} entry; //elemento del tabase

typedef struct {
  long type; //tipo di messaggio
  char id; //id del processo che ha richiesto la risposta
  char done;  //indica se la coda ha concluso di ricevere tutti i messaggi
  entry e; //elemento del database
} out_msg;

typedef struct {
  entry *e;
  struct node *next;
} node;

typedef node *list;

list insert(list l, entry *e) { //inserimento
  node *n = malloc(sizeof(node)); //creiamo un nuovo nodo
  n->e = e; //assegno il nodo da inserire
  n->next = NULL; //imposto l'elemento successivo a NULL

  if (l == NULL) //se non vi sono elementi nella lista imposto il primo elemento come nodo
    l = n;
  else {
    n->next = (struct node *)l; //imposto l'elemento successivo come il nuovo nodo della lista e pongo l = n
    l = n;
  }

  return l;
}

entry *search(list l, char *key) {
  node *ptr = l; //imposto il nodo all'elemento della lista

  while (ptr != NULL) { //scorro tutti i nodi della lista
    if (!strcmp(ptr->e->key, key)) //se combacia
      return ptr->e; //restitusice il nodo

    ptr = (node *)ptr->next; //puntiamo al nodo successivo per fare scorrere
  }

  return NULL;
}

void print(list l) {
  node *ptr = l;

  while (ptr != NULL) {
    printf("Entry -> key: %s, value: %d\n", ptr->e->key, ptr->e->value);
    ptr = (node *)ptr->next;
  }
}

void destroy(list l) {
  node *ptr = l;
  node *tmp;

  while (ptr != NULL) {
    tmp = ptr;
    ptr = (node *)ptr->next;
    free(tmp->e);
    free(tmp);
  }
}

void in_child(char id, int queue, char *path) {
  FILE *f;
  query_msg msg; //prima coda di messaggi da inviare
  msg.type = 1;
  msg.done = 0;
  msg.id = id;
  unsigned counter = 0; //contatore per vedere qunate righe sono state lette rispettivamente dai file

  if ((f = fopen(path, "r")) == NULL) {
    perror("fopen");
    exit(1);
  }

  while (fgets(msg.key, BUFFER_SIZE, f)) { //fgets legge una riga
    counter++;  
    if (msg.key[strlen(msg.key) - 1] == '\n') //Rimuoviamo il carattere speciale di terminazione riga
      msg.key[strlen(msg.key) - 1] = '\0'; //inserire il carattere NULL

    if (msgsnd(queue, &msg, sizeof(query_msg) - sizeof(long), 0) == -1) {
      perror("msgsnd");
      exit(1);
    }

    printf("IN%d: inviata query n.%d '%s'\n", id, counter, msg.key);
  }

  msg.done = 1;
  if (msgsnd(queue, &msg, sizeof(query_msg) - sizeof(long), 0) == -1) {
    perror("msgsnd");
    exit(1);
  }

  fclose(f);
  exit(0);
}

entry *create_entry(char *data) { //funzione che serve a trasformare una riga del database in un file entry del tipo char nome e int valore
  entry *e = malloc(sizeof(entry));
  char *key;
  char *value;

  if (data[strlen(data) - 1] == '\n') //rimuoviamo il carattere speciale di newline per mettere il carattere NULL
    data[strlen(data) - 1] = '\0';

  if ((key = strtok(data, ":")) != NULL) { //separo l'inizio della stringa fino ai :
    if ((value = strtok(NULL, ":")) != NULL) { //separa la parte dei due punti fino alla fine della riga
      strncpy(e->key, key, BUFFER_SIZE); //copia il contenuto di key
      e->value = atoi(value); //convertiamo in intero
      return e;
    }
  }

  free(e);
  return NULL;
}

list load_database(char *path) { //creo una lista degli elementi del database
  list l = NULL; //Creo una lista vuota per inserire tutte le entry
  FILE *f;
  entry *e;
  char buffer[BUFFER_SIZE]; //array temporaneo per leggere una riga dal file
  unsigned counter = 0;

  if ((f = fopen(path, "r")) == NULL) {
    perror("fopen");
    exit(1);
  }

  while (fgets(buffer, BUFFER_SIZE, f)) {  //primo elemento quello dove viene meorizzato, dimensione e il terzo è il file dove legge gli elmenti
    e = create_entry(buffer); //creiamo l'entry della riga letta
    if (e != NULL) { //se non è nulla la inserirsce nella lista è aumeta il counter delle righe lette
      l = insert(l, e);
      counter++;
    }
  }

  printf("DB: letti n.%d record da file\n", counter);
  fclose(f);
  return l;
}

void db_child(int in_queue, int out_queue, char *path) {
  query_msg q_msg; //coda iniziale
  out_msg o_msg; //coda di out
  o_msg.type = 1;
  o_msg.done = 0;
  char done_counter = 0;
  list l = load_database(path); //creo una lista per tutti gli elementi del db
  entry *e; 

  while (1) {
    if (msgrcv(in_queue, &q_msg, sizeof(query_msg) - sizeof(long), 0, 0) == -1) {
      perror("msgrcv");
      exit(1);
    }

    if (q_msg.done) {
      done_counter++;
      if (done_counter < 2)
        continue;
      else
        break;
    }

    e = search(l, q_msg.key);

    if (e != NULL) {
      o_msg.e = *e;
      o_msg.id = q_msg.id;
      if (msgsnd(out_queue, &o_msg, sizeof(out_msg) - sizeof(long), 0) == -1) {
        perror("msgsnd");
        exit(1);
      }

      printf("DB: query '%s' da IN%d trovata con valore %d\n", q_msg.key,
             q_msg.id, e->value);

    } else
      printf("DB: query '%s' da IN%d non trovata\n", q_msg.key, q_msg.id);
  }

  o_msg.done = 1;

  if (msgsnd(out_queue, &o_msg, sizeof(out_msg) - sizeof(long), 0) == -1) {
    perror("msgsnd");
    exit(1);
  }

  destroy(l);
  exit(1);
}

void out_child(int queue) {
  out_msg msg; 
  unsigned record_in1 = 0, record_in2 = 0;
  int val1 = 0, val2 = 0;

  while (1) {
    if (msgrcv(queue, &msg, sizeof(out_msg) - sizeof(long), 0, 0) == -1) {
      perror("msgrcv");
      exit(1);
    }

    if (msg.done)
      break;

    if (msg.id == IN1) {
      record_in1++;
      val1 += msg.e.value;
    } else if (msg.id == IN2) {
      record_in2++;
      val2 += msg.e.value;
    }
  }

  printf("OUT: ricevuti n.%d valori validi di IN1 con totale %d\n", record_in1,
         val1);
  printf("OUT: ricevuti n.%d valori validi di IN2 con totale %d\n", record_in2,
         val2);

  exit(0);
}

int main(int argc, char **argv) {
  if (argc < 4) {
    fprintf(stderr, "Usage: %s <db-file> <queries-file-1> <queries-file-2>\n",
            argv[0]);
    exit(1);
  }

  int queue1, queue2;

  if ((queue1 = msgget(IPC_PRIVATE, IPC_CREAT | 0600)) == -1) {
    perror("msgget");
    exit(1);
  }

  if ((queue2 = msgget(IPC_PRIVATE, IPC_CREAT | 0600)) == -1) {
    perror("msgget");
    exit(1);
  }

  // IN1
  if (fork() == 0)
    in_child(IN1, queue1, argv[2]);

  // IN2
  if (fork() == 0)
    in_child(IN2, queue1, argv[3]);

  // DB
  if (fork() == 0)
    db_child(queue1, queue2, argv[1]);

  // OUT
  if (fork() == 0)
    out_child(queue2);

  for (int i = 0; i < 4; i++)
    wait(NULL);

  msgctl(queue1, IPC_RMID, NULL);
  msgctl(queue2, IPC_RMID, NULL);
}