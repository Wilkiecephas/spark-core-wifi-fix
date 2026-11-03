/**
 * ============================================================================
 * ESP32-CAM AI-THINKER: SMART TELEMETRY, TELEGRAM BOT & WEB STREAM
 * Target: AI-Thinker ESP32-CAM (OV2640)
 * Integrates:
 *   1. Live MJPEG Network Video Stream (port 81: /stream) with CORS
 *   2. Telegram Bot (Alerts, Snapshots & Interactive /photo, /status commands)
 *   3. ThingSpeak Cloud Telemetry Relay (Field 1 - 7)
 *   4. Direct Smart IoT Hub Site REST Telemetry Relay (/api/telemetry)
 * ============================================================================
 */

#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "esp_http_server.h"
#include <ArduinoJson.h>

// ===================== USER CREDENTIALS =====================
const char* WIFI_SSID     = "Uncle beard Rabbie iphone "; 
const char* WIFI_PASSWORD = "harrison45";

// Telegram Bot Token (from user config)
const char* BOT_TOKEN     = "8944365980:AAE2UrPxeJdu6ySb_cChS8kVxyD1l4TOhHM";
String chat_id            = ""; // Auto-learned upon first message or set manually

// ThingSpeak Write API Key (from user config)
const char* THINGSPEAK_WRITE_KEY = "L3M8JA65KDDFKNAK";

// Smart IoT Hub local / LAN endpoint (set your PC's IP running smartroom-web)
const char* SITE_TELEMETRY_URL   = "http://192.168.1.100:5173/api/telemetry?device=esp32_cam";

// ===================== AI-THINKER PIN DEFINITIONS =====================
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

#define FLASH_LED_PIN      4

// ===================== RUNNING TELEMETRY STATE =====================
struct TelemetryState {
  float temperature   = 28.4;
  float humidity      = 54.0;
  float distance      = 115.0;
  int   motion        = 0;
  int   light         = 720;
  int   telegramPhotos = 0;
  String botStatus    = "READY";
  String lastBotCmd   = "None";
  String localIp      = "";
  String streamUrl    = "";
} state;

unsigned long lastTelegramPoll  = 0;
unsigned long lastThingSpeakSync = 0;
unsigned long lastSitePush       = 0;
int lastUpdateId = 0;

// HTTP Stream Server instance
httpd_handle_t stream_httpd = NULL;

// ===================== MJPEG STREAM HANDLER =====================
#define PART_BOUNDARY "123456789000000000000987654321"
static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

static esp_err_t stream_handler(httpd_req_t *req) {
  camera_fb_t *fb = NULL;
  esp_err_t res = ESP_OK;
  size_t _jpg_buf_len = 0;
  uint8_t * _jpg_buf = NULL;
  char part_buf[64];

  res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
  if (res != ESP_OK) return res;

  // Add CORS headers so web dashboard can display video
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_set_hdr(req, "X-Framerate", "25");

  while (true) {
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("[CAM] Frame capture failed");
      res = ESP_FAIL;
    } else {
      _jpg_buf_len = fb->len;
      _jpg_buf = fb->buf;
    }
    if (res == ESP_OK) {
      size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, _jpg_buf_len);
      res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
    }
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
    }
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
    }
    if (fb) {
      esp_camera_fb_return(fb);
      fb = NULL;
      _jpg_buf = NULL;
    } else if (_jpg_buf) {
      free(_jpg_buf);
      _jpg_buf = NULL;
    }
    if (res != ESP_OK) break;
    vTaskDelay(pdMS_TO_TICKS(40)); // ~25 FPS
  }
  return res;
}

static esp_err_t capture_handler(httpd_req_t *req) {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }
  httpd_resp_set_type(req, "image/jpeg");
  httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  esp_err_t res = httpd_resp_send(req, (const char *)fb->buf, fb->len);
  esp_camera_fb_return(fb);
  return res;
}

void startStreamServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 81; // Stream port (keeps port 80 free)
  config.ctrl_port = 32769;

  httpd_uri_t stream_uri = {
    .uri       = "/stream",
    .method    = HTTP_GET,
    .handler   = stream_handler,
    .user_ctx  = NULL
  };

  httpd_uri_t capture_uri = {
    .uri       = "/capture",
    .method    = HTTP_GET,
    .handler   = capture_handler,
    .user_ctx  = NULL
  };

  if (httpd_start(&stream_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(stream_httpd, &stream_uri);
    httpd_register_uri_handler(stream_httpd, &capture_uri);
    Serial.printf("[HTTP] Video stream started on port 81: %s\n", state.streamUrl.c_str());
  }
}

