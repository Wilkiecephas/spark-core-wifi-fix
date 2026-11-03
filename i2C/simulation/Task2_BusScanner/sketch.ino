#include <Wire.h>

void setup() {
  Serial.begin(115200);
  Wire.begin();
  Serial.println("\n--- Kyambogo University TETE 3102 ---");
  Serial.println("Task 2: I2C Bus Scanner");
  Serial.println("Scanning addresses 0x08 to 0x77...");

  byte count = 0;
  for (byte addr = 8; addr < 120; addr++) {
    Wire.beginTransmission(addr);
    byte err = Wire.endTransmission();
    if (err == 0) {
      Serial.print("Found Device: 7-bit: 0x");
      if (addr < 16) Serial.print("0");
      Serial.print(addr, HEX);
      Serial.print("  [Write: 0x");
      Serial.print(addr << 1, HEX);
      Serial.print(", Read: 0x");
      Serial.print((addr << 1) | 1, HEX);
      Serial.print("] - ");
      if (addr == 0x27) Serial.println("LCD1602 PCF8574");
      else if (addr == 0x68) Serial.println("DS1307/DS3231 RTC");
      else Serial.println("Unknown Device");
      count++;
    }
  }
  Serial.print("Scan complete. Total devices: ");
  Serial.println(count);
}

void loop() { }
