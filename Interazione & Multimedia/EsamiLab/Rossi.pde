class Rossi extends Bianchi{
  
  Rossi(int x, int y, int d, int posx ,int velx){
   super(x,y,d,posx,velx);
  }
  void disegna (){
   stroke(255,0,0);
   fill(0);
   ellipse(x, y, d,d); //testa
   line(x, y+10, x, y+60); //corpo
   line (x-15,y+10, x+15,y+10);// braccia
   line(x-15,y+10,x-15,y); // braciosx
   line(x+15,y+10,x+15,y+20); // braccio dx
   line(x,y+60,x-10,y+70);//gamba s
   line(x,y+60,x+10, y+70); //gamba d 
 }
  
   void move(){
    x=x+velx;
    if(x>width+10){
      x=-10;
    }
  }
  
  void run(){
    disegna ();
    move ();
  }
}
