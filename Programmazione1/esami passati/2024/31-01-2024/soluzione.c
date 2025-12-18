#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

struct parametri {
    char* input_filename;
    char* output_filename;
};

struct parametri decodeParameters(int argc, char *argv[]) {
    struct parametri p;

    if (argc != 3) {
        fprintf(stderr, "Usage: %s input_filename.txt output_filename.txt\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    p.input_filename = argv[1];
    p.output_filename = argv[2];

    
    if (strcmp(p.input_filename + strlen(p.input_filename) - 4, ".txt") != 0) {
        fprintf(stderr, "Error: file must have .txt extension.\n");
        exit(EXIT_FAILURE);
    }

    if (strcmp(p.output_filename + strlen(p.output_filename) - 4, ".txt") != 0) {
        fprintf(stderr, "Error: file must have .txt extension.\n");
        exit(EXIT_FAILURE);
    }

    return p;
}

void printParameters(struct parametri p) {
    printf("input_filename = %s\n", p.input_filename);
    printf("output_filename = %s\n", p.output_filename);
    puts("\n");
}

FILE *openFile(const char *filename, const char* modes) {
    FILE *fp = fopen(filename, modes);
    if (fp == NULL) {
        fprintf(stderr, "Error: cannot open file %s.\n", filename);
        exit(EXIT_FAILURE);
    }
    return fp;
}

void readHeader(FILE *fp, int* n, int* m) {
    fscanf(fp, "%d %d", n, m);
}

double ***readFile(FILE *fp, int n, int m) {
    double ***X = (double***) malloc(n * sizeof(double **));
    for (int i = 0; i < n; i++) {
        X[i] = (double**) malloc(m * sizeof(double*));
        for (int j = 0; j < m; j++) {
            X[i][j] = malloc(sizeof(double));
            fscanf(fp, "%lf", X[i][j]);
        }
    }
    return X;
}

void printMatrix(FILE *fp, double ***X, int n, int m) {
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < m; j++) {
            if (X[i][j])
                fprintf(fp, "%7.2f", *X[i][j]);
            else
                fprintf(fp, "%7s","*");
        }
        fprintf(fp,"\n");
    }
}

double* getArray(double ***X, int n, int m) {
    double *Y = (double*) calloc(n, sizeof(double));
    for (int i = 0; i < n; i++) { //righe
        Y[i] = 0;
        int num = 0;
        for (int j=0; j<m; j++) {
            if(X[i][j]) {
                Y[i]+=*X[i][j];
                num++;
            }
        }
        Y[i]/=num;
    }
    return Y;
}

void printArray(double *array, int n) {
    for(int i=0; i<n; i++) {
        printf("%0.2f ", array[i]);
    }
    puts("\n");
}

void selectionSort(double *Y, int n) {
    for(int pass = 0; pass < n-1; pass++) {
        int idx_min = pass;
        for(int i = pass+1; i<n; i++) { 
            if(Y[i]<Y[idx_min])
                idx_min = i;
        }
        
        float tmp = Y[pass];
        Y[pass] = Y[idx_min];
        Y[idx_min] = tmp;
    }
}

void processX(double ***X, double *Y, int n, int m) {
    for(int i=0; i<n; i++) {
        for (int j=0; j<m; j++) {
            if(X[i][j] && *X[i][j]>Y[i]) {
                X[i][j] = NULL;
            }
        }
    }
}

void writeToFile(const char *filename, double ***X, int n, int m) {
    FILE *fp = openFile(filename,"w");

    printMatrix(fp, X, n, m);
    fclose(fp);
}

int main(int argc, char *argv[]) {
    struct parametri p = decodeParameters(argc, argv);
    
    printf("======A Stampa Parametri======\n");
    printParameters(p);

    int n, m;

    FILE *fp = openFile(p.input_filename,"r");

    readHeader(fp, &n, &m);

    printf("======B Valori n e m======\nn=%d m=%d\n\n", n, m);
    
    double ***X = readFile(fp, n, m);
    fclose(fp);
    printf("======B Matrice X======\n");
    printMatrix(stdout, X, n, m);

    double *Y = getArray(X, n, m);
    printf("======C Array Y======\n");
    printArray(Y, n);

    selectionSort(Y, n);
    printf("======C Array Y Ordinato======\n");
    printArray(Y, n);

    processX(X, Y, n, m);
    printf("======D Matrice X modificata======\n");
    printMatrix(stdout, X, n, m);

    writeToFile(p.output_filename, X, n, m);
}
