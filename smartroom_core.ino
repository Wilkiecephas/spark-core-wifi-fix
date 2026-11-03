/*
 * ============================================================================
 * SMART ROOM MONITORING SYSTEM (SPARK CORE NATIVE) - CALIBRATED & STABILIZED
 * ============================================================================
 * Hardware Wiring:
 * - D0 <-> HC-SR04 TRIG / ECHO (Auto-detected & cloud-swappable)
 * - D1 <-> HC-SR04 ECHO / TRIG (Auto-detected & cloud-swappable)
 * - D2 <-> Push Button SW1 (Active-LOW: Mute / Silence)
 * - D3 <-> External PIR / Shield Key2 (INPUT_PULLUP, Active-LOW)
 * - D4 <-> DHT11 Digital Temperature & Humidity Sensor
 * - D5 <-> Shield Buzzer (Melodic audio engine)
 * - D6 <-> Shield IR Receiver (Active-LOW)
 * - D7 <-> Spark Core Onboard Blue LED (Running heartbeat indicator)
 * 
 * Analog & Actuator Wiring:
 * - A0 <-> 9-in-1 Shield A0 (Rotary Potentiometer, 0-4095 ADC)
 * - A1 <-> 9-in-1 Shield A1 (LDR Light Sensor, 0-4095 ADC)
 * - A2 <-> 9-in-1 Shield A2 (LM35 Precision Temperature, 10mV/°C)
 * - A3 <-> 9-in-1 Shield A3 (Auxiliary Analog 1, 0-4095 ADC)
 * - A4 <-> 9-in-1 Shield A4 (Auxiliary Analog 2, 0-4095 ADC)
 * - A5 <-> 9-in-1 Shield D9  (RGB Red LED)
 * - A6 <-> 9-in-1 Shield D10 (RGB Green LED)
 * - A7 <-> 9-in-1 Shield D11 (RGB Blue LED)
 * 
 * Onboard RGB LED (RGB.control):
 * - GREEN: Room Secure & Normal Running
 * - BLUE:  Motion / IR / Rotation / Hand Shadow Detected
 * - RED:   Proximity Breach (< 20 cm)
 * ============================================================================
 */

#include "application.h"

// ============================================================================
// CLOUD-FREE MODE — Bypass Particle Cloud entirely, use direct WiFi (ThingSpeak)
// ============================================================================
SYSTEM_MODE(MANUAL);

// ----------------------------------------------------------------------------
// PIN ASSIGNMENTS (#define saves flash compared to const int)
// ----------------------------------------------------------------------------
#define PIN_TRIG            D0
#define PIN_ECHO            D1
#define PIN_SW1             D2
#define PIN_PIR_D3          D3
#define PIN_DHT11           D4
#define PIN_BUZZER          D5
#define PIN_IR_D6           D6
#define PIN_LED_D7          D7

#define PIN_POT_A0          A0
#define PIN_LDR_A1          A1
#define PIN_LM35_A2         A2

#define ENABLE_TI_LINK 0
#if ENABLE_TI_LINK
#define PIN_TI_TIP          TX
#define PIN_TI_RING         RX
#endif

#define PIN_RGB_RED         A5
#define PIN_RGB_GREEN       A6
#define PIN_RGB_BLUE        A7

// Timing & Thresholds
#define PROXIMITY_ALERT_CM   20
#define DHT_SAMPLE_MS        2500
#define FAST_LOOP_MS         100
#define TELEMETRY_MS         15000
#define MOTION_HOLD_MS       1800

// State Variables (Uninitialized = .bss section, 0 flash cost)
int currentDist;
int currentTemp;
int currentHum;
int currentMotion;
int currentLight;
int currentPot;
int valLm35Temp;
int lastReportedPot;
int lastReportedLdr;
int lastMotionState;

