#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);
const byte RTC_ADDR = 0x68;
const int BUZZER_PIN = 9;
const int POT_PIN = A0;

byte bcd2dec(byte val) { return (val >> 4) * 10 + (val & 0x0F); }
byte dec2bcd(byte val) { return ((val / 10) << 4) | (val % 10); }

void setup() {
  pinMode(BUZZER_PIN, OUTPUT);
  Serial.begin(115200);
  Wire.begin();
  lcd.init();
  lcd.backlight();
  Serial.println("TIME,TEMP_C,THRESHOLD");
}

void loop() {
  Wire.beginTransmission(RTC_ADDR);
  Wire.write(0x00);
  Wire.endTransmission(false);
  Wire.requestFrom((uint8_t)RTC_ADDR, (uint8_t)3);

  byte s = bcd2dec(Wire.read() & 0x7F);
  byte m = bcd2dec(Wire.read() & 0x7F);
  byte h = bcd2dec(Wire.read() & 0x3F);

  float temp = 20.0 + (analogRead(POT_PIN) * 15.0 / 1023.0); // Simulated temp from pot
  float threshold = 28.0;

  char l1[17], l2[17];
  snprintf(l1, sizeof(l1), "TIME: %02d:%02d:%02d", h, m, s);
  snprintf(l2, sizeof(l2), "T:%4.1fC TH:%2.0fC", temp, threshold);
  lcd.setCursor(0, 0); lcd.print(l1);
  lcd.setCursor(0, 1); lcd.print(l2);

  // Alarm threshold trigger
  if (temp > threshold) {
    tone(BUZZER_PIN, 1500, 150);
  } else {
    noTone(BUZZER_PIN);
  }

  // Serial CSV stream for PC logger
  Serial.print(h); Serial.print(":"); Serial.print(m); Serial.print(":"); Serial.print(s);
  Serial.print(","); Serial.print(temp); Serial.print(","); Serial.println(threshold);
  delay(500);
}
