#include "LedControl.h"
#include <Wire.h>
#include "Countdown.h"

int btnPin  = 12;
int hitPin  = A0;
int losePin = A1;

int dataPin = 2;
int csPin   = 3;
int clkPin  = 4;
LedControl lc = LedControl(dataPin, clkPin, csPin);

const int MPU_addr = 0x68;
int16_t AcX, AcY, AcZ, Tmp, GyX, GyY, GyZ;

int xJoyPos;
int yJoyPos;

const int numReads = 4;
int readIndex;
int xReadings[numReads];
int yReadings[numReads];
long totalX;
long totalY;

long startTime;
long endTime;

int score;
int xTgtPos;
int yTgtPos;
bool btnDown = false;
bool gameStarted = false;


void setup() {
  pinMode(btnPin, INPUT_PULLUP);
  pinMode(hitPin, OUTPUT);
  pinMode(losePin, OUTPUT);
  
  lc.shutdown(0, false);
  lc.setIntensity(0, 5);
  lc.clearDisplay(0);
  
  randomSeed(analogRead(A2));

  Wire.begin();
  Wire.beginTransmission(MPU_addr);
  Wire.write(0x6B);
  Wire.write(0);
  Wire.endTransmission(true);
}


void loop() {
  
  if(!gameStarted && digitalRead(btnPin) == LOW) {
    startGame();
    btnDown = true;
  }
  
  if(gameStarted && digitalRead(btnPin) == LOW && !btnDown) {
    updateGame();
    btnDown = true;
  }

  if(gameStarted) {
    updateGameLed();
    //updateTimer();
  }
  

  if(digitalRead(btnPin) == HIGH) {
    btnDown = false;
  }
}

void updateGame() {
  if(xJoyPos == xTgtPos && yJoyPos == yTgtPos) {
    score++;
    digitalWrite(hitPin, HIGH);
    delay(250);
    digitalWrite(hitPin, LOW);
  } else {
    if(score) {
      score++;
    }
    digitalWrite(losePin, HIGH);
    delay(250);
    digitalWrite(losePin, LOW);
  }
  updateDot();
}

void updateGameLed() {
  updatePos();
  totalX -= xReadings[readIndex];
  xReadings[readIndex] = AcX;
  totalX += AcX;
  
  totalY -= yReadings[readIndex];
  yReadings[readIndex] = AcY;
  totalY += AcY;
  
  readIndex++;
  if(readIndex >= numReads) {
    readIndex = 0;
  }
  
  xJoyPos = map(totalX / numReads, -8192, 8191, 0, 8);
  yJoyPos = 7 - map(totalY / numReads, -8192, 8191, 0, 8);

  lc.clearDisplay(0);
  setTarget();
  lc.setLed(0, yJoyPos, xJoyPos, 1);
}


void startGame() {
  readIndex = 0;
  totalX = 0;
  totalY = 0;
  score = 0;
  gameStarted = true;
  
  updateDot();
  
  for(int i = 0; i < numReads; ++i) {
    updatePos();
    totalX += AcX;
    xReadings[i] = AcX;
    totalY += AcY;
    yReadings[i] = AcY;
  }

  setPicture(three);
  delay(1000);
  setPicture(two);
  delay(1000);
  setPicture(one);
  delay(1000);
  setPicture(go);
  delay(250);
  startTime = millis();
  endTime = startTime + 10000;
}

void updateDot() {
  xTgtPos = random(6) + 1;
  yTgtPos = random(6) + 1;
}

void setTarget() {
  lc.setLed(0, yTgtPos + 1, xTgtPos, 1);
  lc.setLed(0, yTgtPos - 1, xTgtPos, 1);
  lc.setLed(0, yTgtPos, xTgtPos + 1, 1);
  lc.setLed(0, yTgtPos, xTgtPos - 1, 1);
}

void setPicture(bool picture[][8]) {
  for(int row = 0; row < 8; ++row) {
    for(int col = 0; col < 8; ++col) {
      lc.setLed(0, row, col, picture[row][col]);
    }
  }
}

void updatePos() {
  Wire.beginTransmission(MPU_addr);
  Wire.write(0x3B);  // starting with register 0x3B (ACCEL_XOUT_H)
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_addr, 14, true);  // request a total of 14 registers
  AcX=Wire.read()<<8|Wire.read();  // 0x3B (ACCEL_XOUT_H) & 0x3C (ACCEL_XOUT_L)    
  AcY=Wire.read()<<8|Wire.read();  // 0x3D (ACCEL_YOUT_H) & 0x3E (ACCEL_YOUT_L)
  
  /*
  AcZ=Wire.read()<<8|Wire.read();  // 0x3F (ACCEL_ZOUT_H) & 0x40 (ACCEL_ZOUT_L)
  Tmp=Wire.read()<<8|Wire.read();  // 0x41 (TEMP_OUT_H) & 0x42 (TEMP_OUT_L)
  GyX=Wire.read()<<8|Wire.read();  // 0x43 (GYRO_XOUT_H) & 0x44 (GYRO_XOUT_L)
  GyY=Wire.read()<<8|Wire.read();  // 0x45 (GYRO_YOUT_H) & 0x46 (GYRO_YOUT_L)
  GyZ=Wire.read()<<8|Wire.read();  // 0x47 (GYRO_ZOUT_H) & 0x48 (GYRO_ZOUT_L)
  */
}

