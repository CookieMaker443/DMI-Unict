class Block {
 int x;
 int y;
 int w;
 int h;
 int sx=4;
 
 Block(int x, int y, int w, int h){
  this.x = x;
  this.y = y;
  this.w = w;
  this.h = h;
 }
 
  void Disegna(){
    noStroke();
    fill(0,255,0);
    rect(x,y,w,h);
  }
  
  void Move(){
    if(x <= 0 && sx<0){
      sx=sx*-1;
    }
    
    if(x >= width && sx>0){
      sx=sx*-1;
    }
   x = x + sx;
   Disegna();
  }
}
