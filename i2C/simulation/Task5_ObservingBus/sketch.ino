#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

void setup() {
  Wire.begin();
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Capturing I2C...");
}

void loop() {
  // Transmit byte 0x55 to address 0x48 periodically for scope/logic analyzer analysis
  Wire.beginTransmission(0x48);
  Wire.write(0x55);
  Wire.endTransmission();
  delay(200);
}
