#include <iostream>
#include <string>
#include <vector>

using namespace std;

class Frame{
    private:
    int x;
    int y;
    int h;
    int w;

    public:
    Frame(int x, int y, int h, int w){
        this->x=x;
        this->y=y;
        this->h=h;
        this->w=w;
    }

    void stampaCoordinate(){
        cout << "\nX: " << x << endl;
        cout << "Y: " << y << "\n" << endl;
    }

    int getX() const { return x; }
    int getY() const { return y; }
    int getH() const { return h; }
    int getW() const { return w; }

    virtual void onClick(int x, int y) = 0;

    friend ostream& operator<<(ostream& os, const Frame& f){
        os << "\nX: " << f.x << "\nY: " << f.y << "\nH: " << f.h << "\nW: " << f.w;
        return os; 
    }
};

class CheckButton : public Frame {
    private:
    bool status;
    string text;

    public:
    CheckButton(int x, int y, int h, int w, string text) : Frame(x,y,h,w), status(false){
        //status = !status;
        this->text = text;
    }

    void changeStatus(){
        status = !status;
    }


    void onClick(int _X, int _Y) override {
        status=!status;
        cout << "\nPulsante premuto in coordinate X: " << _X << " Y: " << _Y << endl;
        cout << "Pulsante premuto: " << text << endl; 
    }

    friend ostream& operator<<(ostream &os, const CheckButton &f){
        os << "\nX: " << f.getX() << "\nY: " << f.getY() << "\nH: " << f.getH() << "\nW: " << f.getW();
        os << "\nStatus: " << f.status;
        return os; 
    }
};

class Window : public Frame {
    private:
    vector<Frame*> items;
    string text;

    public:
    Window(int x, int y, int h, int w, string text) : Frame(x,y,h,w){
        this->text = text;
    }

    void operator+=(Frame &f){
        items.push_back(&f);
    }

    friend ostream& operator<<(ostream &os, const Window &f){
        os << "\nX: " << f.getX() << "\nY: " << f.getY() << "\nH: " << f.getH() << "\nW: " << f.getW();
        os << "\nName Window: " << f.text;
        return os; 
    }

    void onClick(int _X, int _Y){
        for(Frame* item : items){
            if( _X >= item->getX() && _X <= (item->getX()+item->getW()) && _Y >= item->getY() && _Y <= (item->getY()+item->getH())){
                item->onClick(_X,_Y);

                break;
            }
        }
    }
};

int main(){

    Window window(200,200, 200, 200, "Window");
    CheckButton button1(10,10,20,40,"Bottone1");
    CheckButton button2(70,70,10,10,"Bottone2");

    window+=button1;
    window+=button2;

    cout << "Before Click:\n" << window << endl;

    window.onClick(15, 25); // Click nel pulsante
    window.onClick(70, 70); // Click nel pulsante

    cout << "\nAfter Click:\n" << window << endl;

}