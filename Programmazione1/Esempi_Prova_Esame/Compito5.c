/*
Scrivere un programma in C che:
- A prenda un input da tastiera (argomenti della funzione main) costituito da un intero
positivo N in [10,20], e due numeri in virgola mobile positivi x,y. Dovra' essere 5.0 <
x < y < 30.0; se gli argomenti a riga di comando non rispondono ai suddetti requisiti,
il programma stampa un messaggio di errore sullo standard error e termina la
propria esecuzione;
- B allochi dinamicamente una matrice A di numeri double pseudo-casuali in [x,y] di
dimensioni N x N;
- C calcoli il minimo degli elementi della diagonale principale della matrice (sia mind),
e il massimo valore degli elementi della diagonale secondaria della matrice stessa
(sia maxd); si restituisca inoltre il numero di elementi della matrice aventi valori in
[mind, maxd];
- D allochi dinamicamente un array di double e lo riempia con tutti gli elementi di A
nell'intervallo [mind, maxd];
- E ordini l'array mediante un algoritmo a scelta tra selection sort e insertion sort;
- F stampi l’array sullo standard output, insieme alla media aritmetica dei suoi
elementi.
*/

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

unsigned int get_random() {
 static unsigned int m_w = 123456;
 static unsigned int m_z = 789123;
 m_z = 36969 * (m_z & 65535) + (m_z >> 16);
 m_w = 18000 * (m_w & 65535) + (m_w >> 16);
 return (m_z << 16) + m_w;
}

struct data{
    int N;
    double x;
    double y;
};

struct data readInput(int argc, char *argv[]){

    if(argc!=4){
        fprintf(stderr, "Devi inserire 3 parametri");
        exit(-1);
    }

    struct data d={0, 0, 0};

    char **end = malloc(sizeof(char*));
    *end = NULL;
    
    d.N = (int) strtol(argv[1], end, 0);
    if(**end) {
        fprintf(stderr, "Errore, il primo parametro deve essere un intero");
        exit(-1);
    }

    if(d.N<10 || d.N>20){
        fprintf(stderr, "Il primo parametro deve essere compreso tra 10 e 20");
        exit(-1);
    }

    char **end1 = malloc(sizeof(char*));
    *end1 = NULL;
    
    d.x = (double) strtof(argv[2], end1);
    if(**end1) {
        fprintf(stderr, "Errore, il secondo parametro deve essere un numero in virgola mobile");
        exit(-1);
    }

    char **end2 = malloc(sizeof(char*));
    *end2 = NULL;
    
    d.y = (double) strtof(argv[3], end2);
    if(**end2) {
        fprintf(stderr, "Errore, il secondo parametro deve essere un numero in virgola mobile");
        exit(-1);
    }

    if(d.x<5 || d.x>=d.y){
        fprintf(stderr, "Il secondo parametro deve essere compreso tra 5 e %f", d.y);
        exit(-1);
    }

    if(d.y<=d.x || d.y>30){
        fprintf(stderr, "Il terzo parametro deve essere compreso tra %f e 30", d.x);
        exit(-1);
    }
    return d;
}

double genDouble(double x, double y){
    double z=0;
    z=((double) get_random() / UINT_MAX) * (y-x)+x;
    return z;
}

double **genMatrix(int N, double x, double y){
    double **A=calloc(N, sizeof(double*));
    for(int i=0; i<N; i++){
        A[i]=calloc(N, sizeof(double));
        for(int j=0; j<N; j++){
            A[i][j]=genDouble(x, y);
        }
    }

    return A;
}

int computeMinMax(double **A, int N, double *mind, double *maxd){
    
    *mind=A[0][0];
    *maxd=A[N-1][0];
    
    for(int i=0; i<N; i++){
        if(A[i][i]<*mind)
            *mind=A[i][i];
    }

    for(int i=0; i<N; i++){
        for(int j=N-1; j<=0; j--){
            if(A[i][j]>*maxd)
                *maxd=A[i][j];
        }
    }

    int c=0; 
    for(int i=0; i<N; i++){
        for(int j=0; j<N; j++){
            if(A[i][j]>=*mind && A[i][j]<=*maxd)
                c++;
        }
    }
    return c;
}

double *createArray(double **A, int N, int c, double mind, double maxd){
    double *a=calloc(c, sizeof(double));
    int r=0;
    for(int i=0; i<N; i++){
        for(int j=0; j<N; j++){
            if(A[i][j]>=mind && A[i][j]<=maxd){
                a[r]=A[i][j];
                r++;
            }   
        }
    }

    return a;
}

void sortArray(double *a, int c){
    // selectionsort
    for(int i = 0; i<c-1; i++) {
        int smallest = i;
        for(int j=i+1; j<c; j++) {
            if(a[j] < a[smallest]) {
                smallest = j;
            }
        }
        double tmp = a[i];
        a[i] = a[smallest];
        a[smallest] = tmp;
    }
}

void printArray(double *a, int c){
    double s=0, m=0;
    for(int i=0; i<c; i++){
        printf("%f\n", a[i]);
        s=s+a[i];
    }
    m=s/c;
    printf("Media: %f", m);
}

int main(int argc, char *argv[]){
    struct data d=readInput(argc, argv);
    double **A=genMatrix(d.N, d.x, d.y);
    double mind, maxd;
    int c=computeMinMax(A, d.N, &mind, &maxd);
    double *a=createArray(A, d.N, c, mind, maxd);    
    sortArray(a, c);
    printArray(a, c);
}