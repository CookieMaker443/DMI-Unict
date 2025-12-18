PImage S;
int x;
void setup(){
  size(1024,512);
  S= loadImage("lena.png");
  x=(int)random(0,100); 
  PImage P= wow(S,x);
  image(S,0,0);
  
}

void draw(){
}


void keyPressed(){
 if (key== 'r' || key=='R' ){
      
 } 
}

PImage wow(PImage I, int x){
  PImage R= I.copy();
  PImage S= R.get(0,0,256,512);
  PImage T= R.get(256,0,512,512); 
  PImage s=S.get(0,0,256,x); 
  PImage t;
  
 //R.filter(GRAY);
 // S.filter(GRAY);
  T.filter(GRAY);
  image(T,512,0);
  image(s,768,0);
  
  for(int i=R.width/2; i<R.pixels.length; i++); 
  
  
  //image(R,(3*width/4),0); 
  
  return R; 
}
