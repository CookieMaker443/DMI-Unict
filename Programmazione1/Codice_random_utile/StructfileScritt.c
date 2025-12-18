#include <stdio.h>
#include <string.h>

struct persona {
    char nome[15];
    char cognome[20];
    int eta;
    char sesso;
};
typedef struct persona Persona;

int main() {
    
    int NumStruct;

    printf("quanti record vuoi salvare?\n");
    scanf("%d", &NumStruct);
    Persona lista[NumStruct];

    printf("inseriscri in ordine: nome, cognome, eta, sesso[M/F]\n");

    FILE *fptr = fopen("Lista_di_persone.txt", "w");
    if (fptr == NULL) {
        printf("Errore nell'apertura del file\n");
        return 1;
    };

    int i = 0;
    while(i < NumStruct) {
        printf("inserire il %d^ record:\n", i+1);
        scanf("%s %s %d %c", lista[i].nome, lista[i].cognome, &lista[i].eta, &lista[i].sesso);
        fprintf(fptr, "%s,%s,%d,%c\n", lista[i].nome, lista[i].cognome, lista[i].eta, lista[i].sesso);
        i = i + 1;
    };

    fclose(fptr);
    printf("I record sono stati salvati nella lista\n");
    return 0;
}