// ===================== CAMERA INIT =====================
bool initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0       = Y2_GPIO_NUM;
  config.pin_d1       = Y3_GPIO_NUM;
  config.pin_d2       = Y4_GPIO_NUM;
  config.pin_d3       = Y5_GPIO_NUM;
  config.pin_d4       = Y6_GPIO_NUM;
  config.pin_d5       = Y7_GPIO_NUM;
  config.pin_d6       = Y8_GPIO_NUM;
  config.pin_d7       = Y9_GPIO_NUM;
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  if (psramFound()) {
    config.frame_size   = FRAMESIZE_VGA;  // 640x480
    config.jpeg_quality = 12;
    config.fb_count     = 2;
  } else {
    config.frame_size   = FRAMESIZE_QVGA; // 320x240
    config.jpeg_quality = 14;
    config.fb_count     = 1;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("[CAM] Camera init failed with error 0x%x\n", err);
    return false;
  }
  Serial.println("[CAM] Camera hardware initialized.");
  return true;
}

// ===================== TELEGRAM SEND PHOTO =====================
bool sendTelegramPhoto(const String& targetChatId, const String& caption) {
  if (WiFi.status() != WL_CONNECTED || targetChatId.length() == 0) return false;

  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) return false;

  WiFiClientSecure client;
  client.setInsecure();

  if (!client.connect("api.telegram.org", 443)) {
    esp_camera_fb_return(fb);
    return false;
  }

  String boundary = "----ESP32CamBoundary";
  String head = "--" + boundary + "\r\nContent-Disposition: form-data; name=\"chat_id\"\r\n\r\n" + targetChatId + "\r\n";
  if (caption.length() > 0) {
    head += "--" + boundary + "\r\nContent-Disposition: form-data; name=\"caption\"\r\n\r\n" + caption + "\r\n";
  }
  head += "--" + boundary + "\r\nContent-Disposition: form-data; name=\"photo\"; filename=\"cam.jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";
  String tail = "\r\n--" + boundary + "--\r\n";

  uint32_t totalLen = head.length() + fb->len + tail.length();

  client.println("POST /bot" + String(BOT_TOKEN) + "/sendPhoto HTTP/1.1");
  client.println("Host: api.telegram.org");
  client.println("Content-Length: " + String(totalLen));
  client.println("Content-Type: multipart/form-data; boundary=" + boundary);
  client.println();
  client.print(head);

  uint8_t *fbBuf = fb->buf;
  size_t fbLen = fb->len;
  for (size_t n = 0; n < fbLen; n += 1024) {
    size_t chunk = (n + 1024 < fbLen) ? 1024 : (fbLen - n);
    client.write(fbBuf + n, chunk);
  }
  client.print(tail);
  esp_camera_fb_return(fb);

  state.telegramPhotos++;
  state.botStatus = "PHOTO_SENT";
  client.stop();
  Serial.println("[Telegram] Photo sent successfully!");
  return true;
}

// ===================== TELEGRAM SEND MESSAGE =====================
bool sendTelegramMessage(const String& targetChatId, const String& text) {
  if (WiFi.status() != WL_CONNECTED || targetChatId.length() == 0) return false;

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient https;
  String url = "https://api.telegram.org/bot" + String(BOT_TOKEN) + "/sendMessage";
  https.begin(client, url);
  https.addHeader("Content-Type", "application/json");

  StaticJsonDocument<256> doc;
  doc["chat_id"] = targetChatId;
  doc["text"]    = text;
  doc["parse_mode"] = "HTML";

  String body;
  serializeJson(doc, body);

  int code = https.POST(body);
  https.end();
  return (code == 200);
}

// ===================== TELEGRAM BOT POLL =====================
void handleTelegramUpdates() {
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient https;
  String url = "https://api.telegram.org/bot" + String(BOT_TOKEN) + "/getUpdates?offset=" + String(lastUpdateId + 1) + "&limit=5";
  https.begin(client, url);

  int code = https.GET();
  if (code == 200) {
    String payload = https.getString();
    StaticJsonDocument<2048> doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (!error && doc["ok"] == true) {
      JsonArray results = doc["result"].as<JsonArray>();
      for (JsonObject item : results) {
        lastUpdateId = item["update_id"];
        if (item.containsKey("message")) {
          JsonObject msg = item["message"];
          String fromChatId = String((long)msg["chat"]["id"]);
          chat_id = fromChatId; // Auto-learn chat ID

          if (msg.containsKey("text")) {
            String text = msg["text"].as<String>();
            text.trim();
            state.lastBotCmd = text;

            Serial.printf("[Telegram] Command received: %s from %s\n", text.c_str(), chat_id.c_str());

            if (text == "/start" || text == "/help") {
              String help = "<b>ESP32-CAM Smart IoT Sentinel</b>\n\n"
                            "Commands:\n"
                            "📸 /photo - Take & send camera snapshot\n"
                            "📊 /status - Sensor telemetry & system status\n"
                            "🌐 /stream - Get live web video stream link\n"
                            "💡 /flash - Toggle camera flash light";
              sendTelegramMessage(chat_id, help);
            }
            else if (text == "/photo") {
              sendTelegramMessage(chat_id, "Capturing snapshot...");
              sendTelegramPhoto(chat_id, "Snapshot from Smart IoT Hub CAM");
            }
            else if (text == "/status") {
              String stat = "<b>Telemetry Status:</b>\n"
                            "🌡 Temp: " + String(state.temperature, 1) + " °C\n"
                            "💧 Humidity: " + String(state.humidity, 1) + " %\n"
                            "📏 Distance: " + String(state.distance, 1) + " cm\n"
                            "🚨 Motion: " + String(state.motion ? "DETECTED" : "Clear") + "\n"
                            "📹 Web Stream: " + state.streamUrl;
              sendTelegramMessage(chat_id, stat);
            }
            else if (text == "/stream") {
              sendTelegramMessage(chat_id, "Live Video Stream URL:\n" + state.streamUrl);
            }
            else if (text == "/flash") {
              static bool flashOn = false;
              flashOn = !flashOn;
              digitalWrite(FLASH_LED_PIN, flashOn ? HIGH : LOW);
              sendTelegramMessage(chat_id, flashOn ? "Flash LED turned ON" : "Flash LED turned OFF");
            }
          }
        }
      }
    }
  }
  https.end();
}

