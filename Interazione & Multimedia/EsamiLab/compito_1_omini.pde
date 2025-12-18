int x, y, d, posx ,vel;
Bianchi s;
ArrayList<Bianchi> b;

void setup(){
  size(800,500); 
  x=(int)random(30,width-30);
  y=(int)random(30, height-30);
  d=20;
  posx=1; 
  vel=2;
  b = new ArrayList <Bianchi>();
  //Bianchi b= new Bianchi(x,y,d,posx); 
}


void draw(){
 background(0);

 for( Bianchi s: b){
  s.run();
  }
  
}

void keyPressed(){
  
  if (key== 'a'|| key== 'A'){
     x=(int)random(30,width-30)+vel;
     y=(int)random(30, height-30)+vel;
    b.add(new Bianchi(x,y,d,posx,vel));
  }
  
  if (key== 'r'|| key=='R'){
     x=(int)random(30,width-30)+vel;
     y=(int)random(30, height-30)+vel;
    b.add(new Rossi(x,y,d,posx,4));
  }
  
  if(key=='c' || key=='C'){
      fill(0);
      rect(0,0,width,height);
      noLoop(); 
    }
  if (key=='s' || key== 'S'){
    loop();
  }
  
}
