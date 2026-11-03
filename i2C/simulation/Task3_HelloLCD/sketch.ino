#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);
const char scrollMsg[] = "    Kyambogo University Dept of EEE - TETE 3102 Group 4    ";

void setup() {
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Hello, Uganda");
  lcd.setCursor(0, 1);
  lcd.print("Group 4 EEE");
  delay(2500);
}

void loop() {
  int len = strlen(scrollMsg);
  char buf[17];
  for (int i = 0; i <= len - 16; i++) {
    strncpy(buf, scrollMsg + i, 16);
    buf[16] = '\0';
    lcd.setCursor(0, 1);
    lcd.print(buf);
    delay(250);
  }
}
