#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(void) {
    int frequenza1 = 0;
    int frequenza2 = 0;
    int frequenza3 = 0;
    int frequenza4 = 0;
    int frequenza5 = 0;
    int frequenza6 = 0;

srand(time(NULL));

//CREO UN LOOP 600K VOLTE
for(int tiro = 0; tiro < 600000; ++tiro) {
    int faccia = 1 + rand() % 6; //NUMERI RANDOM TRA 1 E 6

    //int x = rand() % (b-a)

    //int x = a + rand() % (b-a)
    //a =1;b = 6;

    switch (faccia) {
        case 1: //uscito 1
        ++frequenza1;
        break;
        case 2: //uscito 2
        ++frequenza2;
        break;
        case 3: //uscito 3
        ++frequenza3;
        break;
        case 4: //uscito 4
        ++frequenza4;
        break;
        case 5: //uscito 5
        ++frequenza5;
        break;
        case 6: //uscito 6
        ++frequenza6;
        break;
    }


    }
    printf("la frequenza del numero 1 e': %d\n", frequenza1);
    printf("la frequenza del numero 2 e': %d\n", frequenza2);
    printf("la frequenza del numero 3 e': %d\n", frequenza3);
    printf("la frequenza del numero 4 e': %d\n", frequenza4);
    printf("la frequenza del numero 5 e': %d\n", frequenza5);
    printf("la frequenza del numero 6 e': %d\n", frequenza6);


    // printf("%s%13s\n", "faccia", "frequenza");
    //  printf("   1%13d\n", frequenza1);
}
