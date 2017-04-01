#include "LedControl.h"
#include <Wire.h>
#include "Countdown.h"

/*
 * digits : holds values of shift register to write digits 0 - 9
 *            to seven segment display
 *          Assumes the following connections; refer to manuals
 *          Q0 connects to A
 *          Q1 connects to B
 *          ................
 *          Q7 connects to DP
 */
const byte digits[] = 
{B11111100, B01100000, B11011010, B11110010, B01100110,
 B10110110, B10111110, B11100000, B11111110, B11100110};

/*
 * digitPins    : holds pins connected to common anode pins on seven segment display
 *                pin numbers are sorted from Most to Least significant
 * regDataPin   : data pin for shift register
 * latchPin     : latch pin for shift register
 * regClockPin  : clock pin for shift register
 */
int digitPins[] = {8, 9, 10, 11};
int regDataPin  = 5;
int latchPin    = 6;
int regClockPin = 7;

/*
 * btnPin   : button input pin
 * hitPin   : LED for a correct hit
 * losePin  : LED for an incorrect hit
 */
int btnPin  = 12;
int hitPin  = A0;
int losePin = A1;

/*
 * ledDataPin   : Data pin for LED Matrix
 * csPin        : CS pin for LED Matrix
 * ledClockPin  : Clock pin for LED Matrix
 */
int ledDataPin  = 2;
int csPin       = 3;
int ledClockPin = 4;
LedControl lc = LedControl(ledDataPin, ledClockPin, csPin);

/*
 * MPU_addr       : I2C address of the IMU
 * AcX, AcY, AcZ  : Accelerometer components
 * Tmp            : Temperature
 * GyX, GyY, GyZ  : Gyroscope components
 */
const int MPU_addr = 0x68;
int16_t AcX, AcY, AcZ, Tmp, GyX, GyY, GyZ;

/*
 * xJoyPos  : X position of the player on the LED matrix
 * yJoyPos  : Y position of the player on the LED matrix
 */
int xJoyPos;
int yJoyPos;

/*
 * numReads   : number of inputs to collect for input smoothing
 * readIndex  : allows tracking of reading history
 * xReadings  : history of x sensor values 
 * yReadings  : history of y sensor values
 * totalX     : total of x values to average
 * totalY     : total of y values to average
 */
const int numReads = 4;
int readIndex;
int xReadings[numReads];
int yReadings[numReads];
long totalX;
long totalY;

/*
 * endTime  : time, in milliseconds since program start, that the game ends
 * score    : player score
 * xTgtPos  : X position of the target on the LED matrix
 * yTgtPos  : Y position of the target on the LED matrix
 * btnDown  : prevents button spam
 */
long endTime;
int score;
int xTgtPos;
int yTgtPos;
bool btnDown = false;

int digitIndex = 0;

void setup() {
  pinMode(btnPin, INPUT_PULLUP);
  pinMode(hitPin, OUTPUT);
  pinMode(losePin, OUTPUT);
  
  pinMode(digitPins[0], OUTPUT);
  pinMode(digitPins[1], OUTPUT);
  pinMode(digitPins[2], OUTPUT);
  pinMode(digitPins[3], OUTPUT);
  digitalWrite(digitPins[0], HIGH);
  digitalWrite(digitPins[1], HIGH);
  digitalWrite(digitPins[2], HIGH);
  digitalWrite(digitPins[3], HIGH);

  pinMode(regDataPin, OUTPUT);
  pinMode(latchPin, OUTPUT);
  pinMode(regClockPin, OUTPUT);
  shiftOut(regDataPin, regClockPin, LSBFIRST, 0);
  
  lc.shutdown(0, false);
  lc.setIntensity(0, 2);
  lc.clearDisplay(0);
  
  randomSeed(analogRead(A2));

  Wire.begin();
  Wire.beginTransmission(MPU_addr);
  Wire.write(0x6B);
  Wire.write(0);
  Wire.endTransmission(true);

  while(digitalRead(btnPin) == HIGH);
  startGame();
}

