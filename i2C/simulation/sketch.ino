/*
 * ============================================================================
 * KYAMBOGO UNIVERSITY - DEPARTMENT OF ELECTRICAL AND ELECTRONIC ENGINEERING
 * TETE 3102: MICROCONTROLLERS AND APPLICATIONS
 * I2C Communication Lab Simulation (Arduino Uno + LCD1602 + RTC + Logic Analyzer)
 * ============================================================================
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define LCD_ADDR 0x27
#define RTC_ADDR 0x68
#define POT_PIN  A0
#define LED_PIN  13

LiquidCrystal_I2C lcd(LCD_ADDR, 16, 2);

// BCD conversion helper routines
byte bcd2dec(byte val) {
  return (val >> 4) * 10 + (val & 0x0F);
}

byte dec2bcd(byte val) {
  return ((val / 10) << 4) | (val % 10);
}

// Set initial time to RTC (12:30:00)
void setRTCInitialTime() {
  Wire.beginTransmission(RTC_ADDR);
  Wire.write(0x00);         // Register 0x00 (Seconds)
  Wire.write(dec2bcd(0));   // 00 seconds
  Wire.write(dec2bcd(30));  // 30 minutes
  Wire.write(dec2bcd(12));  // 12 hours (24-hr format)
  Wire.endTransmission();
}

// Perform I2C Bus Scan (Task 2)
void runI2CBusScan() {
  Serial.println("\n--------------------------------------------");
  Serial.println("KYAMBOGO UNIVERSITY - TETE 3102 I2C SCANNER");
  Serial.println("Scanning bus addresses (0x08 to 0x77)...");
  Serial.println("--------------------------------------------");

  byte error, address;
  int nDevices = 0;

  for (address = 8; address < 120; address++) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0) {
      Serial.print("SUCCESS: Device detected at 7-bit 0x");
      if (address < 16) Serial.print("0");
      Serial.print(address, HEX);
      Serial.print("  [Write Byte: 0x");
      Serial.print(address << 1, HEX);
      Serial.print(", Read Byte: 0x");
      Serial.print((address << 1) | 1, HEX);
      Serial.print("] - ");
      
      if (address == 0x27 || address == 0x3F) {
        Serial.println("LCD1602 PCF8574 Backpack");
      } else if (address == 0x68) {
        Serial.println("DS1307/DS3231 Real-Time Clock");
      } else {
        Serial.println("Generic I2C Slave");
      }
      nDevices++;
    } else if (error == 4) {
      Serial.print("ERROR at address 0x");
      if (address < 16) Serial.print("0");
      Serial.println(address, HEX);
    }
  }

  if (nDevices == 0) {
    Serial.println("No I2C devices found on bus!\n");
  } else {
    Serial.print("Bus scan complete. Detected ");
    Serial.print(nDevices);
    Serial.println(" active device(s).\n");
  }
}

// Read time from DS3231/DS1307 with repeated START (Task 4)
bool readRTCTime(byte &h, byte &m, byte &s) {
  Wire.beginTransmission(RTC_ADDR);
  Wire.write(0x00); // Start at register 0x00
  if (Wire.endTransmission(false) != 0) { // repeated START (Sr)
    return false;
  }

  if (Wire.requestFrom((uint8_t)RTC_ADDR, (uint8_t)3) != 3) {
    return false;
  }

  s = bcd2dec(Wire.read() & 0x7F);
  m = bcd2dec(Wire.read() & 0x7F);
  h = bcd2dec(Wire.read() & 0x3F);
  return true;
}

// Dynamic Bargraph renderer on LCD line 2 (Task 7)
void renderBargraph(int potVal) {
  int numBars = map(potVal, 0, 1023, 0, 16);
  char barStr[17];
  for (int i = 0; i < 16; i++) {
    barStr[i] = (i < numBars) ? 0xFF : ' '; // 0xFF is solid rectangle in HD44780
  }
  barStr[16] = '\0';
  lcd.setCursor(0, 1);
  lcd.print(barStr);
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  Serial.begin(115200);
  Wire.begin();

  // 1. Run bus scanner to verify devices
  runI2CBusScan();

  // 2. Initialize LCD
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("KyU TETE 3102");
  lcd.setCursor(0, 1);
  lcd.print("I2C Bus Ready...");
  delay(1500);
  lcd.clear();

  // 3. Initialize RTC time
  setRTCInitialTime();
}

void loop() {
  static unsigned long lastTick = 0;
  static bool ledState = false;

  // Update every 500 ms
  if (millis() - lastTick >= 500) {
    lastTick = millis();

    // Toggle heartbeat LED
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState ? HIGH : LOW);

    // Read RTC time
    byte h, m, s;
    if (readRTCTime(h, m, s)) {
      char timeBuf[17];
      snprintf(timeBuf, sizeof(timeBuf), "TIME:  %02d:%02d:%02d", h, m, s);
      lcd.setCursor(0, 0);
      lcd.print(timeBuf);

      // Print telemetry to serial monitor
      Serial.print("RTC Time: ");
      Serial.print(timeBuf);
    } else {
      lcd.setCursor(0, 0);
      lcd.print("RTC COMM ERROR! ");
      Serial.println("RTC ERROR: No ACK received");
    }

    // Read potentiometer and update LCD line 2
    int pot = analogRead(POT_PIN);
    renderBargraph(pot);
    Serial.print(" | Pot A0: ");
    Serial.print(pot);
    Serial.print(" (");
    Serial.print(map(pot, 0, 1023, 0, 100));
    Serial.println("%)");
  }
}
