/*
 * ============================================================================
 * SMART IOT HUB — ESP32 EMBEDDED JSON WEB SERVER (DIRECT POLLING)
 * Protocol: Embedded HTTP Server (CORS Enabled)
 * Target: ESP32 NodeMCU, WROOM-32, ESP32-S3, ESP32-C3
 * Built by TekStep Apps Uganda (tekstepapps.org)
 * ============================================================================
 *
 * HOW IT CONNECTS TO SMART IOT HUB:
 * 1. Enter your Wi-Fi SSID & Password below.
 * 2. Upload this sketch to your ESP32.
 * 3. Open Serial Monitor at 115200 baud to see your ESP32's IP address:
 *    Example: "http://192.168.1.145"
 * 4. In Smart IoT Hub dashboard -> Click "+ Add Device" -> Select "Custom REST / HTTP"
 *    Enter: "http://192.168.1.145/telemetry"
 * 5. Smart IoT Hub will poll your ESP32 directly over local Wi-Fi with zero cloud latency!
 *
 * ENDPOINTS PROVIDED:
 * - GET  /telemetry      Returns live JSON: {"temperature":24.5,"humidity":52,"distance":130,"motion":0,"light":620}
 * - POST /alarm?state=1  Triggers physical buzzer test sound
 * - POST /alarm?state=0  Silences physical buzzer
 * - POST /rgb?color=red  Sets RGB LED color (red, green, blue, off)
 * ============================================================================
 */

#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>

const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASS = "YOUR_WIFI_PASSWORD";

WebServer server(80);

// Hardware Pins
#define PIN_US_TRIG   18
#define PIN_US_ECHO   19
#define PIN_PIR       13
#define PIN_BUZZER    5
#define PIN_RGB_R     25
#define PIN_RGB_G     26
#define PIN_RGB_B     27
#define PIN_LDR       34
#define PIN_LM35      35

float currentTemp = 24.5;
float currentHum  = 55.0;
float currentDist = 150.0;
int   currentMotion = 0;
int   currentLight  = 650;
int   pirFilterCounter = 0;

void setRgb(bool r, bool g, bool b) {
  digitalWrite(PIN_RGB_R, r ? HIGH : LOW);
  digitalWrite(PIN_RGB_G, g ? HIGH : LOW);
  digitalWrite(PIN_RGB_B, b ? HIGH : LOW);
}

void triggerBeep(int durationMs) {
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

// Handler: GET /telemetry (with CORS for browser access)
void handleTelemetry() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");

  StaticJsonDocument<256> doc;
  doc["deviceId"]    = "dev_esp32_server";
  doc["temperature"] = round(currentTemp * 10.0) / 10.0;
  doc["humidity"]    = round(currentHum * 10.0) / 10.0;
  doc["distance"]    = round(currentDist * 10.0) / 10.0;
  doc["motion"]      = currentMotion;
  doc["light"]       = currentLight;
  doc["uptime"]      = millis() / 1000;

  String output;
  serializeJson(doc, output);
  server.send(200, "application/json", output);

  // Soft click chirp on poll
  triggerBeep(10);
}

// Handler: POST /alarm
void handleAlarm() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  String state = server.arg("state");
  if (state == "1" || state == "on") {
    triggerBeep(200);
    server.send(200, "application/json", "{\"status\":\"alarm_triggered\"}");
  } else {
    digitalWrite(PIN_BUZZER, LOW);
    server.send(200, "application/json", "{\"status\":\"alarm_silenced\"}");
  }
}

// Handler: CORS pre-flight OPTIONS request
void handleOptions() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
  server.send(204);
}

void setup() {
  Serial.begin(115200);

  pinMode(PIN_US_TRIG, OUTPUT);
  pinMode(PIN_US_ECHO, INPUT);
  pinMode(PIN_PIR, INPUT_PULLDOWN);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_RGB_R, OUTPUT);
  pinMode(PIN_RGB_G, OUTPUT);
  pinMode(PIN_RGB_B, OUTPUT);

  setRgb(false, false, true); // Blue connecting

  Serial.println("\n[ESP32 Server] Connecting to Wi-Fi: " + String(WIFI_SSID));
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\n[ESP32 Server] Wi-Fi Connected!");
  Serial.print("[ESP32 Server] Telemetry URL: http://");
  Serial.print(WiFi.localIP());
  Serial.println("/telemetry");

  // Route registration
  server.on("/telemetry", HTTP_GET, handleTelemetry);
  server.on("/telemetry", HTTP_OPTIONS, handleOptions);
  server.on("/alarm", HTTP_POST, handleAlarm);
  server.on("/alarm", HTTP_OPTIONS, handleOptions);

  server.begin();
  Serial.println("[ESP32 Server] HTTP Server Started on Port 80");
  setRgb(false, true, false); // Green online
  triggerBeep(40);
}

void loop() {
  server.handleClient();

  // Periodic sensor sampling
  static unsigned long lastSample = 0;
  if (millis() - lastSample >= 250) {
    lastSample = millis();

    currentDist = measureUltrasonic();

    int rawPir = digitalRead(PIN_PIR);
    if (rawPir == HIGH) {
      if (pirFilterCounter < 4) pirFilterCounter++;
    } else {
      if (pirFilterCounter > 0) pirFilterCounter--;
    }
    currentMotion = (pirFilterCounter >= 3) ? 1 : 0;

    currentLight = analogRead(PIN_LDR);
    int rawLm35 = analogRead(PIN_LM35);
    currentTemp = (rawLm35 * (3.3 / 4095.0)) * 100.0;
  }
}
