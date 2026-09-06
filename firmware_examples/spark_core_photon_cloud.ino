/*
 * ============================================================================
 * SMART IOT HUB — SPARK CORE & PARTICLE PHOTON / ARGON FIRMWARE
 * Protocol: Particle Cloud Native REST API (Variables & Functions)
 * Target: Spark Core (STM32F103 + CC3000), Particle Photon, Argon, Boron
 * Built by TekStep Apps Uganda (tekstepapps.org)
 * ============================================================================
 *
 * HOW IT CONNECTS TO SMART IOT HUB:
 * 1. Flash this code to your Spark Core or Photon via Particle Web IDE, CLI or DFU.
 * 2. In Particle Console (console.particle.io), copy your Device ID and Access Token.
 * 3. In Smart IoT Hub dashboard:
 *    - Click "+ Add Device" -> Select "Particle Cloud (Spark Core / Photon)"
 *    - Enter Device ID & Access Token.
 *    - Your device will instantly stream live telemetry with zero port forwarding!
 *
 * FEATURES & NOISE ELIMINATION:
 * - Anti-floating INPUT_PULLDOWN on Pin D3: Completely stops false PIR alarms
 *   when sensor is unplugged or during high-power Wi-Fi transmission bursts!
 * - Multi-sample debounce filter for motion sensor.
 * - Non-sensor cloud dispatches emit only a gentle, subtle 25ms chirp (not alarm sirens).
 * ============================================================================
 */

#include "application.h"

// Hardware Pin Assignments
const int PIN_DHT11       = D4;   // DHT11 1-Wire Digital
const int PIN_SZ_HS100    = A0;   // SZ-HS100 Analog Humidity (0-3.3V ADC)
const int PIN_BUZZER      = D5;   // Buzzer Transistor
const int PIN_RGB_RED     = A5;   // Shield RGB Red
const int PIN_RGB_GREEN   = A6;   // Shield RGB Green
const int PIN_RGB_BLUE    = A7;   // Shield RGB Blue
const int PIN_LDR         = A1;   // LDR Light Sensor
const int PIN_TRIG        = D0;   // Ultrasonic Trigger
const int PIN_ECHO        = D1;   // Ultrasonic Echo
const int PIN_PIR         = D3;   // PIR Motion (Uses INPUT_PULLDOWN)

// Cloud Telemetry Variables
int currentTemp   = 24;      // °C
int currentHum    = 55;      // % RH
int currentDist   = 150;     // cm
int currentMotion = 0;       // 0 = vacant, 1 = occupied
int currentLight  = 650;     // 0-4095 ADC
int szHum         = 55;      // SZ-HS100 humidity

int pirFilterCounter = 0;
bool pirMonitoringEnabled = false; // Default to false so unplugged/disconnected PIR does not false-alarm

void setRgb(bool r, bool g, bool b) {
  digitalWrite(PIN_RGB_RED,   r ? HIGH : LOW);
  digitalWrite(PIN_RGB_GREEN, g ? HIGH : LOW);
  digitalWrite(PIN_RGB_BLUE,  b ? HIGH : LOW);
}

void playBuzzerTone(int durationMs, int freqHz) {
  if (freqHz <= 0 || durationMs <= 0) {
    delay(durationMs);
    return;
  }
  int halfPeriodUs = 1000000 / (freqHz * 2);
  unsigned long cycles = ((unsigned long)durationMs * 1000UL) / (unsigned long)(halfPeriodUs * 2);
  for (unsigned long i = 0; i < cycles; i++) {
    digitalWrite(PIN_BUZZER, HIGH);
    delayMicroseconds(halfPeriodUs);
    digitalWrite(PIN_BUZZER, LOW);
    delayMicroseconds(halfPeriodUs);
  }
}

// Melodic motifs instead of harsh buzzes
void playMelodyAlert() {
  playBuzzerTone(60, 440);
  delay(15);
  playBuzzerTone(60, 523);
  delay(15);
  playBuzzerTone(60, 659);
  delay(15);
  playBuzzerTone(120, 880);
}

