#include <stdio.h>

int main() {
    FILE *fp; //dichiarazione del puntatore al file
    char string[] = "Questo è un esempio di una stringa scritta in un file.";

    fp = fopen("file_esempio.txt", "w");  //apre il file in scrittura
    fprintf(fp, "%s", string);  //scrive la stringa nel file
    fclose(fp);   //chiude il file

    char read_string[100];  //dichiarazione di una stringa per leggere il contenuto del file
    fp = fopen("file_esempio.txt", "r");  //apre il file in lettura
    fscanf(fp, "%s", read_string); //legge il contenuto del file e lo memorizza in read_string
    printf("Il contenuto del file è: %s\n", read_string);  //stampa il contenuto del file
    fclose(fp);  //chiude il file
    return 0;
}
