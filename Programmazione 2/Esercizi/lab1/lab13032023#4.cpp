#include <iostream>
#include <time.h>
using namespace std;

class Ora {
//attributi
private:
    int ora, min, sec;

//metodi
public:
    Ora() {ora = 0; min = 0; sec = 0;}; //costruttore che azzera tutto
    Ora(int o, int m, int s) {ora = o; min = m; sec = s;}; //costruttore con i valori.
    // se in nel costruttore "ora" passo dei parametri, usero automaticamente il secondo

    void visualizza (); // modo per visualizzare l'ora  -  Prototipo
    void somma(Ora o1, Ora o2); // somma due orari diversi  -  Prototipo

};

void Ora::visualizza() {
    cout << ora << ":" << min << ":" << sec;
};

void Ora::somma(Ora o1, Ora o2) {
    sec = (o1.sec+o2.sec)%60;
    min = ((o1.sec+o2.sec) / 60 + o1.min +o2.min) % 60;
    ora = (((o1.sec+o2.sec) / 60 + o1.min +o2.min) + o1.ora + o2.ora) % 24;
};

int main() {

    int s = time(NULL) %60;
    int m = s &60;
    int o = s % 24;

    Ora att(o,m,s);
    Ora x;
    Ora y(10,20,30), z(20,30,50);
    x.somma(y,z);
    x.visualizza();
    printf("\n");
    att.visualizza();
    return 0;

}