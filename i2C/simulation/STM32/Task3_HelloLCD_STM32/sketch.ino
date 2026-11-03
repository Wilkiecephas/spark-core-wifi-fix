/*
 * Kyambogo University - Dept of Electrical & Electronic Engineering
 * TETE 3102 Microcontrollers and Applications
 * Task 3: STM32 Blue Pill "Hello World" LCD1602 Display via I2C PCF8574
 * Authors: Group 4 (Mukama Robert, Nabudere Zerubabel, Nanfuka Shakirah, Kidega Wilkie Cephas)
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// I2C Backpack default address 0x27, 16 columns, 2 rows
LiquidCrystal_I2C lcd(0x27, 16, 2);

const char scrollMessage[] = "    Kyambogo University Dept of EEE - TETE 3102 Group 4    ";

void setup() {
  pinMode(PC13, OUTPUT);
  digitalWrite(PC13, HIGH);

  // Initialize STM32 Hardware I2C (PB6=SCL, PB7=SDA)
  Wire.setSDA(PB7);
  Wire.setSCL(PB6);
  Wire.begin();

  lcd.init();
  lcd.backlight();

  // Print Static Welcome Banner on Line 0
  lcd.setCursor(0, 0);
  lcd.print("Hello, Uganda!");

  // Line 1 Initial Label
  lcd.setCursor(0, 1);
  lcd.print("KyU EEE Group 4");
  delay(2500);
}

void loop() {
  int len = strlen(scrollMessage);
  char window[17];

  for (int i = 0; i <= len - 16; i++) {
    strncpy(window, scrollMessage + i, 16);
    window[16] = '\0';

    lcd.setCursor(0, 1);
    lcd.print(window);

    // Toggle onboard LED with every scroll step
    digitalWrite(PC13, !digitalRead(PC13));
    delay(250);
  }
}
