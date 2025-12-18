#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define max_word 100
#define max_len 1000

typedef struct record{
    char word[max_word];
    int count;
} record;

typedef struct list{
    record rec;
    struct list *next;
} list;

int readN(int argc, char *argv[]){

    if(argc != 2){
        fprintf(stderr,"Error!!");
        exit(-1);
    }

    char **end = malloc(sizeof(char*));
    *end = NULL;

    int n = strtod(argv[1], end);

    if(**end){
        fprintf(stderr,"Error 2!!");
        exit(-1);
    }

    return n;

}

void fillList(list **head, record campi){

    list *new_node = (list *) malloc(sizeof(list));

    strcpy(new_node->rec.word, campi.word);
    new_node->rec.count = campi.count;

    new_node->next = *head;
    *head = new_node;
}

void filterList(list **head, int n){

    list *curr = *head;
    list *prev = NULL;

    while(curr != NULL){ //fin quando la testa non arriva a NULL
        if(curr->rec.count < n){ //se la condizione per la quale il nodo deve essere cancellato si attiva

            if (prev == NULL) { //se il precedente � NULL vuol dire che va eliminata la testa
                *head = curr->next;
            } else {
                prev->next = curr->next; //cambia il next del puntatore con il next del nodo da cancellare (cio� al nodo successivo)
            }

        }else{
            prev = curr; //se la condizione si avvera prev diventa la testa (solo se non si attiva la condizione senno rimane l'indirizzo di un nodo cancellato)
        }

        curr = curr->next; //porta avanti la testa

    }

    free(curr); //dealloca temp

}

void printList(list *head){

    while(head != NULL){
        printf("%s - %d\n",head->rec.word, head->rec.count);
        head = head->next;
    }

    printf("\n");

}

list *readFile(){

    list *head = (list *) malloc(sizeof(list));
    head = NULL;

    FILE *fp = fopen("input.txt","r");

    if(!fp){
        fprintf(stderr,"Errore nell'apertura del file!!");
        exit(-1);
    }

    while(!feof(fp)){

        char str[max_len];
        fgets(str, max_len, fp);

        record campi;
        strcpy(campi.word,strtok(str, " "));

        char *CTRLword;
        campi.count = 1;

        while ((CTRLword = strtok(NULL, " ")) != NULL) {
            if (strcmp(CTRLword,campi.word) == 0) {
               campi.count++;
            }
        }
        
        fillList(&head,campi);
        //printf("%s - %d\n",campi.word, campi.count);

   }

    printList(head);

    return head;

}


int main(int argc, char *argv[]){
    
    int n = readN(argc,argv);
    list *head = readFile();
    filterList(&head,n);
    printList(head);
}