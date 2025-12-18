/*
Si scriva un programma C che:
- A prenda un input da tastiera (argomenti della funzione main()) un intero k in [10,15], un
carattere w in [‘a’-’z’] e due interi N ed M in [4,8]; se gli argomenti a riga di comando non
rispondono ai suddetti requisiti, il programma stampa un messaggio di errore sullo standard
error e termina la propria esecuzione con codice di errore “-1”; 
- B allochi dinamicamente una matrice S di stringhe (char*) di dimensioni N x M;
- C riempia la matrice S con NxM stringhe di lunghezza k composte di caratteri pseudocasuali in [a-z];
- D ordini ogni colonna della matrice S in modo crescente (ordinamento lessicografico) con
un algoritmo di ordinamento a scelta tra Insertion Sort e Bubble Sort;
- E stampi la matrice sullo standard output;
- F stampi sullo standard output la stringa (e gli indici all'interno della matrice) che contiene il
maggior numero di occorrenze del simbolo w. Queste ultime vanno sostituite, sullo standard
output, con il carattere '*'.
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
    int k;
    char w;
    int N;
    int M;
};

struct data readInput(int argc, char *argv[]){
    if(argc!=5) {
        fprintf(stderr, "Errore, il numero di argomenti deve essere pari a 4\n");
        exit(-1);
    }

    struct data d = {0, 0, 0, 0};
    
    char **end = malloc(sizeof(char*));
    *end = NULL;
    
    d.k = (int) strtol(argv[1], end, 0);
    if(**end) {
        fprintf(stderr, "Errore, il primo parametro deve essere un intero.\n");
        exit(-1);
    }

    d.k=atoi(argv[1]);
    if(d.k<10 || d.k>15) {
        fprintf(stderr, "Errore, il primo parametro deve essere compreso tra 10 e 15\n");
        exit(-1);
    }

    if(strlen(argv[2])!=1) {
        fprintf(stderr, "Errore, il secondo parametro deve essere un carattere.\n");
        exit(-1);
    }

    d.w = argv[2][0];

    if(d.w<'a' || d.w>'z'){
        fprintf(stderr, "Errore, il carattere deve essere nel range a-z.\n");
        exit(-1); 
    }

    d.N = (int) strtol(argv[3], end, 0);

    if(**end) {
        fprintf(stderr, "Errore, il terzo parametro deve essere un intero.\n");
        exit(-1);
    }

    if(d.N<4 || d.N>8){
        fprintf(stderr, "Errore, il terzo parametro deve essere nel range [4, 8].\n");
        exit(-1);
    }

    d.M = (int) strtol(argv[4], end, 0);

    if(**end) {
        fprintf(stderr, "Errore, il quarto parametro deve essere un intero.\n");
        exit(-1);
    }

    if(d.M<4 || d.M>8){
        fprintf(stderr, "Errore, il quarto parametro deve essere nel range [4, 8].\n");
        exit(-1);
    }

    return d;
}

char ***allocateS(int N, int M){
    char ***S=calloc(N, sizeof(char**));
    for(int i=0; i<N; i++){
        S[i]=calloc(M, sizeof(char*));
    }
    return S;
}

char *genString(int len, char min, char max){
    char *s=calloc(len, sizeof(char*));

    for(int i=0; i<len; i++){
        s[i]=get_random()%(max-min+1)+min;
    }
    return s;
}

void fillS(char ***S, int N, int M, int k){
    for(int i=0; i<N; i++){
        for(int j=0; j<M; j++){
            S[i][j]=genString(k, 'a', 'z');
        }
    }
}

void sortS(char ***S, int N, int M) {
    for(int j=0; j<M; j++) { //scorro le colonne
        for(int pass=0; pass<N-1; pass++) {
            for(int i=0; i<N-1-pass; i++) {
                if(strcmp(S[i][j], S[i+1][j])>0) {
                    char *tmp = S[i][j];
                    S[i][j] = S[i+1][j];
                    S[i+1][j] = tmp;
                }
            }
        }
    }
}

void printMatrix(char ***S, int N, int M){
    for(int i=0; i<N; i++){
        for(int j=0; j<M; j++){
            printf("%s ", S[i][j]);
        }
        printf("\n");
    }
}

int conta(char *s, char w){
    int c=0;
    for(int i=0; i<strlen(s); i++){
        if(s[i]==w)
            c++;
    }
    return c;
}

void printMax(char ***S, int N, int M, char w){
    int i_max=0;
    int j_max=0;
    int max=0;

    for(int i=0; i<N; i++){
        for(int j=0; j<M; j++){
            int count=conta(S[i][j], w);
            if(count>max){
                max=count;
                i_max=i;
                j_max=j;
            }
        }
    }
    printf("\nStringa con %d occorrenze di '%c' trovata agli indici %d, %d: ", max, w, i_max, j_max);   

    for(int i=0; i<strlen(S[i_max][j_max]); i++){
        if(S[i_max][j_max][i]==w){
            S[i_max][j_max][i]='*';
        }
    }
    printf("%s", S[i_max][j_max]);
}

int main(int argc, char *argv[]){

    struct data d=readInput(argc, argv);
    char ***S = allocateS(d.N, d.M);
    fillS(S, d.N, d.M, d.k);
    sortS(S, d.N, d.M);
    printMatrix(S, d.N, d.M);
    printMax(S, d.N, d.M, d.w);
}