#include <iostream>
#include <time.h>
using namespace std;

int main ()  
{
    srand(time(NULL));
    short int x = rand();
    
    short int giorno, mese, anno  = x;
    cout << "inserisci la data: (gg,mm)";
    cin >> giorno >> mese ;

    cout << "la data inserita è:" << giorno << "/" << mese << "/" << anno << endl;
    return 0;
}