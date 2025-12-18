Block x;
AmazingBlock y;

void setup(){
  size(256,512);
   x = new Block((int)random(0,width),(int)random(0,height/2),40,60);
   y = new AmazingBlock((int)random(0,width),(int)random(height/2, height),40,60);
  frameRate(120);
}

void draw(){
  background(0);
  x.Move();
  y.Move();
}

void keyPressed(){
 if(key == 'r' || key == 'R'){
   setup();
 }
}
