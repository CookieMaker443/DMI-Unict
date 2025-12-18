Ball b;
HyperBall hb;

void setup(){
  size(512, 512);
  b = new Ball(150,1,40);
  hb = new HyperBall(300, 1, 40);
  
}

void draw(){
   background(220);
  
  b.move();
  b.disegna();
  
  hb.move();
  hb.checkMouse();
  hb.disegna();
}
