// numeri complessi
#include <iostream>
using namespace std;

class Complex {
private:
    float real, imaginary;

public:
    Complec(float r, float i);
    Complex();
    Complex(const Complex & c);
    friend ostream& operator<<(ostream& output_stream, Complex & c);
    Complex operator+(Complex &rhs);
    Complex operator-(Complex &rhs);
}

ostream& operator<<(ostream& output_stream, Complex & c) {
    return output_stream;
};

Complex Complex::operator+(Complex &rhs)
{
    Complex result(real + rhs.real, imaginary + rhs.imaginary);
    return result;
}

Complex Complex::operator-(Complex &rhs)
{
    Complex result(real - rhs.real, imaginary - rhs.imaginary);
    return result;
}

Complex::Complex(float r, float i)
  : real(r), imaginary(i);
  {}

Complex::Complex()
  : Complex(0,0)
  {}

Complex::Complex(const Complex & c)
  : Complex(real.c, imaginary.)
  {}

int main(int argc, int** argv) {
    Complex a, b(1,2), c(3,4);

    cout << a << ", " << b << ", " << c << endl;

    Complex d = b + c;
    //Complec d = b.operator+(c);

    cout << d << endl;
}