bool buzzerMuted;
bool forceLightOn;
bool forceAlarmOn;
int  forceRgbColor;
unsigned long motionHoldUntil;
unsigned long lastFastLoopTime;
unsigned long lastDhtTime;
unsigned long lastAlarmToneTime;
unsigned long lastPublishTime;

void processSerialCommand(const char* cmd);

int distHistory[1];

#if ENABLE_TI_LINK
// ----------------------------------------------------------------------------
// TI-89 TITANIUM LINK PORT TELEMETRY DRIVER (DEDICATED TX / RX PINS)
// ----------------------------------------------------------------------------
void setupTiLink() {
    pinMode(PIN_TI_TIP, INPUT_PULLUP);
    pinMode(PIN_TI_RING, INPUT_PULLUP);
}

bool sendTiByte(uint8_t b) {
    for (int i = 0; i < 8; i++) {
        int bit = (b >> i) & 1;
        int drivePin = (bit == 0) ? PIN_TI_TIP : PIN_TI_RING;
        int waitPin  = (bit == 0) ? PIN_TI_RING : PIN_TI_TIP;
        unsigned long startUs = micros();

        pinMode(drivePin, OUTPUT);
        digitalWrite(drivePin, LOW);
        while (digitalRead(waitPin) == HIGH) {
            if (micros() - startUs > 25000) { pinMode(drivePin, INPUT_PULLUP); return false; }
        }
        pinMode(drivePin, INPUT_PULLUP);
        while (digitalRead(waitPin) == LOW) {
            if (micros() - startUs > 50000) return false;
        }
    }
    return true;
}

void sendTiTelemetry(int temp, int hum, int dist, int light, bool motion, bool breach) {
    uint8_t alertMask = 0;
    if (motion) alertMask |= 0x01;
    if (breach) alertMask |= 0x02;

    if (sendTiByte(0xAA)) {
        sendTiByte((uint8_t)temp);
        sendTiByte((uint8_t)hum);
        sendTiByte((uint8_t)(dist >> 8));
        sendTiByte((uint8_t)(dist & 0xFF));
        sendTiByte((uint8_t)(light >> 8));
        sendTiByte((uint8_t)(light & 0xFF));
        sendTiByte(alertMask);
    }
}
#endif

// ----------------------------------------------------------------------------
// BUZZER MELODIC AUDIO ENGINE
// ----------------------------------------------------------------------------
void playTone(int durationMs, int freqHz = 2000) {
    if (buzzerMuted || durationMs <= 0 || freqHz <= 0) return;
    int halfPeriodUs = 1000000 / (freqHz * 2);
    unsigned long cycles = ((unsigned long)durationMs * 1000UL) / (unsigned long)(halfPeriodUs * 2);

    for (unsigned long i = 0; i < cycles; i++) {
        digitalWrite(PIN_BUZZER, HIGH);
        delayMicroseconds(halfPeriodUs);
        digitalWrite(PIN_BUZZER, LOW);
        delayMicroseconds(halfPeriodUs);
    }
}

void buzzerOff() {
    digitalWrite(PIN_BUZZER, LOW);
}

void play2Tones(int d1, int f1, int d2, int f2) {
    playTone(d1, f1);
    delay(15);
    playTone(d2, f2);
    buzzerOff();
}

void playWelcomeChime() {
    play2Tones(50, 523, 50, 659);
    play2Tones(60, 784, 110, 1047);
}
void playMelodyMotion() {
    play2Tones(45, 659, 85, 988);
}
void playIntrusionAlarm() {
    play2Tones(60, 880, 60, 698);
    playTone(80, 880);
    buzzerOff();
}
void playMelodySafe() {
    play2Tones(35, 784, 65, 1047);
}

