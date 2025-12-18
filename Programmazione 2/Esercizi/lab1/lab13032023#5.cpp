#include <iostream>
using namespace std;

class Insieme(){
    private:
    int array[100];
    int ultimo;


    public:
    // costruttore
    Insieme() {ultimo = 0; for(int i=0; i<100; i++) {v[i]=0;}};
    void Svuota() {ultimo = 0;};
    void Aggiunge(int);
    void Elimina(int);
    void Unisce(Insieme);
    void Uguale(Insieme);
    void Membro(int);
    void Stampa();
};

void insieme::Aggiunge(int elem) {
    int indice=0;
    while( v[indice]<elem && indice<ultimo) indice++;
    if(v[indice] != elem) {
        for(int i=ultimo; i>indice; i--) v[i]=v[i-1];
        v[indice]=elem;
    }
}