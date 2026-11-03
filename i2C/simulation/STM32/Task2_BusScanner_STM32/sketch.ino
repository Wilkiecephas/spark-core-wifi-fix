/*
 * Kyambogo University - Dept of Electrical & Electronic Engineering
 * TETE 3102 Microcontrollers and Applications
 * Task 2: STM32 Blue Pill I2C Bus Scanner
 * Authors: Group 4 (Mukama Robert, Nabudere Zerubabel, Nanfuka Shakirah, Kidega Wilkie Cephas)
 */

#include <Wire.h>

void setup() {
  // Pin PC13 is active-low user LED on STM32 Blue Pill
  pinMode(PC13, OUTPUT);
  digitalWrite(PC13, HIGH);

  Serial.begin(115200);
  while (!Serial && millis() < 3000); // Wait for USB CDC / Serial

  // Configure STM32 Blue Pill Hardware I2C (I2C1: PB6=SCL, PB7=SDA)
  Wire.setSDA(PB7);
  Wire.setSCL(PB6);
  Wire.begin();

  Serial.println("\n==========================================");
  Serial.println("  KYAMBOGO UNIVERSITY - TETE 3102 LAB");
  Serial.println("  Task 2: STM32 I2C Bus Scanner (Group 4)");
  Serial.println("==========================================\n");
}

void loop() {
  byte error, address;
  int nDevices = 0;

  Serial.println("Starting I2C bus scan [0x01 - 0x7F]...");
  digitalWrite(PC13, LOW); // LED ON during scan

  for (address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0) {
      Serial.print("  [+] I2C device detected at 7-bit address 0x");
      if (address < 16) Serial.print("0");
      Serial.print(address, HEX);

      if (address == 0x27 || address == 0x3F) {
        Serial.print(" -> PCF8574 LCD1602 Backpack");
      } else if (address == 0x68) {
        Serial.print(" -> DS1307 / DS3231 Real-Time Clock");
      } else if (address == 0x08) {
        Serial.print(" -> Secondary Slave Microcontroller");
      } else if (address == 0x3C || address == 0x3D) {
        Serial.print(" -> SSD1306 OLED Display");
      }
      Serial.println();
      nDevices++;
    } else if (error == 4) {
      Serial.print("  [!] Unknown error at address 0x");
      if (address < 16) Serial.print("0");
      Serial.println(address, HEX);
    }
  }

  if (nDevices == 0) {
    Serial.println("  [-] No I2C devices found. Check pull-up resistors & wiring.\n");
  } else {
    Serial.print("Scan complete! Found ");
    Serial.print(nDevices);
    Serial.println(" active device(s) on bus.\n");
  }

  digitalWrite(PC13, HIGH); // LED OFF
  delay(5000);
}
