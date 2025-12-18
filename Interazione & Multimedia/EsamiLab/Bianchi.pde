class Bianchi{
 int x,y,d, posx, velx; 
 
 Bianchi(int x, int y ,int d, int posx, int velx) {
  this.x=x; 
  this.y=y;
  this.d=d;
  this.posx=posx;
  this.velx=velx;
 }
 
 void disegna (){
   stroke(255);
   fill(0);
   ellipse(x, y, d,d); //testa
   line(x, y+10, x, y+60); //corpo
   line (x-15,y+10, x+15,y+10);// braccia
   line(x,y+60,x-10,y+70);//gamba s
   line(x,y+60,x+10, y+70); //gamba d 
 }
  
  void move(){
    x=x+velx;
    if(x>width-20 || x<20){
      posx=posx*-1;
      velx=velx*posx;
    }
  }
  
  void run(){
    disegna ();
    move ();
  }
 
}