// ===================== 1. THINGSPEAK SYNC =====================
void syncThingSpeak() {
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClient client;
  HTTPClient http;

  String url = String("http://api.thingspeak.com/update?api_key=") + THINGSPEAK_WRITE_KEY
             + "&field1=" + String(state.temperature, 2)
             + "&field2=" + String(state.humidity, 2)
             + "&field3=" + String(state.distance, 1)
             + "&field4=" + String(state.motion)
             + "&field5=" + String(state.light)
             + "&field6=" + String(state.telegramPhotos)
             + "&field7=" + String(state.motion ? 2 : 1);

  http.begin(client, url);
  int code = http.GET();
  if (code > 0) {
    Serial.printf("[ThingSpeak] Update synced (Entry #%s)\n", http.getString().c_str());
  } else {
    Serial.printf("[ThingSpeak] Failed: %s\n", http.errorToString(code).c_str());
  }
  http.end();
}

// ===================== 2. DIRECT SITE TELEMETRY RELAY =====================
void relayDirectTelemetryToSite() {
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClient client;
  HTTPClient http;
  http.begin(client, SITE_TELEMETRY_URL);
  http.addHeader("Content-Type", "application/json");

  StaticJsonDocument<384> doc;
  doc["temperature"]       = state.temperature;
  doc["humidity"]          = state.humidity;
  doc["distance"]          = state.distance;
  doc["motion"]            = state.motion;
  doc["light"]             = state.light;
  doc["bot_status"]        = state.botStatus;
  doc["bot_last_cmd"]      = state.lastBotCmd;
  doc["bot_photos_sent"]   = state.telegramPhotos;
  doc["stream_url"]        = state.streamUrl;
  doc["ip"]                = state.localIp;

  String body;
  serializeJson(doc, body);

  int code = http.POST(body);
  http.end();
}

// ===================== SETUP =====================
void setup() {
  Serial.begin(115200);
  pinMode(FLASH_LED_PIN, OUTPUT);
  digitalWrite(FLASH_LED_PIN, LOW);

  Serial.println("\n--- ESP32-CAM Smart IoT Sentinel Initializing ---");

  // 1. Init Camera
  if (!initCamera()) {
    Serial.println("[ERROR] Camera failed to initialize! Check ribbon cable.");
  }

  // 2. Connect WiFi
  Serial.printf("[WiFi] Connecting to %s\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 35) {
    delay(500);
    Serial.print(".");
    retry++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    state.localIp = WiFi.localIP().toString();
    state.streamUrl = "http://" + state.localIp + ":81/stream";

    Serial.println("\n[WiFi] Connected successfully!");
    Serial.printf("[WiFi] Local IP: %s\n", state.localIp.c_str());
    Serial.printf("[WiFi] Live Video Stream: %s\n", state.streamUrl.c_str());

    // Start Video Stream Server
    startStreamServer();

    // Initial Telemetry Sync
    syncThingSpeak();
    relayDirectTelemetryToSite();
  } else {
    Serial.println("\n[WiFi] Connection timed out! Check hotspot SSID & Password.");
  }
}

// ===================== MAIN LOOP =====================
void loop() {
  // Read sensor pins here if connected (e.g. PIR, Ultrasonics, ADC)
  // Check for motion or simulated changes:
  // state.temperature = ...;

  // 1. Poll Telegram Bot every 2 seconds
  if (millis() - lastTelegramPoll > 2000) {
    lastTelegramPoll = millis();
    handleTelegramUpdates();
  }

  // 2. Push direct telemetry to website every 3 seconds
  if (millis() - lastSitePush > 3000) {
    lastSitePush = millis();
    relayDirectTelemetryToSite();
  }

  // 3. Sync ThingSpeak cloud channel every 20 seconds
  if (millis() - lastThingSpeakSync > 20000) {
    lastThingSpeakSync = millis();
    syncThingSpeak();
  }

  delay(20);
}
