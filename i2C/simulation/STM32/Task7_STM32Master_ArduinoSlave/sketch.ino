/*
 * Kyambogo University - Dept of Electrical & Electronic Engineering
 * TETE 3102 Microcontrollers and Applications
 * Task 7: STM32 Master to Arduino Slave I2C Telemetry & Control
 * Authors: Group 4 (Mukama Robert, Nabudere Zerubabel, Nanfuka Shakirah, Kidega Wilkie Cephas)
 *
 * STM32 Master Logic:
 *  - Master queries Arduino Slave (Address 0x08) for 10-bit Potentiometer ADC reading.
 *  - Master formats telemetry onto LCD1602 (Address 0x27).
 *  - Master calculates LED status (Threshold > 512) and commands Slave/Onboard LED.
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define SLAVE_ADDR   0x08
#define STATUS_LED   PB0

LiquidCrystal_I2C lcd(0x27, 16, 2);

void setup() {
  pinMode(STATUS_LED, OUTPUT);
  digitalWrite(STATUS_LED, LOW);

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
  lcd.print("STM32 Master");
  lcd.setCursor(0, 1);
  lcd.print("I2C Telemetry...");
  delay(1500);
  lcd.clear();
}

void loop() {
  // Step 1: Request 2 bytes of ADC data from Arduino Slave (0x08)
  Wire.requestFrom(SLAVE_ADDR, 2);

  uint16_t adcVal = 0;
  if (Wire.available() >= 2) {
    uint8_t msb = Wire.read();
    uint8_t lsb = Wire.read();
    adcVal = (msb << 8) | lsb;
  }

  // Convert ADC value (0 - 1023) to Voltage (0.00V - 5.00V)
  float voltage = (adcVal * 5.0) / 1023.0;

  // Step 2: Send Command back to Arduino Slave (LED Control Byte)
  uint8_t ledCmd = (adcVal > 512) ? 0x01 : 0x00;
  Wire.beginTransmission(SLAVE_ADDR);
  Wire.write(ledCmd);
  Wire.endTransmission();

  // Control Master Status LED
  digitalWrite(STATUS_LED, ledCmd ? HIGH : LOW);

  // Step 3: Display Real-Time Telemetry on I2C LCD
  char line0[17];
  char line1[17];
  snprintf(line0, sizeof(line0), "ADC:%4u (M->S)", adcVal);
  snprintf(line1, sizeof(line1), "V:%4.2fV LED:%s", voltage, ledCmd ? "ON " : "OFF");

  lcd.setCursor(0, 0);
  lcd.print(line0);
  lcd.setCursor(0, 1);
  lcd.print(line1);

  // Serial Diagnostic Logging
  Serial.print("[STM32 MASTER] Recv ADC: ");
  Serial.print(adcVal);
  Serial.print(" | Volt: ");
  Serial.print(voltage);
  Serial.print("V | Slave CMD: ");
  Serial.println(ledCmd ? "HIGH" : "LOW");

  digitalWrite(PC13, !digitalRead(PC13));
  delay(400);
}
