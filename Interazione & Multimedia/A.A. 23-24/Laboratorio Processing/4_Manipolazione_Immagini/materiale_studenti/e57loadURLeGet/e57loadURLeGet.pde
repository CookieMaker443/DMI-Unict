PImage I, H;
PImage subI;
String url;

void setup(){
  size(512,512);
  url = "https://media.laregione.ch/files/domains/laregione.ch/images/4by1/s_-homer-quando-va-essere-quel-pdcb.jpg";
  I=loadImage(url,"jpg");
  
  subI=I.get(I.width/2, height/2, 150, 150);
  I.filter(GRAY);  //-> Homer in toni di grigio
  imageMode(CENTER);
  I.resize(int(I.width/2.5), int(I.height/2.5));
  image(I,width/2,height/2);  
  image(subI, width/2, height/2);
  subI.save(savePath("HomerPorzione.png"));

}
void draw(){
  
}