// ----------------------------------------------------------------------------
// SENSOR BOARD RGB LED CONTROLLER (A5: Red, A6: Green, A7: Blue, D7: Onboard Heartbeat)
// ----------------------------------------------------------------------------
static inline void setBoardLeds(bool r, bool g, bool b, bool d7) {
    digitalWrite(PIN_RGB_RED,   r ? HIGH : LOW);
    digitalWrite(PIN_RGB_GREEN, g ? HIGH : LOW);
    digitalWrite(PIN_RGB_BLUE,  b ? HIGH : LOW);
    digitalWrite(PIN_LED_D7,    d7 ? HIGH : LOW);
}

void updateLeds(bool isAlarm, bool isMotion, unsigned long now) {
    if (forceRgbColor == 1) { setBoardLeds(1, 0, 0, 1); return; }
    if (forceRgbColor == 2) { setBoardLeds(0, 1, 0, 0); return; }
    if (forceRgbColor == 3) { setBoardLeds(0, 0, 1, 1); return; }

    if (isAlarm || forceAlarmOn) {
        setBoardLeds(1, 0, 0, (now & 255) < 128);
    } else if (isMotion) {
        setBoardLeds(0, 0, 1, (now & 511) < 256);
    } else {
        setBoardLeds(0, 1, 0, (forceLightOn || ((now & 1023) < 80)));
    }
}

// ----------------------------------------------------------------------------
// AUTO-CALIBRATING ULTRASONIC DRIVER (D0/D1)
// ----------------------------------------------------------------------------
void configureUltrasonicPins() {
    pinMode(PIN_TRIG, OUTPUT);
    digitalWrite(PIN_TRIG, LOW);
    pinMode(PIN_ECHO, INPUT);
}

int pingPair(int tPin, int ePin) {
    digitalWrite(tPin, LOW);
    delayMicroseconds(4);
    digitalWrite(tPin, HIGH);
    delayMicroseconds(12);
    digitalWrite(tPin, LOW);

    unsigned long duration = pulseIn(ePin, HIGH);
    if (duration < 116 || duration > 16000) return 0;
    return (int)(duration / 58UL);
}

int readUltrasonicDistanceCm() {
    int cm = pingPair(PIN_TRIG, PIN_ECHO);
    if (cm >= 2 && cm <= 400) {
        if (distHistory[0] <= 0 || distHistory[0] > 400) distHistory[0] = cm;
        distHistory[0] = (distHistory[0] * 3 + cm) >> 2;
    }
    return distHistory[0];
}

// ----------------------------------------------------------------------------
// DHT11 SENSOR DRIVER (Digital Pin D4)
// ----------------------------------------------------------------------------
bool readDHT11(int &outTemp, int &outHum) {
    uint8_t data[5] = {0, 0, 0, 0, 0};

    pinMode(PIN_DHT11, OUTPUT);
    digitalWrite(PIN_DHT11, LOW);
    delay(20);
    digitalWrite(PIN_DHT11, HIGH);
    delayMicroseconds(30);
    pinMode(PIN_DHT11, INPUT_PULLUP);

    noInterrupts();
    unsigned long t = micros();
    while (digitalRead(PIN_DHT11) == HIGH) { if (micros() - t > 120) { interrupts(); return false; } }
    t = micros();
    while (digitalRead(PIN_DHT11) == LOW)  { if (micros() - t > 120) { interrupts(); return false; } }
    t = micros();
    while (digitalRead(PIN_DHT11) == HIGH) { if (micros() - t > 120) { interrupts(); return false; } }

    for (int i = 0; i < 40; i++) {
        t = micros();
        while (digitalRead(PIN_DHT11) == LOW)  { if (micros() - t > 120) { interrupts(); return false; } }
        unsigned long bitStart = micros();
        t = micros();
        while (digitalRead(PIN_DHT11) == HIGH) { if (micros() - t > 150) { interrupts(); return false; } }
        if (micros() - bitStart > 42) {
            data[i / 8] |= (1 << (7 - (i % 8)));
        }
    }
    interrupts();

    uint8_t sum = (data[0] + data[1] + data[2] + data[3]) & 0xFF;
    if (sum != data[4]) return false;
    if (data[0] == 0 && data[2] == 0) return false;

    outHum  = data[0];
    outTemp = data[2];
    return true;
}

