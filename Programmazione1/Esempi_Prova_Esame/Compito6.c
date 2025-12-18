/*
Scrivere un programma in C che:
- A prenda come argomenti da riga di comando il nome di un file di testo di input
“input” (es. “input.txt”) e il nome di un file di testo di output “output” (es. “output.txt”).
Si assuma che i nomi dei file non superano i 255 caratteri. 
- B legga una matrice A di puntatori float di dimensioni n x m dal file “input”. Si
assuma che il file contenga un numero di righe di testo pari a n e che ogni riga
contenga m valori separati da spazi. Il programma dovrà inferire le dimensioni n e
m dal file di input e riempire opportunamente i valori della matrice;
- C elimini dalla matrice i valori di A che sono superiori ai valori medi delle righe
corrispondenti (far puntare a NULL i puntatori relativi ai valori identificati e liberare
la relativa memoria);
- D inserisca i valori puntati dai puntatori non nulli in A in un array di float; 
- E scriva i valori dell’array sul file “output” il cui nome è stato passato come
argomento da riga di comando (es “output.txt”).
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct data{
    char input[255];
    char output[255];
};

struct data readParameters(int argc, char *argv[]){
    if(argc!=3){
        fprintf(stderr, "Devi inserire due parametri");
        exit(-1);
    }
    
    struct data d;

    strcpy(d.input, argv[1]);
    strcpy(d.output, argv[2]);

    return d;
}

float ***createMatrix(char *input, int *n, int *m){
    FILE *fp=fopen(input, "r");
    if(fp==NULL){
        fprintf(stderr, "Errore nell'apertura del file");
        exit(-1);
    }

    char line[1000];
    while(fgets(line, 1000, fp)!=NULL){
        (*n)++;
    }

    char *token = strtok(line, " ");
    while(token!=NULL){
        token=strtok(NULL, " ");
        (*m)++;
    }
    
    float ***A = calloc(*n, sizeof(float**));
    for(int i=0; i<*n; i++){
        A[i] = calloc(*m, sizeof(float*));
        for(int j=0; j<*m; j++){
            A[i][j] = calloc(1, sizeof(float));
        }
    }

    fseek(fp, 0, SEEK_SET);

    for(int i=0; i<*n; i++){
        fgets(line, 1000, fp);
        token = strtok(line, " ");
        for(int j=0; j<*m; j++){ 
            *(A[i][j]) = atof(token);
            token=strtok(NULL, " ");
        }
    }
    /*
    for(int i=0; i<*n; i++){
        for(int j=0; j<*m; j++){ 
            printf("%f ", *(A[i][j]));
        }
        printf("\n");
    }
    */
    fclose(fp);

    return A;
}

int sparsify(float ***A, int n, int m){
    int c=n*m;
    for(int i=0; i<n; i++){
        float s=0, med=0;
        for(int j=0; j<m; j++){
            s=s+*(A[i][j]);
        }
        med=s/m;
        for(int w=0; w<m; w++){
            if(*(A[i][w])>med){
                A[i][w]=NULL;
                free(A[i][w]);
                c--;
            }
        }
    }

    /*
    for(int i=0; i<n; i++){
        for(int j=0; j<m; j++){
            if(A[i][j]!=NULL)
                printf("%f ", *(A[i][j]));
        }
        printf("\n");
    }
    */
    return c;
}

float **collect(float ***A, int n, int m, int c){
    float **v=calloc(c, sizeof(float*));
    for(int i=0; i<c; i++){
        v[i]=calloc(1, sizeof(float));
    }

    int k=0;
    for(int i=0; i<n; i++){
        for(int j=0; j<m; j++){
            if(A[i][j]!=NULL){
                v[k]=A[i][j];
                k++;
            }
        }
    }
    /*
    for(int i=0; i<c; i++){
        printf("%.2f\n", *(v[i]));
    }
    */

    return v;
}

void writeToFile(char *output, float **v, int c){
    FILE *fp=fopen(output, "w");
    if(fp==NULL){
        fprintf(stderr, "Errore nell'apertura del file");
        exit(-1);
    }

    for(int i=0; i<c; i++){
        fprintf(fp, "%5.2f\n", *(v[i]));
    }
}

int main(int argc, char *argv[]){
    struct data d=readParameters(argc, argv);

    int n=0, m=0;
    float ***A=createMatrix(d.input, &n, &m);

    int c=sparsify(A, n, m);

    float **v=collect(A, n, m, c);

    writeToFile(d.output, v, c);
}