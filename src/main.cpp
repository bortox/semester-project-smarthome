#include "Arduino.h"
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <light.h>

const int ledPin = 3; // Pin PWM collegato al transistor
LiquidCrystal_I2C lcd(0x27, 16, 2); // I2C address 0x27, 16 columns, 2 rows


void setupLCD() {
  lcd.init(); // Initialize the LCD
  lcd.backlight(); // Turn on the backlight
  lcd.clear(); // Clear the LCD
  lcd.setCursor(0, 0);
  lcd.print("test led transistor");
}


void setup() {
  setupLCD();
  SimpleLight("Diocane",3)
}



void loop() {
  //Accendi gradualmente il LED RGB
  for (int brightness = 0; brightness <= 255; brightness++) {
    analogWrite(ledPin, brightness);
    delay(10);
  }

  delay(250);

  // Spegni gradualmente il LED RGB
  for (int brightness = 255; brightness >= 0; brightness--) {
    analogWrite(ledPin, brightness);
    delay(10);
  }
}
