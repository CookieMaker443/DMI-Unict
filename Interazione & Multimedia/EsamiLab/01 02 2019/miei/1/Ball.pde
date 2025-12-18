class Ball {
  int x,y;
  int R;
  int sy =5;
  
  Ball(int x, int y, int R){
    this.x = x;
    this.y = y;
    this.R = R;
  }
  
  void disegna(){
    noStroke();
    fill(255,0,0);
    ellipse(x,y,R,R);
  }
  
  
  void move(){
    if(y >= width || y <= 0){
      sy=sy*-1;
    }
    y = y + sy;
    disegna();
  }
}
