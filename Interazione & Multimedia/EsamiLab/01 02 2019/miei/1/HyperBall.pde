class HyperBall extends Ball{
  color X = color(0,0,255);
  
  HyperBall(int x, int y, int R){
    super(x,y,R);
  }
  
  void disegna(){
    noStroke();
    fill(X);
    ellipse(x,y,R,R);
  }
  
  
  void move(){
    if(y >= width || y <= 0){
      sy=sy*-1;
    }
    y = y + sy;
    disegna();
  }
  
   void checkMouse() {
    float d = dist(mouseX, mouseY, x, y);
    if (d < R) {
      X = color(random(255), random(255), random(255)); // Cambia colore casuale
    }
  }
}
