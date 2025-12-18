//tiro di due dadi 
#include <stdio.h>
#include <time.h>

int main(void) {
   srand(time(NULL));
   
    int dado1 = 1 + rand() % 6; //NUMERO RANDOM TRA 1 E 6 - DADO 1
    int dado2 = 1 + rand() % 6; //NUMERO RANDOM TRA 1 E 6 - DADO 2


printf("numero del primo dado: %d\n", dado1);
printf("numero del secondo dado: %d\n", dado2);
printf("Il risultato e': %d\n", dado1 + dado2); // RISULTATO DEL TIRO

}