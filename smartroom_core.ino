/*
 * ============================================================================
 * SMART ROOM MONITORING SYSTEM (SPARK CORE NATIVE - NO ARDUINO UNO REQUIRED)
 * ============================================================================
 * Course: TETE/TEEE 3102 - Microcontrollers
 * Kyambogo University - Faculty of Engineering
 *
 * Direct Hardware Integration:
 * - 9-in-1 Multifunctional Expansion Shield (DHT11, Buzzer, RGB LED, LDR)
 * - SZ-HS100 Analog Humidity Sensor (Analog Pin A0, 0-3.3V ADC)
 * - HC-SR04 Ultrasonic Distance Sensor
 * - 12V PIR Motion Sensor / HC-SR501 (Digital Pin D3, INPUT_PULLUP Dry Contact)
 *
 * Cloud Target:
 * - ThingSpeak Channel ID: 3475948 (Write API Key: 2W20O13FTT3CIUD3)
 * - Particle Cloud Telemetry (Variables & Events)
 *
 * Cadence: Sensor acquisition and ThingSpeak cloud transmission every 20 seconds.
 * Alert Thresholds:
 * - Buzzer & Visual Alert when (motion == 1 || distance < 20.0 cm)
 * ============================================================================
 */

#include "application.h"

// ----------------------------------------------------------------------------
// PIN ASSIGNMENTS (Spark Core <-> 9-in-1 Shield & Sensors)
// ----------------------------------------------------------------------------
const int PIN_DHT11       = D4;   // Shield D4: DHT11 Data (5V tolerant)
const int PIN_SZ_HS100    = A0;   // Analog Pin A0: SZ-HS100 Humidity Sensor (0-3.3V ADC)
const int PIN_BUZZER      = D5;   // Shield D5: Buzzer Transistor Input
const int PIN_RGB_RED     = A5;   // Shield D9: RGB Red
const int PIN_RGB_GREEN   = A6;   // Shield D10: RGB Green
const int PIN_RGB_BLUE    = A7;   // Shield D11: RGB Blue
const int PIN_LDR         = A1;   // Shield A1: LDR Light Sensor (0-4095 ADC)

const int PIN_TRIG        = D0;   // HC-SR04 Trigger (3.3V Output -> TTL compatible)
const int PIN_ECHO        = D1;   // HC-SR04 Echo (5V tolerant Input)
// PIR Motion Sensor Pin (Configured with internal pullup for 12V dry contact closed-circuit trigger)
const int PIN_PIR         = D3;   // Shield D3: PIR Motion (INPUT_PULLUP, Active-LOW)

// ----------------------------------------------------------------------------
// CLOUD CONFIGURATION
// ----------------------------------------------------------------------------
const char* THINGSPEAK_KEY  = "2W20O13FTT3CIUD3";
IPAddress   THINGSPEAK_IP(44, 213, 137, 49); // Direct IP
const int   THINGSPEAK_PORT = 80;

const unsigned long TRANSMIT_INTERVAL_MS = 20000; // 20s
const int           PROXIMITY_THRESHOLD  = 20;    // 20 cm

// ----------------------------------------------------------------------------
// HUMIDITY SENSOR SOURCE MODES
// ----------------------------------------------------------------------------
enum HumiditySourceMode {
    HUM_MODE_DHT11    = 0,  // DHT11 Digital Sensor (D4)
    HUM_MODE_SZ_HS100 = 1,  // SZ-HS100 Analog Sensor (A0)
    HUM_MODE_DUAL     = 2   // Dual mode (Reads both, reports analog primary)
};

// ----------------------------------------------------------------------------
// SENSOR DATA STATE (Integer representations: temp*10, hum*10, dist in cm)
// ----------------------------------------------------------------------------
int currentTemp   = 24;      // degrees C
int currentHum    = 55;      // active humidity percent
int currentDist   = 150;     // cm
int currentMotion = 0;       // 0 or 1
int currentLight  = 0;       // 0-4095
int isAlert       = 0;       // 0 or 1
int pirFilterCounter = 0;    // Anti-chatter filter for PIR contact
bool pirMonitoringEnabled = true; // Enabled by default; INPUT_PULLUP prevents floating false-alarms
int pirTriggerLevel = LOW;   // 12V PIR dry-contact closes circuit to GND on motion (Active-LOW: LOW=Motion)


int dhtTemp       = 24;      // DHT11 temperature
int dhtHum        = 55;      // DHT11 humidity
int szHum         = 55;      // SZ-HS100 analog humidity percent
int szRawAdc      = 2200;    // SZ-HS100 raw ADC reading (0-4095)
int humidityMode  = HUM_MODE_SZ_HS100; // Default active humidity mode