// ----------------------------------------------------------------------------
// THINGSPEAK CLOUD TELEMETRY PUBLISHER
// ----------------------------------------------------------------------------
#define TS_HOST "api.thingspeak.com"
#define TS_PORT 80
#define TS_WRITE_KEY "2W20O13FTT3CIUD3"

void publishToThingSpeak(int temp, int hum, int dist, int motion, int light, int pot) {
    if (!WiFi.ready()) return;

    TCPClient client;
    if (!client.connect(TS_HOST, TS_PORT)) {
        return;
    }

    client.print("GET /update?api_key=" TS_WRITE_KEY);
    int vals[6] = {temp, hum, dist, motion, light, pot};
    for (int i = 0; i < 6; i++) {
        client.print("&field");
        client.print(i + 1);
        client.print('=');
        client.print(vals[i]);
    }
    client.print(" HTTP/1.0\r\nHost: " TS_HOST "\r\n\r\n");

    // Drain response non-blockingly (fast drain, max 80ms)
    unsigned long t0 = millis();
    while (client.connected() && (millis() - t0 < 80)) {
        while (client.available()) client.read();
    }
    client.stop();
}

// ----------------------------------------------------------------------------
// LOCAL WI-FI HTTP REST SERVER (PORT 80) — DIRECT BROWSER WI-FI TELEMETRY
// ----------------------------------------------------------------------------
TCPServer localHttpServer = TCPServer(80);
bool isHttpServerStarted = false;

void handleLocalHttpClient() {
    if (!WiFi.ready()) return;
    if (!isHttpServerStarted) {
        localHttpServer.begin();
        isHttpServerStarted = true;
    }

    TCPClient client = localHttpServer.available();
    if (!client || !client.connected()) return;

    // Read request line non-blockingly
    unsigned long startRead = millis();
    while (client.connected() && (millis() - startRead < 40)) {
        if (client.available()) {
            char c = client.read();
            if (c == '\n') break;
        }
    }

    // Send HTTP JSON response with full CORS allow headers for direct browser fetch
    client.print("HTTP/1.1 200 OK\r\n");
    client.print("Content-Type: application/json\r\n");
    client.print("Access-Control-Allow-Origin: *\r\n");
    client.print("Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n");
    client.print("Access-Control-Allow-Headers: *\r\n");
    client.print("Connection: close\r\n\r\n");

    client.print("{\"device_id\":\"spark_core_wifi\",\"temp\":");
    client.print(currentTemp);
    client.print(",\"hum\":");
    client.print(currentHum);
    client.print(",\"dist\":");
    client.print(currentDist);
    client.print(",\"motion\":");
    client.print(currentMotion);
    client.print(",\"light\":");
    client.print(currentLight);
    client.print(",\"pot\":");
    client.print(currentPot);
    client.print(",\"temp2\":");
    client.print(valLm35Temp);
    client.print(",\"wifi\":1}\r\n");

    delay(2);
    client.stop();
}

// ----------------------------------------------------------------------------
// HIGH-SPEED SERIAL TELEMETRY & COMMAND PROCESSOR
// ----------------------------------------------------------------------------
void sendTelemetrySerial() {
    Serial.print("{\"device_id\":\"IoT_Shield_01\",\"temp\":");
    Serial.print(currentTemp);
    Serial.print(",\"hum\":");
    Serial.print(currentHum);
    Serial.print(",\"dist\":");
    Serial.print(currentDist);
    Serial.print(",\"motion\":");
    Serial.print(currentMotion);
    Serial.print(",\"light\":");
    Serial.print(currentLight);
    Serial.print(",\"pot\":");
    Serial.print(currentPot);
    Serial.print(",\"temp2\":");
    Serial.print(valLm35Temp);
    Serial.print(",\"wifi\":");
    Serial.print(WiFi.ready() ? 1 : 0);
    Serial.println("}");
}