// ***** MAIN LOOP *****
void loop() {
  // Registered a button press, should probably use interrupts
  if(digitalRead(btnPin) == LOW && !btnDown) {
    updateGame();
    btnDown = true;
  }

  // Update the position of the player
  updateGameLed();
  
  // Update timer and score counter
  printNum((endTime - millis()) / 1000 * 100 + score + 100);

  // End the game
  if(millis() > endTime) {
    digitalWrite(digitPins[digitIndex], HIGH);
    lc.clearDisplay(0);
    while(digitalRead(btnPin) == LOW || endTime + 1000 > millis()) {
      printNum(score);
    }
    while(digitalRead(btnPin) == HIGH) {
      printNum(score);
    }
    //while(digitalRead(btnPin) == LOW);
    startGame();
  }
  
  if(digitalRead(btnPin) == HIGH) {
    btnDown = false;
  }
}

/*
 * startGame
 *  This function performs the necessary actions to start the game
 */
void startGame() {
  // Initialize default values
  readIndex = 0;
  totalX = 0;
  totalY = 0;
  score = 0;

  // update target position
  updateDot();

  // turn off seven segment LEDs
  for(int i = 0; i < 4; ++i) {
    digitalWrite(digitPins[i], HIGH);
  }

  // seed position smoother with values
  for(int i = 0; i < numReads; ++i) {
    updatePos();
    totalX += AcX;
    xReadings[i] = AcX;
    totalY += AcY;
    yReadings[i] = AcY;
  }

  // start countdown
  setPicture(three);
  delay(1000);
  setPicture(two);
  delay(1000);
  setPicture(one);
  delay(1000);
  setPicture(go);
  delay(250);
  //set endTime
  endTime = millis() + 10000;
}


/*
 * updateGame
 *  This function should be called when a button press is registered
 */
 void updateGame() {
  long hitTime = millis();

  // test for a correct hit, then adjust score and set respective LED
  if(xJoyPos == xTgtPos && yJoyPos == yTgtPos) {
    score++;
    digitalWrite(hitPin, HIGH);
  } else {
    if(score > 0) {
      score--;
    }
    digitalWrite(losePin, HIGH);
  }

  // keep updating the timer and score
  while(millis() - hitTime < 100) {
    printNum((endTime - millis()) / 1000 * 100 + score + 100);
  }
  digitalWrite(hitPin, LOW);
  digitalWrite(losePin, LOW);

  // get a new target
  updateDot();
}

/*
 * updateGameLed
 *  This function updates the player's cursor on the LED matrix
 */

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


/*
 * updateDot
 *  This function gets a new position for the target
 */
void updateDot() {
  xTgtPos = random(6) + 1;
  yTgtPos = random(6) + 1;
}

/*
 * setTarget
 *  This function sets the LEDs for the target
 */
void setTarget() {
  lc.setLed(0, yTgtPos + 1, xTgtPos, 1);
  lc.setLed(0, yTgtPos - 1, xTgtPos, 1);
  lc.setLed(0, yTgtPos, xTgtPos + 1, 1);
  lc.setLed(0, yTgtPos, xTgtPos - 1, 1);
}

/*
 * setPicture
 *  This function sets the LED matrix to equal a 8x8 bool array
 */
void setPicture(bool picture[][8]) {
  for(int row = 0; row < 8; ++row) {
    for(int col = 0; col < 8; ++col) {
      lc.setLed(0, row, col, picture[row][col]);
    }
  }
}

/*
 * printNum
 *  This function prints up to 4 decimal places of an integer to a seven segment display
 */
void printNum(int num) {
  int nextDigitIndex = (digitIndex >= 3) ? 0 : digitIndex + 1;
  digitalWrite(latchPin, LOW);
  for(int i = 0; i < 3 - nextDigitIndex; i++) {
    num /= 10;
  }
  shiftOut(regDataPin, regClockPin, LSBFIRST, digits[num % 10]);
  digitalWrite(digitPins[digitIndex], HIGH);
  digitalWrite(latchPin, HIGH);
  digitalWrite(digitPins[nextDigitIndex], LOW);
  digitIndex = nextDigitIndex;
}

/*
 * updatePos
 *  This function polls the IMU for new sensor values
 */
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

