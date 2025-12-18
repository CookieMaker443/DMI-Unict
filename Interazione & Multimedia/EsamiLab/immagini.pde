PImage L;
PImage C; 
void setup(){
  size(1024,512); 
  L=loadImage("lena.png");
  C=L.copy(); 
  C.filter(GRAY);
}


void draw(){
  background(0);
  image(L,0,0);
  image(C,512,0);
}


void keyPressed(){
  if(key=='-'&& C.width>32 && C.height >32){
    C.resize(C.width/2, C.height/2);
    image(C,512,0);
  }
  if(key=='+' && C.width<512 && C.height <512){
    C.resize(C.width*2, C.height*2);
    image(C,512,0);
  }
}