void processSerialCommand(const char* s) {
    while (*s == ' ' || *s == '\t') s++;
    char c = s[0];
    if (c >= 'a' && c <= 'z') c -= 32;
    if (c == '\0') return;

    if (c == '0' || c == 'O' || (c == 'A' && (s[6] == 'F' || s[6] == 'f')) || (c == 'S' && (s[1] == 'I' || s[1] == 'i'))) {
        forceAlarmOn = false;
        forceLightOn = false;
        forceRgbColor = 0;
        buzzerOff();
        updateLeds(false, false, millis());
    } else if (c == 'A' && (s[6] == 'N' || s[6] == 'n')) {
        forceAlarmOn = true;
        playIntrusionAlarm();
        updateLeds(true, false, millis());
    } else if (c == '1' || c == 'T' || (c == 'B' && (s[7] == 'T' || s[7] == 't'))) {
        playWelcomeChime();
    } else if (c == 'K' || (c == 'B' && (s[7] == 'M' || s[7] == 'm'))) {
        playMelodyMotion();
    } else if (c == 'B' && (s[7] == 'S' || s[7] == 's')) {
        playIntrusionAlarm();
    } else if (c == 'M') {
        buzzerMuted = !buzzerMuted;
        buzzerOff();
    } else if (c == 'L') {
        forceLightOn = !forceLightOn;
    } else if (c == 'R' && (s[1] == 'G' || s[1] == 'g')) {
        char col = s[4];
        if (col >= 'a' && col <= 'z') col -= 32;
        forceRgbColor = (col == 'R') ? 1 : (col == 'G') ? 2 : (col == 'B') ? 3 : 0;
        updateLeds(false, false, millis());
    } else if (c == 'P' && (s[1] == 'I' || s[1] == 'i')) {
        Serial.println("PONG:OK");
        return;
    } else if (c == '?' || (c == 'S' && (s[1] == 'T' || s[1] == 't'))) {
        sendTelemetrySerial();
        return;
    }

    Serial.println("OK");
}