// SZ-HS100 Calibration: 12-bit ADC (0-4095) mapped to 0-3.3V
// Nominal probe: ~0.6V at 10% RH (~745 ADC) to ~3.0V at 90% RH (~3723 ADC)
int szAdcMin      = 745;     // ADC value for minimum calibrated RH
int szAdcMax      = 3723;    // ADC value for maximum calibrated RH
int szRhMin       = 10;      // Calibrated minimum RH percentage
int szRhMax       = 90;      // Calibrated maximum RH percentage

unsigned long lastTransmitTime = 0;
unsigned long lastSensorSample = 0;

TCPClient tcpClient;

// ----------------------------------------------------------------------------
// ZERO-DEPENDENCY ROBUST DHT11 DRIVER (Fixed-point, no float)
// ----------------------------------------------------------------------------
class DHT11Driver {
private:
    int _pin;
    int _lastT;
    int _lastH;
public:
    DHT11Driver(int pin) : _pin(pin), _lastT(24), _lastH(55) {}

    void begin() {
        pinMode(_pin, INPUT_PULLUP);
    }

    bool read(int &temp, int &hum) {
        uint8_t data[5] = {0, 0, 0, 0, 0};

        pinMode(_pin, OUTPUT);
        digitalWrite(_pin, LOW);
        delay(20);

        digitalWrite(_pin, HIGH);
        delayMicroseconds(30);
        pinMode(_pin, INPUT_PULLUP);

        unsigned long tStart = micros();
        while (digitalRead(_pin) == HIGH) {
            if (micros() - tStart > 100) { temp = _lastT; hum = _lastH; return false; }
        }

        tStart = micros();
        while (digitalRead(_pin) == LOW) {
            if (micros() - tStart > 100) { temp = _lastT; hum = _lastH; return false; }
        }

        tStart = micros();
        while (digitalRead(_pin) == HIGH) {
            if (micros() - tStart > 100) { temp = _lastT; hum = _lastH; return false; }
        }

        noInterrupts();
        for (int i = 0; i < 40; i++) {
            tStart = micros();
            while (digitalRead(_pin) == LOW) {
                if (micros() - tStart > 100) { interrupts(); temp = _lastT; hum = _lastH; return false; }
            }

            unsigned long pulseStart = micros();
            tStart = micros();
            while (digitalRead(_pin) == HIGH) {
                if (micros() - tStart > 150) { interrupts(); temp = _lastT; hum = _lastH; return false; }
            }
            unsigned long pulseLen = micros() - pulseStart;

            if (pulseLen > 40) {
                data[i / 8] |= (1 << (7 - (i % 8)));
            }
        }
        interrupts();

        uint8_t checksum = (data[0] + data[1] + data[2] + data[3]) & 0xFF;
        if (checksum != data[4] || (data[0] == 0 && data[2] == 0)) {
            temp = _lastT;
            hum = _lastH;
            return false;
        }

        hum  = data[0];
        temp = data[2];
        _lastT = temp;
        _lastH = hum;
        return true;
    }
};

DHT11Driver dht(PIN_DHT11);

// ----------------------------------------------------------------------------
// SZ-HS100 ANALOG HUMIDITY SENSOR DRIVER (12-bit ADC Oversampling & Mapping)
// ----------------------------------------------------------------------------
// Hardware Wiring Guide:
// - VCC (Red):    Connect to Spark Core 3V3 (or 5V with 2/3 voltage divider to OUT)
// - GND (Black):  Connect to Spark Core GND
// - OUT (Green):  Connect to Spark Core Pin A0 (ADC 0-3.3V)
//
// Calibration Constants:
// Default nominal range: 0.6V at 10% RH (~745 ADC) to 3.0V at 90% RH (~3723 ADC)
int readSzHs100Humidity() {
    uint32_t adcSum = 0;
    // 10-sample oversampling to suppress high-frequency noise on analog rails
    for (int i = 0; i < 10; i++) {
        adcSum += analogRead(PIN_SZ_HS100);
        delayMicroseconds(50);
    }
    szRawAdc = (int)(adcSum / 10);

    // Linear interpolation from ADC counts to Relative Humidity percentage
    long rh = map(szRawAdc, szAdcMin, szAdcMax, szRhMin, szRhMax);
    if (rh < 0)   rh = 0;
    if (rh > 100) rh = 100;
    szHum = (int)rh;
    return szHum;
}

