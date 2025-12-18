#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

unsigned int get_random() {
    static unsigned int m_w = 424242;
    static unsigned int m_z = 242424;
    m_z = 36969 * (m_z & 65535) + (m_z >> 16);
    m_w = 18000 * (m_w & 65535) + (m_w >> 16);
    return (m_z << 16) + m_w;
}

struct Parameters {
    int min;
    int max;
    int n;
};

struct Parameters decodeParameters(int argc, char *argv[]) {
    struct Parameters params;

    if (argc != 4) {
        fprintf(stderr, "Uso: %s min max n\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *endptr;
    params.min = strtol(argv[1], &endptr, 10);

    if (*endptr) {
        fprintf(stderr, "Errore: Il primo parametro deve essere un intero valido.\n");
        exit(EXIT_FAILURE);
    }

    params.max = strtol(argv[2], &endptr, 10);

    if (*endptr) {
        fprintf(stderr, "Errore: Il secondo parametro deve essere un intero valido.\n");
        exit(EXIT_FAILURE);
    }

    params.n = strtol(argv[3], &endptr, 10);

    if (*endptr) {
        fprintf(stderr, "Errore: Il terzo parametro deve essere un intero valido.\n");
        exit(EXIT_FAILURE);
    }

    if (params.min < 7 || params.max > 12) {
        fprintf(stderr, "Errore: min deve essere compreso tra 7 e 12.\n");
        exit(EXIT_FAILURE);
    }

    if (params.max < 7 || params.max > 12) {
        fprintf(stderr, "Errore: max deve essere compreso tra 7 e 12.\n");
        exit(EXIT_FAILURE);
    }

    if (params.n <= 0) {
        fprintf(stderr, "Errore: n deve essere maggiore di zero.\n");
        exit(EXIT_FAILURE);
    }

    return params;
}

char *generateString(int l) {
    char alphabet[] = "aeiou";

    char *str = calloc(l+1,sizeof(char));

    for(int i=0; i<l; i++) {
        str[i] = alphabet[get_random()%5];
    }
    str[l]=0;

    return str;
}

char **makeArray(int min, int max, int n) {
    char **A = calloc(n, sizeof(char*));

    for(int i=0; i<n; i++){
        int x = get_random()%(max-min+1)+min;
        A[i] = generateString(x);
    }

    return A;
}

void printArray(char **A, int n) {
    for(int i=0; i<n; i++) {
        printf("%s\n", A[i]);
    }
}

void sortArray(char **A, int n) {
    // scorri tutte le sottosequenze ordinate che
    // iniziano con zero 
    for(int i=1; i<n; i++) {
        // data la sottosequenza che inizia in zero
        // e finisce in i-1, l'obiettivo è di
        // inserire il valore a[i] nella posizione
        // corretta della sottosequenza ordinata
        char* val = A[i]; //conservo il valore di a[i] (lo sovrascriveremo)
        int j = i-1; //parto dall'ultimo elemento della sottosequenza
        while(j>=0 && strcmp(A[j],val)>0){ //finché val è più piccolo dell'elemento corrente
            A[j+1] = A[j]; //sposto l'elemento "in avanti"
            j--;
        }
        A[j+1] = val; //inserisco val nella posizione corretta
    }
}

char* concatString(char **A, int n) {
    char* outstr = calloc(n*12, sizeof(char));

    for(int i=0; i<n; i++) {
        strcat(outstr, A[i]);
    }

    return outstr;
}

void replaceCharacters(char *B) {

    int i = 0;

    char last = ' ';

    while(B[i]) {
        if(B[i]=='u' && last=='e')
            B[i] = '*';
        last = B[i];
        i++;
    }

}


int main(int argc, char *argv[]) {
    struct Parameters params = decodeParameters(argc, argv);

    printf("Punto A - Parametri inseriti: min=%d, max=%d, n=%d\n", params.min, params.max, params.n);

    char **A = makeArray(params.min, params.max, params.n);

    printf("\nPunto B:\n");
    printArray(A,params.n);

    sortArray(A, params.n);
    char *B = concatString(A, params.n);
    
    printf("\nPunto C: %s\n", B);

    replaceCharacters(B);

    printf("\nPunto D: %s\n", B);

    return 0;
}