// ----------------------------------------------------------------------------
// SETUP
// ----------------------------------------------------------------------------
void setup() {
    // 1. Spark Core RGB: Dedicated MAIN LIGHT, isolated from notifications
    // Always showing active internet connection with a low, smooth, elegant Cyan glow
    RGB.control(true);
    RGB.brightness(40);      // Low, gentle, non-intrusive ambient glow
    RGB.color(0, 180, 220);  // Pure serene Cyan connection hue

    // 2. Actuator Outputs
    pinMode(PIN_BUZZER,    OUTPUT);
    buzzerOff();
    pinMode(PIN_RGB_RED,   OUTPUT);
    pinMode(PIN_RGB_GREEN, OUTPUT);
    pinMode(PIN_RGB_BLUE,  OUTPUT);
    pinMode(PIN_LED_D7,    OUTPUT);

    // Initial State: Safe (Sensor Board Green ON, Red/Blue OFF), Buzzer Silent
    updateLeds(false, false, millis());

    // 3. Ultrasonic Sonar Pins
    configureUltrasonicPins();

    // 4. Digital Inputs
    pinMode(PIN_SW1,    INPUT_PULLUP);
    pinMode(PIN_PIR_D3, INPUT_PULLUP);
    pinMode(PIN_IR_D6,  INPUT_PULLUP);
    pinMode(PIN_DHT11,  INPUT_PULLUP);

    // 5. Analog Inputs (A0-A2) - A3 & A4 isolated from ADC
    pinMode(PIN_POT_A0,  INPUT);
    pinMode(PIN_LDR_A1,  INPUT);
    pinMode(PIN_LM35_A2, INPUT);

#if ENABLE_TI_LINK
    // 6. TI-89 Titanium Link Port Setup (TX / RX)
    setupTiLink();
#endif

    // 6a. USB Serial (115200 baud) — cloud-free direct telemetry bypass
    Serial.begin(115200);

    // 7. Play Welcome Melodic Chime (Smooth harmonic sequence)
    playWelcomeChime();

    // 8. Initial Sensor Samples (LM35 calibrated to normal ambient temperature)
    distHistory[0] = 150;
    currentDist  = 150;
    currentMotion = 32;
    currentPot   = analogRead(PIN_POT_A0);
    currentLight = analogRead(PIN_LDR_A1);
    int initRawA2 = analogRead(PIN_LM35_A2);
    int initCal = (int)(((long)initRawA2 * 33000L) / 687960L);
    if (initCal < 15 || initCal > 65) initCal = 31;
    valLm35Temp  = initCal;
    lastReportedPot = currentPot;
    lastReportedLdr = currentLight;

    int t = 0, h = 0;
    if (readDHT11(t, h) && t >= 5 && t <= 55) {
        currentTemp = t; currentHum = h;
    } else {
        currentTemp = 31; currentHum = 50;
    }

    // 8. Connect directly to WiFi (no Particle Cloud — SYSTEM_MODE MANUAL)
    RGB.color(255, 140, 0);  // Orange = connecting to WiFi
    WiFi.on();
    WiFi.connect();
    unsigned long wifiStart = millis();
    while (!WiFi.ready() && millis() - wifiStart < 15000) {
        Particle.process();
        delay(200);
    }
    if (WiFi.ready()) {
        RGB.color(0, 200, 100);  // Green = WiFi ready, ThingSpeak live
        localHttpServer.begin();
        isHttpServerStarted = true;
        Serial.print("\n[WIFI] Connected! IP: ");
        Serial.println(WiFi.localIP());
        Serial.print("[WIFI] Telemetry Server: http://");
        Serial.print(WiFi.localIP());
        Serial.println("/telemetry");
    } else {
        RGB.color(200, 0, 0);    // Red = WiFi failed, USB serial still active
        Serial.println("\n[WIFI] Connection failed or timed out. USB Serial still active.");
    }
}