// ----------------------------------------------------------------------------
// HC-SR04 ULTRASONIC SENSOR DRIVER (Integer cm)
// ----------------------------------------------------------------------------
uint32_t pulseInWithTimeout(int pin, int value, unsigned long timeoutUs) {
    unsigned long startWait = micros();
    while (digitalRead(pin) == value) {
        if (micros() - startWait > timeoutUs) return 0;
    }
    while (digitalRead(pin) != value) {
        if (micros() - startWait > timeoutUs) return 0;
    }
    unsigned long pulseStart = micros();
    while (digitalRead(pin) == value) {
        if (micros() - pulseStart > timeoutUs) return 0;
    }
    return micros() - pulseStart;
}

int readUltrasonicDistanceCm() {
    digitalWrite(PIN_TRIG, LOW);
    delayMicroseconds(2);
    digitalWrite(PIN_TRIG, HIGH);
    delayMicroseconds(10);
    digitalWrite(PIN_TRIG, LOW);

    unsigned long duration = pulseInWithTimeout(PIN_ECHO, HIGH, 30000);
    if (duration == 0) return 999;

    // duration / 58 = cm
    return (int)(duration / 58UL);
}

// ----------------------------------------------------------------------------
// BUZZER & RGB ALERT INDICATION
// ----------------------------------------------------------------------------
void setRgbColor(bool red, bool green, bool blue) {
    digitalWrite(PIN_RGB_RED,   red ? HIGH : LOW);
    digitalWrite(PIN_RGB_GREEN, green ? HIGH : LOW);
    digitalWrite(PIN_RGB_BLUE,  blue ? HIGH : LOW);
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

// Musical melodies instead of harsh buzzes
void playMelodyAlert() {
    // Dramatic minor arpeggio (A4 -> C5 -> E5 -> A5)
    playBuzzerTone(60, 440);
    delay(15);
    playBuzzerTone(60, 523);
    delay(15);
    playBuzzerTone(60, 659);
    delay(15);
    playBuzzerTone(120, 880);
}

void playMelodyMotion() {
    // Gentle 2-note musical chime (E5 -> B5)
    playBuzzerTone(50, 659);
    delay(15);
    playBuzzerTone(90, 988);
}

void updateAlerts(bool alertTriggered, bool isProximity, bool isMotion) {
    if (alertTriggered) {
        if (isProximity) {
            setRgbColor(true, false, false); // Red
            playMelodyAlert();
        } else if (isMotion && pirMonitoringEnabled) {
            setRgbColor(false, false, true); // Blue
            playMelodyMotion();
        }
    } else {
        setRgbColor(false, true, false);     // Green
        digitalWrite(PIN_BUZZER, LOW);
    }
}

// ----------------------------------------------------------------------------
// THINGSPEAK CLOUD DISPATCHER
// ----------------------------------------------------------------------------
bool sendToThingSpeak(int temp, int hum, int dist, int motion, int light) {
    if (!tcpClient.connect(THINGSPEAK_IP, THINGSPEAK_PORT)) {
        return false;
    }

    char req[128];
    snprintf(req, sizeof(req),
        "GET /update?api_key=%s&field1=%d&field2=%d&field3=%d&field4=%d&field5=%d HTTP/1.1\r\nHost: api.thingspeak.com\r\nConnection: close\r\n\r\n",
        THINGSPEAK_KEY, temp, hum, dist, motion, light);
    tcpClient.print(req);
    tcpClient.flush();

    unsigned long waitStart = millis();
    while (millis() - waitStart < 1000) {
        Particle.process();
        while (tcpClient.available()) tcpClient.read();
        if (!tcpClient.connected()) break;
    }
    tcpClient.stop();
    return true;
}

// ----------------------------------------------------------------------------
// REMOTE CLOUD COMMAND HANDLERS (Web Dashboard Interface)
// ----------------------------------------------------------------------------
int handleCloudCommand(String args) {
    if (args.length() == 0) return -1;
    char c = args.charAt(0);
    // Alarm & Visual controls
    if (c == 't' || c == '1') { playMelodyAlert(); return 1; }
    if (c == '0' || c == 'o') { digitalWrite(PIN_BUZZER, LOW); setRgbColor(false, true, false); return 0; }
    if (c == 'k')             { playMelodyMotion(); return 5; } // Gentle musical status motif
    if (c == 'r') { setRgbColor(true, false, false); return 2; }
    if (c == 'g') { setRgbColor(false, true, false); return 3; }
    if (c == 'b') { setRgbColor(false, false, true);  return 4; }
    // Sensor mode controls: 's' or 'a' = SZ-HS100 analog, 'd' = DHT11 digital
    if (c == 's' || c == 'a') { humidityMode = HUM_MODE_SZ_HS100; return 10; }
    if (c == 'd')             { humidityMode = HUM_MODE_DHT11;    return 11; }
    // PIR sensor controls:
    // 'p' toggles PIR monitoring on/off
    if (c == 'p')             { pirMonitoringEnabled = !pirMonitoringEnabled; return pirMonitoringEnabled ? 20 : 21; }
    // 'i' toggles PIR trigger polarity (Active-LOW = 22, Active-HIGH = 23)
    if (c == 'i')             { pirTriggerLevel = (pirTriggerLevel == LOW) ? HIGH : LOW; return (pirTriggerLevel == LOW) ? 22 : 23; }
    return -1;
}

// ----------------------------------------------------------------------------
// SETUP
// ----------------------------------------------------------------------------
void setup() {
    pinMode(PIN_BUZZER, OUTPUT);
    digitalWrite(PIN_BUZZER, LOW);

    pinMode(PIN_RGB_RED, OUTPUT);
    pinMode(PIN_RGB_GREEN, OUTPUT);
    pinMode(PIN_RGB_BLUE, OUTPUT);
    setRgbColor(false, true, false);

    pinMode(PIN_TRIG, OUTPUT);
    digitalWrite(PIN_TRIG, LOW);
    pinMode(PIN_ECHO, INPUT);

    // Configure PIN_PIR with internal pull-up resistor.
    // Floating/Open contact stays HIGH (3.3V, idle/no-motion).
    // When 12V PIR triggers, closed contact shorts D3 to GND (reads LOW).
    pinMode(PIN_PIR, INPUT_PULLUP);
    pinMode(PIN_LDR, INPUT);
    pinMode(PIN_SZ_HS100, INPUT); // Analog Pin A0 ADC

    dht.begin();

    // Particle Cloud Telemetry Variables
    Particle.variable("temp", currentTemp);
    Particle.variable("hum", currentHum);
    Particle.variable("dist", currentDist);
    Particle.variable("motion", currentMotion);
    Particle.variable("szHum", szHum);

    // Register remote cloud control functions
    Particle.function("alarm", handleCloudCommand);
    Particle.function("cmd", handleCloudCommand);
}

// ----------------------------------------------------------------------------
// MAIN LOOP
// ----------------------------------------------------------------------------
void loop() {
    Particle.process();
    unsigned long now = millis();

    if (now - lastSensorSample >= 250) {
        lastSensorSample = now;

        // Debounce / filter PIR motion to eliminate contact bounce and transient RF spikes
        int rawPir = digitalRead(PIN_PIR);
        bool isMotionTriggered = (rawPir == pirTriggerLevel);
        if (isMotionTriggered && pirMonitoringEnabled) {
            if (pirFilterCounter < 4) pirFilterCounter++;
        } else {
            if (pirFilterCounter > 0) pirFilterCounter--;
        }
        // Require at least 3 consecutive positive samples (750ms) to confirm motion
        currentMotion = (pirMonitoringEnabled && pirFilterCounter >= 3) ? 1 : 0;

        currentDist   = readUltrasonicDistanceCm();
        currentLight  = analogRead(PIN_LDR);

        // 1. Read DHT11 Digital Sensor (Temp + Humidity)
        dht.read(dhtTemp, dhtHum);
        currentTemp = dhtTemp;

        // 2. Read SZ-HS100 Analog Humidity Sensor (Pin A0)
        readSzHs100Humidity();

        // 3. Select Active Humidity based on configured mode
        if (humidityMode == HUM_MODE_SZ_HS100) {
            currentHum = szHum;
        } else {
            currentHum = dhtHum;
        }

        bool proxAlert = (currentDist > 0 && currentDist < PROXIMITY_THRESHOLD);
        bool motAlert  = (currentMotion == 1);
        isAlert        = (proxAlert || motAlert) ? 1 : 0;

        updateAlerts(isAlert == 1, proxAlert, motAlert);
    }

    if (now - lastTransmitTime >= TRANSMIT_INTERVAL_MS) {
        lastTransmitTime = now;

        sendToThingSpeak(currentTemp, currentHum, currentDist, currentMotion, currentLight);

        char payload[32];
        snprintf(payload, sizeof(payload), "%d,%d,%d,%d,%d", currentTemp, currentHum, currentDist, currentMotion, szHum);
        Particle.publish("smartroom", payload, PRIVATE);
    }
}