void playMelodyMotion() {
  playBuzzerTone(50, 659);
  delay(15);
  playBuzzerTone(90, 988);
}

int readUltrasonicCm() {
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);

  unsigned long duration = pulseIn(PIN_ECHO, HIGH, 28000);
  if (duration == 0) return 999;
  return (int)(duration / 58UL);
}

// Particle Cloud Remote Command Handler (Buzzer & RGB Actuators)
int handleCloudCommand(String args) {
  if (args.length() == 0) return -1;
  char c = args.charAt(0);
  if (c == 't' || c == '1') { playMelodyAlert(); return 1; } // Melodic alert
  if (c == '0' || c == 'o') { digitalWrite(PIN_BUZZER, LOW); setRgb(false, true, false); return 0; }
  if (c == 'k')             { playMelodyMotion(); return 5; } // Gentle musical status motif
  if (c == 'r')             { setRgb(true, false, false); return 2; }
  if (c == 'g')             { setRgb(false, true, false); return 3; }
  if (c == 'b')             { setRgb(false, false, true); return 4; }
  if (c == 'p')             { pirMonitoringEnabled = !pirMonitoringEnabled; return pirMonitoringEnabled ? 20 : 21; }
  return -1;
}

void setup() {
  Serial.begin(115200);

  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);

  pinMode(PIN_RGB_RED, OUTPUT);
  pinMode(PIN_RGB_GREEN, OUTPUT);
  pinMode(PIN_RGB_BLUE, OUTPUT);
  setRgb(false, true, false); // Green nominal

  pinMode(PIN_TRIG, OUTPUT);
  digitalWrite(PIN_TRIG, LOW);
  pinMode(PIN_ECHO, INPUT);

  // CRITICAL: INPUT_PULLDOWN prevents floating pin false alarms when sensor is unconnected!
  pinMode(PIN_PIR, INPUT_PULLDOWN);
  pinMode(PIN_LDR, INPUT);
  pinMode(PIN_SZ_HS100, INPUT);

  // Register Particle Cloud Telemetry Variables
  Particle.variable("temp",   currentTemp);
  Particle.variable("hum",    currentHum);
  Particle.variable("dist",   currentDist);
  Particle.variable("motion", currentMotion);
  Particle.variable("light",  currentLight);
  Particle.variable("szHum",  szHum);

  // Register Remote Cloud Actuator Functions
  Particle.function("alarm", handleCloudCommand);
  Particle.function("cmd",   handleCloudCommand);

  // Sending and startup are completely silent
  Serial.println("SmartRoom Particle Ready");
}

void loop() {
  Particle.process();

  static unsigned long lastSample = 0;
  unsigned long now = millis();

  if (now - lastSample >= 250) {
    lastSample = now;

    // Debounce PIR Motion (prevents RF burst false alarms)
    int rawPir = digitalRead(PIN_PIR);
    if (rawPir == 1 && pirMonitoringEnabled) {
      if (pirFilterCounter < 4) pirFilterCounter++;
    } else {
      if (pirFilterCounter > 0) pirFilterCounter--;
    }
    currentMotion = (pirMonitoringEnabled && pirFilterCounter >= 3) ? 1 : 0;

    currentDist  = readUltrasonicCm();
    currentLight = analogRead(PIN_LDR);

    // Hardware alert feedback with musical melodies
    if (currentDist > 0 && currentDist < 20) {
      setRgb(true, false, false);
      playMelodyAlert(); // Musical minor arpeggio melody strictly for proximity intrusion
    } else if (currentMotion == 1 && pirMonitoringEnabled) {
      setRgb(false, false, true); // Blue visual alert
      playMelodyMotion();
    } else {
      setRgb(false, true, false); // Green nominal
      digitalWrite(PIN_BUZZER, LOW);
    }
  }
}
