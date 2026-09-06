/*
 * ============================================================================
 * SMART ROOM MONITORING SYSTEM (SPARK CORE NATIVE) - CALIBRATED & STABILIZED
 * ============================================================================
 * Pin Mapping:
 * - D0 & D1   -> HC-SR04 Ultrasonic Sonar (D1=Trig, D0=Echo; reverse via cloud)
 * - D2 (SW1)  -> Push Button SW1 (Active-LOW: Mutes buzzer / clears alert)
 * - D4        -> DHT11 Digital Temperature & Humidity Sensor (Read every 2.5s)
 * - D5        -> Active Buzzer (Direct logic drive; quiet & calm, no screeching)
 * - D6        -> Infrared (IR) Obstacle / Proximity Sensor (Active-LOW)
 * - D7        -> Spark Core Onboard Blue LED (Alert visual indicator)
 * - A0        -> Rotary Potentiometer / Secondary Humidity (0-3.3V ADC)
 * - A1        -> UNUSED (Completely removed to eliminate 5V rail interference)
 * - A2        -> LM35 Precision Analog Temperature Sensor (10mV/°C)
 * ============================================================================
 */

#include "application.h"

// Hardware Pin Definitions
int pinTrig             = D1;   // Default Trig (Auto-tested / cloud-swappable)
int pinEcho             = D0;   // Default Echo
const int PIN_SW1       = D2;   // SW1 Button (Mute)
const int PIN_DHT11     = D4;   // DHT11 Sensor Data
const int PIN_BUZZER    = D5;   // Active Buzzer on Shield
const int PIN_IR_D6     = D6;   // IR Obstacle Sensor
const int PIN_LED_D7    = D7;   // Spark Core Onboard Blue LED

const int PIN_POT_A0    = A0;   // Shield Potentiometer
const int PIN_LM35_A2   = A2;   // LM35 Analog Temperature Sensor

// Thresholds
const int PROXIMITY_THRESHOLD_CM = 40;   // Alert threshold (< 40 cm)
const unsigned long DHT_SAMPLE_MS = 2500; // DHT11 sample interval (2.5s)
const unsigned long TELEMETRY_MS  = 20000; // Particle publish interval (20s)

// State Variables (Spark Core supports max 4 variables reliably)
int currentDist   = 120;   // Distance in cm
int currentTemp   = 24;    // Temperature in deg C
int currentHum    = 50;    // Humidity in % RH
int currentMotion = 0;     // 0=Clear, 1=Object close to IR sensor

// Internal Diagnostics
int tempDht       = 0;
int humDht        = 0;
int tempLm35      = 24;
int rawA2         = 0;
int rawA0         = 0;
int dhtOk         = 0;
int ultrasonicMode= 0;     // 0: D1=Trig, D0=Echo; 1: D0=Trig, D1=Echo

bool buzzerMuted      = false;
bool buzzerActiveLow  = true;  // 9-in-1 active buzzer: LOW=On, HIGH=Off

unsigned long lastFastLoopTime = 0;
unsigned long lastDhtTime      = 0;
unsigned long lastChirpTime    = 0;
unsigned long lastPublishTime  = 0;
int  irFilterCount     = 0;
int  distHistory[3]    = {120, 120, 120};
int  distHistIdx       = 0;

// ----------------------------------------------------------------------------
// BUZZER CONTROL (Quiet & Gentle, No Screeching)
// ----------------------------------------------------------------------------
void setBuzzer(bool on) {
    if (on && !buzzerMuted) {
        digitalWrite(PIN_BUZZER, buzzerActiveLow ? LOW : HIGH);
    } else {
        digitalWrite(PIN_BUZZER, buzzerActiveLow ? HIGH : LOW);
    }
}

void playSoftChirp(int durationMs) {
    if (buzzerMuted || durationMs <= 0) return;
    setBuzzer(true);
    delay(durationMs);
    setBuzzer(false);
}

