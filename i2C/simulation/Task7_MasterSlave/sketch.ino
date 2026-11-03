#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

void renderBargraph(uint8_t val) {
  int numBars = map(val, 0, 255, 0, 16);
  char bar[17];
  for (int i = 0; i < 16; i++) {
    bar[i] = (i < numBars) ? 0xFF : ' ';
  }
  bar[16] = '\0';
  lcd.setCursor(0, 1);
  lcd.print(bar);
}

void setup() {
  pinMode(13, OUTPUT);
  Serial.begin(115200);
  Wire.begin();
  lcd.init();
  lcd.backlight();
}

void loop() {
  static bool ledState = false;
  ledState = !ledState;
  digitalWrite(13, ledState ? HIGH : LOW);

  uint8_t potVal = analogRead(A0) >> 2;
  char line1[17];
  snprintf(line1, sizeof(line1), "POT VAL: %3d/255", potVal);
  lcd.setCursor(0, 0);
  lcd.print(line1);
  renderBargraph(potVal);
  delay(200);
}
