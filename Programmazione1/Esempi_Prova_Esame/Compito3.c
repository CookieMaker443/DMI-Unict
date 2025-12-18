/*
Scrivere un programma in C che:
- A prenda in input da tastiera (argomenti della funzione main) un intero positivo N in 
[10,20] ed un carattere w; se gli argomenti a riga di comando non rispondono ai 
suddetti requisiti, il programma stampa un messaggio di errore sullo standard error 
e termina la propria esecuzione;
- B generi, mediante successivi inserimenti, una lista concatenata semplice (ordinata 
in modo crescente - ordinamento lessicografico) che contenga N stringhe con 
caratteri pseudo-casuali in [‘a’-’z’] di lunghezza pseudo-casuale L nell'intervallo 
[5,15];
- C stampi sullo standard output l'intera lista; 
- D stampi sullo standard output il numero totale di occorrenze del carattere w in tutte
le stringhe della lista.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

unsigned int get_random() {
  static unsigned int m_w = 123456;
  static unsigned int m_z = 789123;
  m_z = 36969 * (m_z & 65535) + (m_z >> 16);
  m_w = 18000 * (m_w & 65535) + (m_w >> 16);
  return (m_z << 16) + m_w;
}

struct data {
  int N;
  char w;
};

struct data readInput(int argc, char *argv[]) {

  if (argc != 3) {
    fprintf(stderr, "Devi inserire 2 parametri");
    exit(-1);
  }

  struct data d = {0, 0};
  d.N = atoi(argv[1]);

  if (d.N < 10 || d.N > 20) {
    fprintf(stderr, "Il primo parametro deve essere compreso tra 10 e 20");
    exit(-1);
  }

  if (strlen(argv[2]) != 1) {
    fprintf(stderr, "Il secondo parametro deve essere un carattere");
    exit(-1);
  }

  d.w = argv[2][0];

  return d;
}

char *genString() {
  int L = 0;
  L = get_random() % (15 - 5 + 1) + 5;
  char *s = calloc(L, sizeof(char));
  for (int i = 0; i < L; i++) {
    s[i] = get_random() % ('z' - 'a' + 1) + 'a';
  }
  return s;
}

struct node {
  char *str;
  struct node *next;
};

void insertString(struct node **head) {
  struct node *nuovo = (struct node *)malloc(sizeof(struct node));
  
  nuovo->str = genString();
  nuovo->next = NULL;

  if(*head==NULL || strcmp((*head)->str, nuovo->str)>0){ //Se la testa e' null o il contenuto della testa è maggiore del nuovo nodo
    nuovo->next=*head; //il next di nuovo diventa la testa
    *head=nuovo; //e la testa diventa il nuovo nodo
    return;
  }

  struct node *current=*head; //si crea una variabile nodo per gestire il corrente
  while(current->next!=NULL && strcmp(current->next->str, nuovo->str)<0){ //cicla fino a quando non sei arrivato a null o la condizione si e' avverata
    current=current->next; //vai avanti;
  }

  //inserimento in coda
  nuovo->next=current->next; //il next del nuovo nodo diventa il next del corrente
  current->next=nuovo; //e il next del corrente diventa il nuovo nodo
}

void genList(struct node **head, int N) {
  for (int i = 0; i < N; i++) {
    insertString(head);
  }
}

void printList(struct node *head) {
  while (head != NULL) {
    printf("%s\n", head->str);
    head = head->next;
  }
}

int printOcc(struct node *head, char w){
  int c=0;
  while (head!=NULL){
    for(int i=0; i<strlen(head->str); i++){
      if(head->str[i]==w)
        c++;
    }
    head=head->next;
  }
  return c;
}

int main(int argc, char *argv[]) {
  struct data d = readInput(argc, argv);

  struct node *head = NULL;

  genList(&head, d.N);

  printList(head);

  printf("Numero totale di occorrenze di %c: %d", d.w, printOcc(head, d.w));
}
