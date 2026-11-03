#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);
const byte RTC_ADDR = 0x68;

byte bcd2dec(byte val) { return (val >> 4) * 10 + (val & 0x0F); }
byte dec2bcd(byte val) { return ((val / 10) << 4) | (val % 10); }

void setRTC(byte h, byte m, byte s) {
  Wire.beginTransmission(RTC_ADDR);
  Wire.write(0x00);
  Wire.write(dec2bcd(s));
  Wire.write(dec2bcd(m));
  Wire.write(dec2bcd(h));
  Wire.endTransmission();
}

bool getRTC(byte &h, byte &m, byte &s) {
  Wire.beginTransmission(RTC_ADDR);
  Wire.write(0x00);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((uint8_t)RTC_ADDR, (uint8_t)3) != 3) return false;
  s = bcd2dec(Wire.read() & 0x7F);
  m = bcd2dec(Wire.read() & 0x7F);
  h = bcd2dec(Wire.read() & 0x3F);
  return true;
}

void setup() {
  Wire.begin();
  lcd.init();
  lcd.backlight();
  setRTC(10, 45, 0);
}

void loop() {
  byte h, m, s;
  if (getRTC(h, m, s)) {
    char l1[17], l2[17];
    snprintf(l1, sizeof(l1), "TIME: %02d:%02d:%02d", h, m, s);
    snprintf(l2, sizeof(l2), "TEMP: 24.50 C   ");
    lcd.setCursor(0, 0); lcd.print(l1);
    lcd.setCursor(0, 1); lcd.print(l2);
  } else {
    lcd.setCursor(0, 0); lcd.print("** RTC ERROR ** ");
    lcd.setCursor(0, 1); lcd.print("RETRYING...     ");
  }
  delay(1000);
}
