PImage img;

void setup() {
  size(1024, 512);
  img = loadImage("lena.jpg");  // Carica l'immagine
  img.filter(GRAY);  // Scala di grigi
  PImage result1 = hash(img, 100, 200);  
  PImage result2 = hash(img, (int)random(100), 200, 3);  

  image(result1, 0, 0, width/2, height);  // Mostra la prima immagine a sinistra
  image(result2, width/2, 0, width/2, height); // Mostra la seconda a destra
}

// Funzione hash base: aggiunge linee nere
PImage hash(PImage I, int h, int k) {
  PImage output = I.copy();
  output.loadPixels();
  
  for (int x = 0; x < output.width; x++) {
    output.pixels[h * output.width + x] = color(0);  // Riga h
    output.pixels[k * output.width + x] = color(0);  // Riga k
  }
  
  for (int y = 0; y < output.height; y++) {
    output.pixels[y * output.width + h] = color(0);  // Colonna h
    output.pixels[y * output.width + k] = color(0);  // Colonna k
  }

  output.updatePixels();
  return output;
}

// Funzione hash con operatore massimo
PImage hash(PImage I, int h, int k, int n) {
  PImage temp = maxFilter(I, n);  // Applica il filtro massimo
  return hash(temp, h, k);  // Applica il metodo hash
}

// Applica filtro massimo con finestra n × n
PImage maxFilter(PImage I, int n) {
  PImage output = I.copy();
  output.loadPixels();
  
  for (int y = 0; y < I.height; y++) {
    for (int x = 0; x < I.width; x++) {
      int maxVal = 0;
      
      // Scansiona la finestra n × n
      for (int j = -n/2; j <= n/2; j++) {
        for (int i = -n/2; i <= n/2; i++) {
          int nx = constrain(x + i, 0, I.width - 1);
          int ny = constrain(y + j, 0, I.height - 1);
          int index = ny * I.width + nx;
          maxVal = (int)max(maxVal, brightness(I.pixels[index]));
        }
      }
      
      output.pixels[y * I.width + x] = color(maxVal);
    }
  }
  
  output.updatePixels();
  return output;
}
