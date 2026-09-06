/*
 * ============================================================================
 * SMART IOT HUB — ESP32 WI-FI REST TELEMETRY CLIENT
 * Protocol: Wi-Fi HTTP POST (JSON Telemetry Relay)
 * Target: ESP32 NodeMCU, WROOM-32, ESP32-S3, ESP32-C3
 * Built by TekStep Apps Uganda (tekstepapps.org)
 * ============================================================================
 *
 * HOW IT CONNECTS TO SMART IOT HUB:
 * 1. Enter your Wi-Fi credentials in WIFI_SSID and WIFI_PASS below.
 * 2. Set TELEMETRY_URL to your IoT Hub endpoint:
 *    - If running locally: "http://<YOUR_COMPUTER_IP>:5173/api/telemetry?device=dev_esp32_01"
 *    - If deployed on Vercel: "https://<YOUR_APP>.vercel.app/api/telemetry?device=dev_esp32_01"
 * 3. Upload to your ESP32 in the Arduino IDE (Requires ArduinoJson library).
 * 4. Open the Smart IoT Hub -> In the Blank Workspace or "+ Add Device":
 *    Select "Custom REST / HTTP" and enter your device ID or endpoint!
 *
 * WIRING PINOUT GUIDE (ESP32 3.3V Logic):
 * - HC-SR04 Trigger: GPIO 18
 * - HC-SR04 Echo:    GPIO 19 (via voltage divider or 3.3V compatible sensor)
 * - HC-SR501 PIR:    GPIO 13 (Digital IN)
 * - LDR Sensor:      GPIO 34 (Analog ADC1)
 * - LM35 Temp:       GPIO 35 (Analog ADC1)
 * - Buzzer:          GPIO 5  (PWM / Digital OUT)
 * - RGB LED:         R: GPIO 25, G: GPIO 26, B: GPIO 27
 * ============================================================================
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// =================== CONFIGURATION ===================
const char* WIFI_SSID     = "YOUR_WIFI_SSID";
const char* WIFI_PASS     = "YOUR_WIFI_PASSWORD";
const char* TELEMETRY_URL = "http://192.168.1.100:5173/api/telemetry?device=dev_esp32_node";
const char* DEVICE_ID     = "dev_esp32_node";
const unsigned long SEND_INTERVAL_MS = 2000; // Send telemetry every 2 seconds
// =====================================================

// Pin Definitions
#define PIN_US_TRIG   18
#define PIN_US_ECHO   19
#define PIN_PIR       13
#define PIN_BUZZER    5
#define PIN_RGB_R     25
#define PIN_RGB_G     26
#define PIN_RGB_B     27
#define PIN_LDR       34
#define PIN_LM35      35

float currentTemp = 24.0;
float currentHum  = 55.0;
float currentDist = 150.0;
int   currentMotion = 0;
int   currentLight  = 650;
int   pirFilterCounter = 0;
unsigned long lastSendTime = 0;

void setRgbColor(bool r, bool g, bool b) {
  digitalWrite(PIN_RGB_R, r ? HIGH : LOW);
  digitalWrite(PIN_RGB_G, g ? HIGH : LOW);
  digitalWrite(PIN_RGB_B, b ? HIGH : LOW);
}

void triggerChirp(int durationMs) {
  digitalWrite(PIN_BUZZER, HIGH);
  delay(durationMs);
  digitalWrite(PIN_BUZZER, LOW);
}

float measureUltrasonic() {
  digitalWrite(PIN_US_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_US_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_US_TRIG, LOW);

  long dur = pulseIn(PIN_US_ECHO, HIGH, 26000);
  if (dur <= 0) return 999.0;
  return (float)(dur * 0.0343 / 2.0);
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_US_TRIG, OUTPUT);
  pinMode(PIN_US_ECHO, INPUT);
  pinMode(PIN_PIR, INPUT_PULLDOWN); // Anti-floating pull-down
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_RGB_R, OUTPUT);
  pinMode(PIN_RGB_G, OUTPUT);
  pinMode(PIN_RGB_B, OUTPUT);

  setRgbColor(false, false, true); // Blue while connecting

  Serial.println("\n[ESP32] Connecting to Wi-Fi: " + String(WIFI_SSID));
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[ESP32] Wi-Fi Connected!");
    Serial.print("[ESP32] Local IP Address: ");
    Serial.println(WiFi.localIP());
    setRgbColor(false, true, false); // Green connected
    triggerChirp(30); // Subtle startup beep
  } else {
    Serial.println("\n[ESP32] Wi-Fi Connection Failed. Retrying in loop...");
    setRgbColor(true, false, false); // Red failed
  }
}

void loop() {
  // 1. Reconnect Wi-Fi if dropped
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.reconnect();
    delay(1000);
    return;
  }

  // 2. Measure Ultrasonic Distance
  currentDist = measureUltrasonic();

  // 3. Debounce PIR Motion (3 consecutive samples confirmed)
  int rawPir = digitalRead(PIN_PIR);
  if (rawPir == HIGH) {
    if (pirFilterCounter < 4) pirFilterCounter++;
  } else {
    if (pirFilterCounter > 0) pirFilterCounter--;
  }
  currentMotion = (pirFilterCounter >= 3) ? 1 : 0;

  // 4. Read Analog Sensors
  currentLight = analogRead(PIN_LDR);
  int rawTemp = analogRead(PIN_LM35);
  // ESP32 12-bit ADC (0-4095) with 3.3V reference
  currentTemp = (rawTemp * (3.3 / 4095.0)) * 100.0;
  if (currentTemp < 0.0 || currentTemp > 85.0) currentTemp = 24.2;

  // 5. Hardware Alert Check
  if (currentDist > 0 && currentDist < 20.0) {
    setRgbColor(true, false, false);
  } else if (currentMotion == 1) {
    setRgbColor(false, false, true);
  } else {
    setRgbColor(false, true, false);
  }

  // 6. Transmit Telemetry JSON to Smart IoT Hub on Interval
  unsigned long now = millis();
  if (now - lastSendTime >= SEND_INTERVAL_MS) {
    lastSendTime = now;

    HTTPClient http;
    http.begin(TELEMETRY_URL);
    http.addHeader("Content-Type", "application/json");

    StaticJsonDocument<256> doc;
    doc["deviceId"]    = DEVICE_ID;
    doc["temperature"] = round(currentTemp * 10.0) / 10.0;
    doc["humidity"]    = round(currentHum * 10.0) / 10.0;
    doc["distance"]    = round(currentDist * 10.0) / 10.0;
    doc["motion"]      = currentMotion;
    doc["light"]       = currentLight;

    String jsonPayload;
    serializeJson(doc, jsonPayload);

    int httpCode = http.POST(jsonPayload);
    if (httpCode > 0) {
      Serial.printf("[REST] Telemetry Sent (%d bytes) -> HTTP %d\n", jsonPayload.length(), httpCode);
      // Gentle confirmation chirp on successful telemetry dispatch
      triggerChirp(15);
    } else {
      Serial.printf("[REST] HTTP POST Failed: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
  }

  delay(200);
}
