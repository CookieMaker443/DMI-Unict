PImage I;
String url="https://media.laregione.ch/files/domains/laregione.ch/images/4by1/s_-homer-quando-va-essere-quel-pdcb.jpg";
int g=0;
void settings(){
  I=loadImage(url);
  size(I.width,I.height);
}
void draw(){
  image(I,0,0);
}

void keyTyped(){
  if (key=='+'){
    g++;
    I.filter(BLUR, g);
    println("Blur: "+g);
  }
  if (key=='r' || key=='R'){
    I=loadImage(url);
    g=0;
  }
  
}
  
