/*
 * Kyambogo University - Dept of Electrical & Electronic Engineering
 * TETE 3102 Microcontrollers and Applications
 * Task 4: STM32 Blue Pill Dual-Device Digital Clock (DS1307 RTC + LCD1602)
 * Authors: Group 4 (Mukama Robert, Nabudere Zerubabel, Nanfuka Shakirah, Kidega Wilkie Cephas)
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <RTClib.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);
RTC_DS1307 rtc;

void setup() {
  pinMode(PC13, OUTPUT);
  digitalWrite(PC13, HIGH);

  Serial.begin(115200);

  // Initialize STM32 Hardware I2C (PB6=SCL, PB7=SDA)
  Wire.setSDA(PB7);
  Wire.setSCL(PB6);
  Wire.begin();

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("KyU TETE 3102");
  lcd.setCursor(0, 1);
  lcd.print("RTC Clock Init..");
  delay(1500);

  if (!rtc.begin()) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("RTC Not Found!");
    Serial.println("RTC Not Found!");
    while (1);
  }

  // If RTC lost power or running first time, seed with lab timestamp
  if (!rtc.isrunning()) {
    rtc.adjust(DateTime(2026, 11, 2, 14, 30, 0));
  }

  lcd.clear();
}

void loop() {
  DateTime now = rtc.now();

  char timeStr[17];
  char dateStr[17];

  // Format: "TIME: 14:30:05  "
  snprintf(timeStr, sizeof(timeStr), "TIME:  %02d:%02d:%02d", 
           now.hour(), now.minute(), now.second());

  // Format: "DATE: 2026-11-02"
  snprintf(dateStr, sizeof(dateStr), "DATE: %04d-%02d-%02d", 
           now.year(), now.month(), now.day());

  lcd.setCursor(0, 0);
  lcd.print(timeStr);

  lcd.setCursor(0, 1);
  lcd.print(dateStr);

  // Serial debug logging
  Serial.print(dateStr);
  Serial.print(" ");
  Serial.println(timeStr);

  // Heartbeat LED flash on PC13
  digitalWrite(PC13, LOW);
  delay(100);
  digitalWrite(PC13, HIGH);
  delay(900);
}
