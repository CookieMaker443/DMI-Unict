#include <stdio.h>
#include <string.h>

struct persona {
    char nome[20];
    int eta;
    char sesso;
};

int main() {
    struct persona lista[3];

    // Inizializzazione della lista
    strcpy(lista[0].nome, "Mario Rossi");
    lista[0].eta = 30;
    lista[0].sesso = 'M';
    strcpy(lista[1].nome, "Lucia Bianchi");
    lista[1].eta = 25;
    lista[1].sesso = 'F';
    strcpy(lista[2].nome, "Giovanni Neri");
    lista[2].eta = 35;
    lista[2].sesso = 'M';

    // Apertura del file in modalità scrittura
    FILE *fp = fopen("lista_persone.txt", "w");
    if (fp == NULL) {
        printf("Errore nell'apertura del file\n");
        return 1;
    }

    // Scrittura della lista nel file
    for (int i = 0; i < 3; i++) {
        fprintf(fp, "%s,%d,%c\n", lista[i].nome, lista[i].eta, lista[i].sesso);
    }

    // Chiusura del file
    fclose(fp);

    printf("La lista è stata salvata nel file lista_persone.txt\n");

    return 0;
}