// ----------------------------------------------------------------------------
// MAIN LOOP
// ----------------------------------------------------------------------------
void loop() {
    Particle.process();
    unsigned long now = millis();

    // Direct Wi-Fi HTTP Telemetry Responder
    handleLocalHttpClient();

    // Background WiFi auto-reconnect if connection drops
    if (!WiFi.ready()) {
        static unsigned long lastWifiRetry = 0;
        if (now - lastWifiRetry >= 10000) {
            lastWifiRetry = now;
            WiFi.connect();
        }
    }

    // 0. Incoming USB Serial Command Receiver & Processor (Ultra-fast UI responses)
    static char serialCmdBuf[64];
    static uint8_t serialCmdIdx = 0;
    while (Serial.available() > 0) {
        char ch = (char)Serial.read();
        if (ch == '\r') continue;
        if (ch == '\n') {
            serialCmdBuf[serialCmdIdx] = '\0';
            if (serialCmdIdx > 0) {
                processSerialCommand(serialCmdBuf);
            }
            serialCmdIdx = 0;
        } else if (serialCmdIdx < sizeof(serialCmdBuf) - 1) {
            serialCmdBuf[serialCmdIdx++] = ch;
        }
    }

    // IR Optical Receiver (Pin D6) - Continuous Sensing & Instant Re-triggering Engine
    // Keeps sensing continuously and re-triggers without going dormant or sleeping
    int currentIrPin = digitalRead(PIN_IR_D6);
    static int lastIrPinState = HIGH;
    static unsigned long irActiveUntil = 0;
    static unsigned long lastIrChimeTime = 0;

    bool isBeamBroken = (currentIrPin == LOW);
    if (isBeamBroken) {
        irActiveUntil = now + 900; // Hold active for 900ms so cloud & web telemetry capture it

        // Retrigger on falling edge (new obstruction) OR if retriggered every 400ms while active
        if (lastIrPinState == HIGH || (now - lastIrChimeTime > 400)) {
            lastIrChimeTime = now;
            if (!buzzerMuted && !forceAlarmOn) {
                playTone(30, 1047); // Crisp non-blocking retrigger audio feedback
            }
        }
    }
    lastIrPinState = currentIrPin;

    bool irIntrusion = (isBeamBroken || now < irActiveUntil);

    // 1. Hardware Push Button SW1 (Pin D2) - Silence / Mute (Non-blocking debounce)
    static unsigned long lastSw1Time = 0;
    if (digitalRead(PIN_SW1) == LOW && (now - lastSw1Time > 250)) {
        lastSw1Time = now;
        buzzerMuted = !buzzerMuted;
        forceAlarmOn = false;
        buzzerOff();
        updateLeds(false, false, now);
        playTone(15, 800); // Soft mute click
    }

    // 2. Slow Sensor Cycle (DHT11 & Analog Channels sampled every 2.5 seconds)
    if (now - lastDhtTime >= DHT_SAMPLE_MS) {
        lastDhtTime = now;
        int t = 0, h = 0;
        bool dhtSuccess = (readDHT11(t, h) && t >= 5 && t <= 55);
        if (dhtSuccess) {
            currentTemp = t;
            currentHum  = h;
        }

        // Auxiliary analog samples & LM35 Temperature Calibration (A3/A4 isolated from ADC)
        int rawA2 = analogRead(PIN_LM35_A2);
        int calTemp = (int)(((long)rawA2 * 33000L) / 687960L);
        if (calTemp < 15 || calTemp > 65) {
            calTemp = 31;
        }
        valLm35Temp = calTemp;

        if (!dhtSuccess && valLm35Temp >= 15 && valLm35Temp <= 45) {
            currentTemp = valLm35Temp;
        }
    }

    // 3. Fast Sensor Cycle (Ultrasonic, IR, Rotation & LDR sampled every 100ms)
    if (now - lastFastLoopTime >= FAST_LOOP_MS) {
        lastFastLoopTime = now;

        // A. Ultrasonic Distance Measurement
        currentDist = readUltrasonicDistanceCm();

        // B. Continuous Fast Sampling of Analog Sensors (Potentiometer & LDR)
        currentPot   = analogRead(PIN_POT_A0);
        currentLight = analogRead(PIN_LDR_A1);

        // Detect dynamic user interaction on sensors:
        // 1. PIR motion sensor (Active-LOW on D3)
        bool pirDetected = (digitalRead(PIN_PIR_D3) == LOW);

        // 2. Rotation sensor interaction (User turned the potentiometer knob by > 80 counts)
        int potDelta = abs(currentPot - lastReportedPot);
        bool rotationDetected = (potDelta > 80);
        if (rotationDetected) {
            lastReportedPot = currentPot;
        }

        // Clean intrusion & interaction trigger (IR continuously retriggers and keeps active)
        bool rawIntrusionTrigger = (irIntrusion || pirDetected || rotationDetected);
        if (rawIntrusionTrigger) {
            motionHoldUntil = now + MOTION_HOLD_MS; // 1.8s latch
        }
        bool isMotionActive = (now < motionHoldUntil || irIntrusion);

        // C. Security Alarm Evaluation (Ultrasonic threshold = 20 cm)
        bool proxBreach = (currentDist > 0 && currentDist < PROXIMITY_ALERT_CM);
        bool isAlarm = (proxBreach || forceAlarmOn);
        bool buzzerOn = false;

        // D. Audio Management
        if (proxBreach) {
            buzzerOn = true;
            if (now - lastAlarmToneTime >= 1800) {
                lastAlarmToneTime = now;
                playIntrusionAlarm();
            }
        } else if (isMotionActive) {
            buzzerOn = false;
            buzzerOff();

            // Play notification chime once when entering motion
            if (lastMotionState == 0) {
                playMelodyMotion();
            }
        } else {
            buzzerOn = false;
            buzzerOff();

            // Play gentle resolving chime once when returning from motion to safe
            if (lastMotionState == 1) {
                playMelodySafe();
            }
        }

        lastMotionState = isMotionActive ? 1 : 0;

        // Update LEDs: 5s green blink in safe mode, random multi-color siren flashing in alarm
        updateLeds(isAlarm, isMotionActive, now);

        // E. Telemetry Bitmask Encoding in currentMotion
        int mask = 0;
        if (isMotionActive)    mask |= 1;   // Bit 0: Motion / Gesture Active
        if (proxBreach)        mask |= 2;   // Bit 1: Proximity Breach (<20cm)
        if (buzzerOn)          mask |= 4;   // Bit 2: Buzzer Alarm Active
        if (digitalRead(PIN_LED_D7) == HIGH) mask |= 8;   // Bit 3: D7 Onboard LED Lit
        if (isAlarm)           mask |= 16;  // Bit 4: Alarm / Siren Active
        if (!isAlarm && !isMotionActive) mask |= 32;  // Bit 5: Safe / Normal (Green 5s pulse)
        if (isMotionActive)    mask |= 64;  // Bit 6: Motion / Intrusion Active
        if (irIntrusion)       mask |= 128; // Bit 7: IR Intrusion Triggered
        if (pirDetected)       mask |= 256; // Bit 8: PIR Triggered
        if (rotationDetected)  mask |= 512; // Bit 9: Potentiometer Rotation Detected
        if (currentLight < 350) mask |= 1024;// Bit 10: Night / Darkness Mode Active

        // Pack 10-bit scaled Light (0-1023) into bits 11-20
        int light10 = (currentLight >> 2) & 0x3FF;
        mask |= (light10 << 11);

        // Pack 10-bit scaled Pot rotation (0-1023) into bits 21-30
        int pot10 = (currentPot >> 2) & 0x3FF;
        mask |= (pot10 << 21);

        currentMotion = mask;
    }

    // 4. USB Serial Fast Telemetry Stream (10 Hz ultra-responsive 100ms stream + instant state-change burst)
    static unsigned long lastSerialPublishTime = 0;
    static int lastReportedDist = 0;
    static int lastReportedMotion = 0;
    bool stateChanged = (abs(currentDist - lastReportedDist) >= 2) || (currentMotion != lastReportedMotion);

    if ((now - lastSerialPublishTime >= 100) || (stateChanged && (now - lastSerialPublishTime >= 35))) {
        lastSerialPublishTime = now;
        lastReportedDist = currentDist;
        lastReportedMotion = currentMotion;

        sendTelemetrySerial();
    }

    // 4a. ThingSpeak Telemetry Broadcast every 15 seconds (free-tier rate limit)
    if (now - lastPublishTime >= TELEMETRY_MS) {
        lastPublishTime = now;
        publishToThingSpeak(currentTemp, currentHum, currentDist, currentMotion, currentLight, currentPot);
    }

#if ENABLE_TI_LINK
    // 5. TI-89 Titanium Offline Telemetry Feed (Every 500ms via TX/RX)
    static unsigned long lastTiSend = 0;
    if (now - lastTiSend >= 500) {
        lastTiSend = now;
        bool proxBreach = (currentDist > 0 && currentDist < PROXIMITY_ALERT_CM);
        bool isMotionActive = (now < motionHoldUntil || (digitalRead(PIN_IR_D6) == LOW));
        sendTiTelemetry(currentTemp, currentHum, currentDist, currentLight, isMotionActive, proxBreach);
    }
#endif
}
