#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct data{
    int N;
} data;

typedef struct list{
    char cIN;
    struct list *next;
} list;

unsigned int get_random(){
    static unsigned int m_w = 123456;
    static unsigned int m_z = 789123;
    m_z = 36969 * (m_z & 65535) + (m_z >> 16);
    m_w = 18000 * (m_w & 65535) + (m_w >> 16);
    return (m_z << 16) + m_w;
}

data readInput(int argc, char *argv[]){

    if(argc != 2){
        fprintf(stderr,"Errore: inserire un solo numero positivo (int)!");
        exit(-1);
    }

    char **end = calloc(1,sizeof(char *));
    int N = strtod(argv[1],end);

    if(N<0 && *end != NULL){
        fprintf(stderr,"Errore: inserire un numero positivo (int)!");
        exit(-1);
    }

    data dati = {N};
    return dati;
}

char getRandomChar(char *s) {
    return s[get_random() %strlen(s)];
}

char genVowel(){
    return getRandomChar("AEIOUaeiou");
}

char getConsonant() {
 return getRandomChar("QWRTYPDSFGHJKLZXCVBNMqwrtypdsfghjklzxcvbnm");

}

void push(list **head, char c){

    list *node = malloc(sizeof(list));

    node->cIN = c;
    node->next = *head;
    *head = node;

}

char pop(list **head){

    list *cur = *head;

    char c = cur->cIN;

    *head = cur->next;

    return c;

}

void fillStack(list **head, int N){

    for(int i=0;i<N;i++){
        char x = getRandomChar("123456789");
        int c;

        if(x>='1' && x<='4'){
            for(int ix=0;ix<x-48;ix++){
                c = genVowel();

                if(c=='v'){
                    c='*';
                }
                if(c=='w'){
                    c='?';
                }
                push(head,c);
            }
        }

        if(x>='5' && x<='9'){
            for(int ix=0;ix<x-48;ix++){
                c = getConsonant();

                if(c=='v'){
                    c='*';
                }
                if(c=='w'){
                    c='?';
                }
                push(head,c);
            }
        }

        push(head,x);
    }

}

char **emptyStack(list **head, int N){

    char **A = calloc(N, sizeof(char*));

    for(int i=0;i<N;i++){
        int x = pop(head);

        A[i] = calloc(x-48, sizeof(char*));
        for(int j=0;j<x-48;j++){
            A[i][j] = pop(head);
        }
        
    }

    return A;

}

void printArray(char **A, int N){
    
    for(int i=0;i<N;i++){
        printf("%s\n",A[i]);
    }

}

int main(int argc, char *argv[]){

    data dati = readInput(argc,argv);
    list *head = NULL;
    fillStack(&head, dati.N);
    char **A = emptyStack(&head, dati.N);
    printArray(A, dati.N);
    
}