// ----------------------------------------------------------------------------
// ULTRASONIC SENSOR DRIVER (HC-SR04)
// ----------------------------------------------------------------------------
int pingOnce(int tPin, int ePin) {
    digitalWrite(tPin, LOW);
    delayMicroseconds(4);
    digitalWrite(tPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(tPin, LOW);

    // Wait for Echo to rise (Timeout 6ms)
    unsigned long t0 = micros();
    while (digitalRead(ePin) == LOW) {
        if (micros() - t0 > 6000) return 0;
    }

    // Measure Echo HIGH duration (Timeout 25ms = ~430 cm)
    unsigned long tStart = micros();
    while (digitalRead(ePin) == HIGH) {
        if (micros() - tStart > 25000) return 0;
    }
    unsigned long echoUs = micros() - tStart;
    if (echoUs < 116) return 0;
    return (int)(echoUs / 58UL);
}

int getFilteredDistance() {
    int sample = pingOnce(pinTrig, pinEcho);
    if (sample >= 2 && sample <= 400) {
        distHistory[distHistIdx] = sample;
        distHistIdx = (distHistIdx + 1) % 3;
    }
    int a = distHistory[0], b = distHistory[1], c = distHistory[2];
    int med = a;
    if ((a <= b && b <= c) || (c <= b && b <= a)) med = b;
    else if ((b <= a && a <= c) || (c <= a && a <= b)) med = a;
    else med = c;
    return med;
}

// ----------------------------------------------------------------------------
// DHT11 SENSOR DRIVER (Digital Pin D4)
// ----------------------------------------------------------------------------
bool readDHT(int &tOut, int &hOut) {
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

    if (((data[0] + data[1] + data[2] + data[3]) & 0xFF) != data[4]) return false;
    if (data[0] == 0 && data[2] == 0) return false;

    hOut = data[0];
    tOut = data[2];
    return true;
}

// ----------------------------------------------------------------------------
// LM35 TEMPERATURE DRIVER (Analog Pin A2)
// ----------------------------------------------------------------------------
int readLM35() {
    uint32_t sum = 0;
    for (int i = 0; i < 16; i++) {
        sum += analogRead(PIN_LM35_A2);
        delayMicroseconds(30);
    }
    rawA2 = (int)(sum / 16);
    float mv = ((float)rawA2 * 3300.0f) / 4095.0f;
    int c = (int)(mv / 10.0f + 0.5f);
    if (c >= 2 && c <= 80) tempLm35 = c;
    return tempLm35;
}

// ----------------------------------------------------------------------------
// POTENTIOMETER / ANALOG HUMIDITY (Pin A0)
// ----------------------------------------------------------------------------
int readPotHumidity() {
    uint32_t sum = 0;
    for (int i = 0; i < 8; i++) {
        sum += analogRead(PIN_POT_A0);
        delayMicroseconds(20);
    }
    rawA0 = (int)(sum / 8);
    int rh = (int)map(rawA0, 0, 4095, 10, 95);
    if (rh < 5)  rh = 5;
    if (rh > 99) rh = 99;
    return rh;
}

// ----------------------------------------------------------------------------
// PIN CONFIGURATION & COMMAND HANDLER
// ----------------------------------------------------------------------------
void setUltrasonicMode(int mode) {
    ultrasonicMode = mode;
    if (ultrasonicMode == 0) {
        pinTrig = D1; pinEcho = D0;
    } else {
        pinTrig = D0; pinEcho = D1;
    }
    pinMode(pinTrig, OUTPUT);
    digitalWrite(pinTrig, LOW);
    pinMode(pinEcho, INPUT);
}

int handleCommand(String args) {
    if (args.length() == 0) return -1;
    char c = args.charAt(0);

    if (c == 'm') {
        buzzerMuted = !buzzerMuted;
        setBuzzer(false);
        return buzzerMuted ? 10 : 11;
    }
    if (c == 'u' || c == 's') {
        setUltrasonicMode(ultrasonicMode == 0 ? 1 : 0);
        return ultrasonicMode == 0 ? 30 : 31;
    }
    if (c == 'p') {
        buzzerActiveLow = !buzzerActiveLow;
        setBuzzer(false);
        return buzzerActiveLow ? 40 : 41;
    }
    if (c == 't' || c == '1') {
        playSoftChirp(40);
        return 1;
    }
    if (c == '0' || c == 'o') {
        setBuzzer(false);
        digitalWrite(PIN_LED_D7, LOW);
        return 0;
    }
    return -1;
}

// ----------------------------------------------------------------------------
// SETUP
// ----------------------------------------------------------------------------
void setup() {
    // 1. Buzzer Silent on Boot
    pinMode(PIN_BUZZER, OUTPUT);
    setBuzzer(false);

    // 2. Onboard Blue LED (D7)
    pinMode(PIN_LED_D7, OUTPUT);
    digitalWrite(PIN_LED_D7, LOW);

    // 3. Inputs
    pinMode(PIN_SW1,    INPUT_PULLUP);
    pinMode(PIN_IR_D6,  INPUT_PULLUP);
    pinMode(PIN_DHT11,  INPUT_PULLUP);
    pinMode(PIN_POT_A0, INPUT);
    pinMode(PIN_LM35_A2, INPUT);
    // Pin A1 is intentionally excluded from firmware

    // 4. Ultrasonic Sonar
    setUltrasonicMode(0);

    // 5. Short 25ms soft chirp on boot
    playSoftChirp(25);

    // 6. Register Exactly 4 Particle Cloud Variables (Spark Core standard limit)
    Particle.variable("temp",   currentTemp);
    Particle.variable("hum",    currentHum);
    Particle.variable("dist",   currentDist);
    Particle.variable("motion", currentMotion);

    // 7. Register Cloud Command Function
    Particle.function("alarm", handleCommand);
}

// ----------------------------------------------------------------------------
// MAIN LOOP
// ----------------------------------------------------------------------------
void loop() {
    Particle.process();
    unsigned long now = millis();

    // Hardware Mute Button SW1 (Pin D2)
    if (digitalRead(PIN_SW1) == LOW) {
        buzzerMuted = true;
        setBuzzer(false);
        digitalWrite(PIN_LED_D7, LOW);
        delay(120);
    }

    // Slow Sensor Cycle (DHT11 & LM35 read every 2.5s)
    if (now - lastDhtTime >= DHT_SAMPLE_MS) {
        lastDhtTime = now;

        int t = 0, h = 0;
        if (readDHT(t, h)) {
            tempDht = t;
            humDht  = h;
            dhtOk   = 1;
        } else {
            dhtOk   = 0;
        }

        readLM35();

        // Primary Temperature Selection
        if (dhtOk == 1 && tempDht >= 5 && tempDht <= 60) {
            currentTemp = tempDht;
        } else if (tempLm35 >= 5 && tempLm35 <= 75) {
            currentTemp = tempLm35;
        }

        // Primary Humidity Selection
        if (dhtOk == 1 && humDht >= 10 && humDht <= 95) {
            currentHum = humDht;
        } else {
            currentHum = readPotHumidity();
        }
    }

    // Fast Sensor Cycle (Ultrasonic & IR read every 100ms)
    if (now - lastFastLoopTime >= 100) {
        lastFastLoopTime = now;

        currentDist = getFilteredDistance();

        // IR Obstacle Sensor (Active-LOW on D6)
        if (digitalRead(PIN_IR_D6) == LOW) {
            if (irFilterCount < 3) irFilterCount++;
        } else {
            if (irFilterCount > 0) irFilterCount--;
        }
        currentMotion = (irFilterCount >= 2) ? 1 : 0;

        // Alert Condition: Distance < 40 cm OR IR detected
        bool proxBreach = (currentDist > 0 && currentDist < PROXIMITY_THRESHOLD_CM);
        bool irBreach   = (currentMotion == 1);
        bool breach     = (proxBreach || irBreach);

        if (breach) {
            digitalWrite(PIN_LED_D7, (now % 300 < 150) ? HIGH : LOW);
            // Calm, non-annoying pulse spaced 2.2 seconds apart
            if (now - lastChirpTime >= 2200) {
                lastChirpTime = now;
                playSoftChirp(35);
            }
        } else {
            digitalWrite(PIN_LED_D7, LOW);
            setBuzzer(false);
            buzzerMuted = false;
        }
    }

    // Telemetry Broadcast every 20s
    if (now - lastPublishTime >= TELEMETRY_MS) {
        lastPublishTime = now;
        char payload[40];
        snprintf(payload, sizeof(payload), "%d,%d,%d,%d,%d,%d",
                 currentTemp, currentHum, currentDist, currentMotion, tempLm35, dhtOk);
        Particle.publish("smartroom", payload, PRIVATE);
    }
}
