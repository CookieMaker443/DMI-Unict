class AmazingBlock extends Block{
  
  float angle;
  color rgb = color(angle%256, 255-angle%256, 255);
  
  AmazingBlock(int x,int y, int w, int h){
    super(x,y,w,h);
    angle = random(0,360);
  }
  
  void Disegna(){
    pushMatrix();
    translate(x,y);
    rotate(radians(angle));
    
    int r = (int)angle % 256;
    int g = 256 - r;
    fill(r,g,255);
    
    noStroke();
    rectMode(CENTER);
    rect(0,0,w,h);   
    popMatrix();
  }
  
    void display() {
    //pushMatrix();
    translate(x, y);
    rotate(radians(angle));
    
    int r = int(angle) % 256;
    int g = 255 - r;
    fill(r, g, 255);
    
    noStroke();
    rectMode(CENTER);
    rect(0, 0, w, h);
    //popMatrix();
  }
  
  @Override
  void Move(){
    if(x <= 0 && sx<0){
      sx=sx*-1;
    }
    
    if(x >= width && sx>0){
      sx=sx*-1;
    }
   x = x + sx;
   Disegna();
    angle+=5;
  }
}
