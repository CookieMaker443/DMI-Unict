// Richiedo una quantità di numeri pari, e successivamente una quantità di numeri dispari
#include <stdio.h>

int main() {
    int InputP = 0;
    int InputD = 0;

        printf("quantità di numeri pari: ");
    scanf("%d", &InputP);
        printf("quantità di numeri dispari: ");
    scanf("%d", &InputD);

    int P = 0;
    int Pc = 0;

        while (Pc < InputP) {
        printf("%d ",P);
        P = P + 2;
        Pc = Pc + 1;
        }
        
    int D = P - 1;
    int Dc = 0;
        while (Dc < InputD) {
        printf("%d ",D);
        D = D + 2;
        Dc = Dc + 1;
        }
}
