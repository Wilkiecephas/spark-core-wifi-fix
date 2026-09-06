/*
 * ============================================================================
 * SMART IOT HUB — ARDUINO UNO / NANO / MEGA TELEMETRY FIRMWARE
 * Protocol: High-Speed WebSerial UART (115200 Baud)
 * Compatible with: Arduino Uno R3, Nano V3, Mega 2560, Leonardo, STM32 Blue Pill
 * Built by TekStep Apps Uganda (tekstepapps.org)
 * ============================================================================
 *
 * HOW IT CONNECTS TO SMART IOT HUB:
 * 1. Connect Arduino to your computer via USB cable.
 * 2. Upload this sketch using the Arduino IDE.
 * 3. Open the Smart IoT Hub web dashboard in Google Chrome / Edge / Opera.
 * 4. Click "Connect USB COM Port" (or "Connect Hardware" -> WebSerial).
 * 5. Select your Arduino's COM port. Live telemetry streams instantly at 3.3 pkt/s!
 *
 * WIRING PINOUT GUIDE:
 * - HC-SR04 Ultrasonic:  VCC -> 5V, GND -> GND, TRIG -> D7, ECHO -> D8
 * - HC-SR501 PIR Motion: VCC -> 5V, GND -> GND, OUT  -> D3 (Digital)
 * - LDR Light Sensor:    Analog Pin A1 (with 10k pull-down to GND)
 * - LM35 Temp / Pot:     Analog Pin A2 (LM35 VOUT) or A0 (Potentiometer)
 * - Piezo Buzzer:        D5 (Transistor / PWM)
 * - Tricolor RGB LED:    Red -> D9, Green -> D10, Blue -> D11
 * ============================================================================
 */

// Pin Assignments
const int PIN_PIR       = 3;   // PIR motion digital input
const int PIN_BUZZER    = 5;   // Piezo alarm buzzer
const int PIN_US_TRIG   = 7;   // HC-SR04 trigger
const int PIN_US_ECHO   = 8;   // HC-SR04 echo
const int PIN_RGB_RED   = 9;   // RGB LED Red
const int PIN_RGB_GREEN = 10;  // RGB LED Green
const int PIN_RGB_BLUE  = 11;  // RGB LED Blue
const int PIN_LDR       = A1;  // LDR photoresistor ADC
const int PIN_LM35      = A2;  // LM35 Centigrade Temp ADC

// Telemetry State
float currentTemp = 24.5;
float currentHum  = 55.0;
float currentDist = 150.0;
int   currentMotion = 0;
int   currentLight  = 650;

// PIR debounce counter to eliminate false alarms
int pirFilterCounter = 0;

void setRgbColor(bool r, bool g, bool b) {
  digitalWrite(PIN_RGB_RED,   r ? HIGH : LOW);
  digitalWrite(PIN_RGB_GREEN, g ? HIGH : LOW);
  digitalWrite(PIN_RGB_BLUE,  b ? HIGH : LOW);
}

void triggerBeep(int freqHz, int durationMs) {
  int halfPeriodUs = 1000000 / (freqHz * 2);
  unsigned long cycles = ((unsigned long)durationMs * 1000UL) / (unsigned long)(halfPeriodUs * 2);
  for (unsigned long i = 0; i < cycles; i++) {
    digitalWrite(PIN_BUZZER, HIGH);
    delayMicroseconds(halfPeriodUs);
    digitalWrite(PIN_BUZZER, LOW);
    delayMicroseconds(halfPeriodUs);
  }
}

float readUltrasonicCm() {
  digitalWrite(PIN_US_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_US_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_US_TRIG, LOW);

  unsigned long duration = pulseIn(PIN_US_ECHO, HIGH, 28000); // 28ms timeout (~480cm)
  if (duration == 0) return 999.0;
  return (float)(duration / 58.0);
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000); // Wait for native USB if Leonardo/SAMD

  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);

  pinMode(PIN_RGB_RED, OUTPUT);
  pinMode(PIN_RGB_GREEN, OUTPUT);
  pinMode(PIN_RGB_BLUE, OUTPUT);
  setRgbColor(false, true, false); // Green nominal

  pinMode(PIN_US_TRIG, OUTPUT);
  digitalWrite(PIN_US_TRIG, LOW);
  pinMode(PIN_US_ECHO, INPUT);

  // Use INPUT_PULLUP or stable digital input for PIR
  pinMode(PIN_PIR, INPUT);

  // Startup beep
  triggerBeep(2400, 60);

  Serial.println(F("{\"status\":\"ready\",\"board\":\"arduino_uno\",\"baud\":115200}"));
}

void loop() {
  // 1. Sample Ultrasonic Distance
  currentDist = readUltrasonicCm();

  // 2. Debounce PIR Motion (prevents noise spikes)
  int rawPir = digitalRead(PIN_PIR);
  if (rawPir == HIGH) {
    if (pirFilterCounter < 4) pirFilterCounter++;
  } else {
    if (pirFilterCounter > 0) pirFilterCounter--;
  }
  currentMotion = (pirFilterCounter >= 3) ? 1 : 0;

  // 3. Sample Light & Temperature
  currentLight = analogRead(PIN_LDR);
  int rawLm35 = analogRead(PIN_LM35);
  // LM35: 10 mV / °C. At 5.0V ADC: (ADC / 1024.0) * 500.0
  currentTemp = (rawLm35 * 5.0 / 1024.0) * 100.0;
  if (currentTemp < 0.0 || currentTemp > 85.0) currentTemp = 24.5;

  // 4. Local Hardware Security Check
  if (currentDist > 0 && currentDist < 20.0) {
    // Proximity Breach: Red LED + Alarm sound
    setRgbColor(true, false, false);
    triggerBeep(2200, 40);
  } else if (currentMotion == 1) {
    // Motion: Blue LED
    setRgbColor(false, false, true);
  } else {
    // Nominal: Green LED
    setRgbColor(false, true, false);
  }

  // 5. Transmit Standardized JSON Telemetry to Smart IoT Hub
  Serial.print(F("{\"temp\":"));
  Serial.print(currentTemp, 1);
  Serial.print(F(",\"hum\":"));
  Serial.print(currentHum, 1);
  Serial.print(F(",\"dist\":"));
  Serial.print(currentDist, 1);
  Serial.print(F(",\"motion\":"));
  Serial.print(currentMotion);
  Serial.print(F(",\"light\":"));
  Serial.print(currentLight);
  Serial.println(F("}"));

  // 6. Handle Incoming Commands from IoT Hub (Buzzer & RGB Control)
  if (Serial.available() > 0) {
    char cmd = Serial.read();
    if (cmd == '1' || cmd == 't') {
      triggerBeep(2400, 150); // Buzzer test
    } else if (cmd == '0') {
      digitalWrite(PIN_BUZZER, LOW);
      setRgbColor(false, true, false);
    } else if (cmd == 'r') {
      setRgbColor(true, false, false);
    } else if (cmd == 'g') {
      setRgbColor(false, true, false);
    } else if (cmd == 'b') {
      setRgbColor(false, false, true);
    }
  }

  delay(300); // 3.3 telemetry frames per second
}
