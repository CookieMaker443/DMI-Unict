#include <iostream>
using namespace std;

typedef struct{
    float b, h;
}rettangolo;

void init_rettangolo(rettangolo * rect, float _b, float _h){
    rect->b=_b;
    rect->h=_h;
}

float area(rettangolo * rect){
    return rect->b * rect->h;
}

float perimetro(rettangolo * rect){
    return 2*(rect->b + rect->h);
}

int main(){
    rettangolo myrettangolo;
    init_rettangolo(&myrettangolo, 10.5, 22.3);
    cout<<"Perimetro: "<<perimetro(&myrettangolo)<<endl;
    cout<<"Area: "<<area(&myrettangolo)<<endl;
}

