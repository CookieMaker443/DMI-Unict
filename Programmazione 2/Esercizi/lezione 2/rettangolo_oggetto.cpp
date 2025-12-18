#include <iostream>

class Rettangolo{
private:
    //attrinuti
    float base;
    float altezza;
 public:   
    //metodi
    void init_rettangolo(float _b, float _h);
    float area();
    float perimetro();


};

void Rettangolo::init_rettangolo(float _b, float _h){
    base=_b;
    altezza=_h;
}

float Rettangolo::area(){

    return base*altezza;

}

float Rettangolo::perimetro(){
    return 2*(base+altezza);
}

int main(int argc, char *argv[]){
    //Dichiarazione oggetti;

    Rettangolo mioRettangolo1, mioRettangolo2; 
    mioRettangolo1.init_rettangolo(3.0,10.0);
    mioRettangolo2.init_rettangolo(20.5,44);
    std::cout<<"Il periemtro del primo rettangolo: "<<mioRettangolo1.perimetro()<<std::endl;
    std::cout<<"Il periemtro del secondo rettangolo: "<<mioRettangolo2.perimetro()<<std::endl;
    std::cout<<"l'area del primo rettangolo: "<<mioRettangolo1.area()<<std::endl;
    std::cout<<"l'area del secondo rettangolo: "<<mioRettangolo2.area()<<std::endl;
    return 0;
}