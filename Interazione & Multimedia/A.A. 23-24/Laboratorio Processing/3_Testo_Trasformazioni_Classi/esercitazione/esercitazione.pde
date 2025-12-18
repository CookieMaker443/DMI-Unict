/********* VARIABLES *********/

// We control which screen is active by settings / updating
// gameScreen variable. We display the correct screen according
// to the value of this variable.
//
// 0: Initial Screen
// 1: Game Screen

int gameScreen = 0;
float ballX, ballY;
int ballSize = 20;
int ballColor = color(0);
float gravity = 1;
float ballSpeedVert = 0;
float ballSpeedHorizon = 10;
float airfriction = 0.0001;
float friction = 0.1;
color racketColor = color(0);
float racketWidth = 100;
float racketHeight = 10;
int racketBounceRate = 20;

/********* SETUP BLOCK *********/

void setup() {
  size(500, 500);
  ballX=width/4;
  ballY=height/5;
}


/********* DRAW BLOCK *********/

void draw() {
  // Display the contents of the current screen
  if (gameScreen == 0) {
    initScreen();
  } else if (gameScreen == 1) {
    gameScreen();
  } 
}


/********* SCREEN CONTENTS *********/

void initScreen() {
   background(0);
  textAlign(CENTER);
  text("Click to start", height/2, width/2);
}
void gameScreen() {
   background(255);
   drawBall();
   drawRacket();
   applyGravity();
   applyHorizontalSpeed();
   keepInScreen();
   racketBounce();
}



/********* INPUTS *********/

public void mousePressed() {
  // if we are on the initial screen when clicked, start the game
  if (gameScreen==0) {
    startGame();
  }
}


/********* OTHER FUNCTIONS *********/

// This method sets the necessary variables to start the game  
void startGame() {
  gameScreen=1;
}

void drawBall() {
  fill(ballColor);
  ellipse(ballX, ballY, ballSize, ballSize);
}

void applyGravity() {
  ballSpeedVert += gravity;
  ballY += ballSpeedVert;
  ballSpeedVert -= (ballSpeedVert * airfriction);
}
void makeBounceBottom(float surface) {
  ballY = surface-(ballSize/2);
  ballSpeedVert*=-1;
  ballSpeedVert -= (ballSpeedVert * friction);
}
void makeBounceTop(float surface) {
  ballY = surface+(ballSize/2);
  ballSpeedVert*=-1;
  ballSpeedVert -= (ballSpeedVert * friction);
}
// manitiene la palla nello schermo
void keepInScreen() {
  // la pallina colpisce la parte bassa
  if (ballY+(ballSize/2) > height) { 
    makeBounceBottom(height);
  }
   // la pallina colpisce la parte alta
  if (ballY-(ballSize/2) < 0) {
    makeBounceTop(0);
  }
  
  if (ballX-(ballSize/2) < 0){
    makeBounceLeft(0);
  }
  if (ballX+(ballSize/2) > width){
    makeBounceRight(width);
  }
}

void drawRacket(){
  fill(racketColor);
  rectMode(CENTER);
  rect(mouseX, mouseY, racketWidth, racketHeight);
}

void racketBounce() {
  float overhead = mouseY - pmouseY;
  if ((ballX+(ballSize/2) > mouseX-(racketWidth/2)) && (ballX-(ballSize/2) < mouseX+(racketWidth/2))) {
    if (dist(ballX, ballY, ballX, mouseY)<=(ballSize/2)+abs(overhead)) {
      makeBounceBottom(mouseY);
      // racket moving up
      if (overhead<0) {
        ballY+=overhead;
        ballSpeedVert+=overhead;
        ballSpeedHorizon = (ballX - mouseX)/5;
      }
    }
  }
}

void applyHorizontalSpeed(){
  ballX += ballSpeedHorizon;
  ballSpeedHorizon -= (ballSpeedHorizon * airfriction);
}
void makeBounceLeft(float surface){
  ballX = surface+(ballSize/2);
  ballSpeedHorizon*=-1;
  ballSpeedHorizon -= (ballSpeedHorizon * friction);
}
void makeBounceRight(float surface){
  ballX = surface-(ballSize/2);
  ballSpeedHorizon*=-1;
  ballSpeedHorizon -= (ballSpeedHorizon * friction);
}
