/*
Si scriva un programma C che:
- A chieda all'utente di inserire un intero “n” (si assuma n<256), una stringa "s1", una stringa
“s2”, e un carattere "c" da tastiera. Si verifichi che entrambe le stringhe siano di lunghezza
“n”. In caso contrario, si stampi un errore su standard error e si termini il programma con un
opportuno codice di terminazione;
- B costruisca una nuova stringa “s3” ottenuta sostituendo tutte le occorrenze del carattere "c"
in “s1” con i caratteri che si trovano in “s2” nelle posizioni corrispondenti; 
- C definisca una nuova stringa “s4” ottenuta invertendo l’ordine dei caratteri in “s2”;
- D concateni le stringhe “s3” e “s4” in una nuova stringa “s5” e la ordini in ordine
lessicografico ascendente usando un algoritmo di ordinamento a scelta;
- E stampi a schermo la stringa ordinata. I caratteri i cui codici numerici relativi (fare cast dei
caratteri a int) siano dispari, vanno sostituiti con “*”.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void readInput(int *n, char *s1, char *s2, char *c){
    /*
    FILE *fp;
    fp = fopen("input.txt","r");
    if (fp == NULL){
        fprintf(stderr,"DOCd");
        exit(-1);
    }

    if(!feof(fp)){
        fscanf(fp,"%d\n%s\n%s\n%c",n,s1,s2,c);
    }

    fclose(fp);
    //printf("%d %s %s %c\n\n\n",*n,s1,s2,*c);
    */

    printf("Inserisci un numero: ");
    scanf("%d", n);
    printf("Inserisci la prima stringa: ");
    scanf("%s", s1);
    printf("Inserisci la seconda stringa: ");
    scanf("%s", s2);
    printf("Inserisci un carattere: ");
    scanf(" %c", c);

    if(*n>=256){
        fprintf(stderr, "Il primo numero deve essere minore di 256\n");
        exit(-1);
    }

    if(strlen(s1)!=*n){
        fprintf(stderr, "La prima stringa deve avere lunghezza %d \n", *n);
        exit(-1);
    }

    if(strlen(s2)!=*n){
        fprintf(stderr, "La seconda stringa deve avere lunghezza %d \n", *n);
        exit(-1);
    }
}

char *replaceChar(char *s1, char *s2, char c){
    char *s3=calloc(strlen(s1), sizeof(char));

    for(int i=0; i<strlen(s1); i++){
        if(*(s1+i)==c)
            *(s3+i)=*(s2+i);
        else
        *(s3+i)=*(s1+i);
    }
    return s3;
}

char *invertString(char *s2){
    char *s4=calloc(strlen(s2), sizeof(char));

    for(int i=0; i<strlen(s2); i++){
        *(s4+i)=*(s2+strlen(s2)-1-i);
    }
    return s4;
}

void sort(char *s5){
    for(int pass = 0; pass < strlen(s5)-1; pass++) {
        for (int i=0; i<strlen(s5)-1-pass; i++) {
            if(s5[i]>s5[i+1]) {
                char c = s5[i];
                s5[i] = s5[i+1];
                s5[i+1] = c;
            }
        }
    }
}

void printResult(char *s5){
    for(int i=0; i<strlen(s5); i++){
        if(*(s5+i)%2==1)
            *(s5+i)='*';
    }
    
    printf("%s", s5);

}

int main(){
    int n; 
    char *s1=calloc(n, sizeof(char));
    char *s2=calloc(n, sizeof(char));
    char c;

    readInput(&n, s1, s2, &c);
    char *s3=replaceChar(s1, s2, c);
    char *s4=invertString(s2);
    char *s5=strcat(s3, s4);
    sort(s5);
    printResult(s5);
}