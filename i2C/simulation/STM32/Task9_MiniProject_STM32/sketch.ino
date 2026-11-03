/*
 * Kyambogo University - Dept of Electrical & Electronic Engineering
 * TETE 3102 Microcontrollers and Applications
 * Task 9: STM32 Blue Pill Autonomous Environmental & Time Station
 * Peripherals: DS1307 RTC, PCF8574 LCD1602, Analog Sensor (A0), Alarm LED (B0)
 * Authors: Group 4 (Mukama Robert, Nabudere Zerubabel, Nanfuka Shakirah, Kidega Wilkie Cephas)
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <RTClib.h>

#define SENSOR_PIN   A0
#define ALARM_PIN    PB0
#define ALARM_THRESH 600

LiquidCrystal_I2C lcd(0x27, 16, 2);
RTC_DS1307 rtc;

void setup() {
  pinMode(ALARM_PIN, OUTPUT);
  digitalWrite(ALARM_PIN, LOW);
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
  lcd.print("KyU Mini-Project");
  lcd.setCursor(0, 1);
  lcd.print("Group 4 Monitor");
  delay(1800);

  if (!rtc.begin()) {
    lcd.clear();
    lcd.print("RTC ERROR");
    while (1);
  }

  if (!rtc.isrunning()) {
    rtc.adjust(DateTime(2026, 11, 2, 16, 0, 0));
  }

  lcd.clear();
}

void loop() {
  DateTime now = rtc.now();
  int rawVal = analogRead(SENSOR_PIN);
  float voltage = (rawVal * 3.3) / 4095.0; // STM32 12-bit ADC (3.3V ref)
  bool alarmActive = (rawVal > ALARM_THRESH);

  digitalWrite(ALARM_PIN, alarmActive ? HIGH : LOW);

  // Line 0: Timestamp
  char tBuf[17];
  snprintf(tBuf, sizeof(tBuf), "%02d:%02d:%02d KyU EEE", now.hour(), now.minute(), now.second());
  lcd.setCursor(0, 0);
  lcd.print(tBuf);

  // Line 1: Telemetry & Status
  char sBuf[17];
  snprintf(sBuf, sizeof(sBuf), "A0:%4d %s", rawVal, alarmActive ? "!ALARM!" : "NORMAL ");
  lcd.setCursor(0, 1);
  lcd.print(sBuf);

  // Serial CSV stream
  Serial.print(now.timestamp(DateTime::TIMESTAMP_TIME));
  Serial.print(",");
  Serial.print(rawVal);
  Serial.print(",");
  Serial.print(voltage, 2);
  Serial.print(",");
  Serial.println(alarmActive ? "1" : "0");

  digitalWrite(PC13, !digitalRead(PC13));
  delay(500);
}
