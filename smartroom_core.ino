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

// ----------------------------------------------------------------------------
// PIN ASSIGNMENTS
// ----------------------------------------------------------------------------
int pinTrig               = D0;   // Confirmed D0 Trig
int pinEcho               = D1;   // Confirmed D1 Echo
const int PIN_SW1         = D2;   // Push Button SW1 (Mute)
const int PIN_PIR_D3      = D3;   // PIR Motion Sensor / Key2
const int PIN_DHT11       = D4;   // DHT11 Data
const int PIN_BUZZER      = D5;   // Buzzer Pin
const int PIN_IR_D6       = D6;   // IR Receiver
const int PIN_LED_D7      = D7;   // Spark Core Onboard Blue LED

// Analog Sensor Inputs (A0-A4)
const int PIN_POT_A0      = A0;   // Rotary Potentiometer
const int PIN_LDR_A1      = A1;   // LDR Light Sensor
const int PIN_LM35_A2     = A2;   // LM35 Temperature Sensor
const int PIN_AUX_A3      = A3;   // Aux Analog 1
const int PIN_AUX_A4      = A4;   // Aux Analog 2

// External RGB LED Outputs (A5-A7)
const int PIN_RGB_RED     = A5;   // Red LED
const int PIN_RGB_GREEN   = A6;   // Green LED
const int PIN_RGB_BLUE    = A7;   // Blue LED

// Timing & Thresholds (Ultrasonic proximity threshold changed to 20 cm)
const int PROXIMITY_ALERT_CM   = 20;    // Alert if obstacle < 20 cm (per user request)
const unsigned long DHT_SAMPLE_MS     = 2500;  // DHT11 sample interval (2.5s)
const unsigned long FAST_LOOP_MS      = 100;   // Fast loop (100ms - ultra-responsive)
const unsigned long TELEMETRY_MS      = 10000; // Cloud publish interval (10s)
const unsigned long MOTION_HOLD_MS    = 1800;  // 1.8s hold for crisp, snappy motion triggers

// State Variables (Published to Particle Cloud)
int currentDist   = 150;   // Distance in cm
int currentTemp   = 25;    // Temperature in °C
int currentHum    = 50;    // Relative Humidity (% RH)
int currentMotion = 32;    // Bitmask of active triggers & LED states (Green = 32)
int currentLight  = 800;   // LDR Light Sensor reading (0-4095)
int currentPot    = 2048;  // Rotary Potentiometer reading (0-4095)

// Auxiliary Analog Channels
int valLm35Temp   = 25;
int valAuxA3      = 0;
int valAuxA4      = 0;

// Dynamic Sensitivity Tracking
int lastReportedPot   = 2048;
int lastReportedLdr   = 800;
int lastMotionState   = 0;
unsigned int irActiveBurstCount = 0; // Continuous microsecond pulse counter for IR intrusion
unsigned long lastIrHitTime     = 0; // Timestamp of last valid IR pulse

bool buzzerMuted      = false;
bool forceLightOn     = false;
bool forceAlarmOn     = false;
unsigned long motionHoldUntil   = 0;
unsigned long lastFastLoopTime  = 0;
unsigned long lastDhtTime       = 0;
unsigned long lastAlarmToneTime = 0;
unsigned long lastPublishTime   = 0;

// Filter history for distance (3 samples)
int distHistory[3] = {150, 150, 150};
int distHistIdx    = 0;

// ----------------------------------------------------------------------------
// BUZZER MELODIC AUDIO ENGINE
// ----------------------------------------------------------------------------
void playTone(int durationMs, int freqHz = 2000) {
    if (buzzerMuted || durationMs <= 0 || freqHz <= 0) return;
    Particle.process();
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

// 1. Boot / Welcome Melody (Smooth ascending harmonic chime: C5 -> E5 -> G5 -> C6)
void playWelcomeChime() {
    if (buzzerMuted) return;
    playTone(50, 523);   // C5
    delay(20);
    playTone(50, 659);   // E5
    delay(20);
    playTone(60, 784);   // G5
    delay(20);
    playTone(110, 1047); // C6 smooth sustain
    buzzerOff();
}

// 2. Motion / Notification Melody (Pleasant 2-note chime: E5 -> B5)
void playMelodyMotion() {
    if (buzzerMuted) return;
    playTone(45, 659);
    delay(15);
    playTone(85, 988);
    buzzerOff();
}

// 3. Proximity Alarm Siren (< 20cm: Urgent warble)
void playIntrusionAlarm() {
    if (buzzerMuted) return;
    playTone(60, 880);  // A5
    delay(20);
    playTone(60, 698);  // F5
    delay(20);
    playTone(80, 880);  // A5
    buzzerOff();
}

// 4. Safe State Restored Melody (Gentle resolving motif: G5 -> C6)
void playMelodySafe() {
    if (buzzerMuted) return;
    playTone(35, 784);
    delay(15);
    playTone(65, 1047);
    buzzerOff();
}

// ----------------------------------------------------------------------------
// SENSOR BOARD RGB LED CONTROLLER (A5: Red, A6: Green, A7: Blue, D7: Onboard Heartbeat)
// Spark Core onboard RGB LED is dedicated MAIN LIGHT (Continuous low smooth Cyan glow)
// ----------------------------------------------------------------------------
void updateLeds(bool isAlarm, bool isMotion, unsigned long now) {
    // Spark Core RGB Main Light is strictly isolated: holds serene low smooth Cyan glow.
    // Sensor board external RGB LEDs handle all security notifications:
    if (isAlarm || forceAlarmOn) {
        // PROXIMITY BREACH (< 20cm) / ALARM: Sensor Board RED LED
        digitalWrite(PIN_RGB_RED,   HIGH);
        digitalWrite(PIN_RGB_GREEN, LOW);
        digitalWrite(PIN_RGB_BLUE,  LOW);

        // Fast 5Hz D7 onboard strobe for alarm alert
        digitalWrite(PIN_LED_D7, (now % 200 < 100) ? HIGH : LOW);
    } else if (isMotion) {
        // INTRUSION / PIR MOTION / IR ACTIVE: Sensor Board BLUE LED
        digitalWrite(PIN_RGB_RED,   LOW);
        digitalWrite(PIN_RGB_GREEN, LOW);
        digitalWrite(PIN_RGB_BLUE,  HIGH);

        // Solid / medium pulse on D7
        digitalWrite(PIN_LED_D7, (now % 400 < 200) ? HIGH : LOW);
    } else {
        // NORMAL / SAFE STATE: Sensor Board GREEN LED
        digitalWrite(PIN_RGB_RED,   LOW);
        digitalWrite(PIN_RGB_GREEN, HIGH);
        digitalWrite(PIN_RGB_BLUE,  LOW);

        // Calm 1Hz heartbeat pulse on D7 (80ms pulse every 1000ms: visual proof of running)
        digitalWrite(PIN_LED_D7, (forceLightOn || (now % 1000 < 80)) ? HIGH : LOW);
    }
}

// ----------------------------------------------------------------------------
// AUTO-CALIBRATING ULTRASONIC DRIVER (D0/D1)
// ----------------------------------------------------------------------------
void configureUltrasonicPins() {
    pinMode(pinTrig, OUTPUT);
    digitalWrite(pinTrig, LOW);
    pinMode(pinEcho, INPUT);
}

void swapUltrasonicPins() {
    int temp = pinTrig;
    pinTrig = pinEcho;
    pinEcho = temp;
    configureUltrasonicPins();
}

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

int pingPair(int tPin, int ePin) {
    pinMode(tPin, OUTPUT);
    digitalWrite(tPin, LOW);
    pinMode(ePin, INPUT);
    delayMicroseconds(4);
    digitalWrite(tPin, HIGH);
    delayMicroseconds(12);
    digitalWrite(tPin, LOW);

    // Timeout shortened to 16000us (275cm range max) to eliminate long busy-loop freezing
    unsigned long duration = pulseInWithTimeout(ePin, HIGH, 16000);
    if (duration < 116 || duration > 16000) return 0;
    return (int)(duration / 58UL);
}

int readUltrasonicDistanceCm() {
    int cm = pingPair(pinTrig, pinEcho);
    static int consecutiveFails = 0;
    if (cm <= 0 || cm > 400) {
        consecutiveFails++;
        if (consecutiveFails >= 3) {
            consecutiveFails = 0;
            // Try reverse configuration only after 3 consecutive failures
            int altTrig = (pinTrig == D0) ? D1 : D0;
            int altEcho = (pinEcho == D0) ? D1 : D0;
            int altCm = pingPair(altTrig, altEcho);
            if (altCm >= 2 && altCm <= 400) {
                pinTrig = altTrig;
                pinEcho = altEcho;
                cm = altCm;
            }
        }
    } else {
        consecutiveFails = 0;
    }

    if (cm >= 2 && cm <= 400) {
        distHistory[distHistIdx] = cm;
        distHistIdx = (distHistIdx + 1) % 3;
    }

    int a = distHistory[0], b = distHistory[1], c = distHistory[2];
    int median = a;
    if ((a <= b && b <= c) || (c <= b && b <= a)) median = b;
    else if ((b <= a && a <= c) || (c <= a && a <= b)) median = a;
    else median = c;

    return median;
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
// CLOUD COMMAND HANDLER
// ----------------------------------------------------------------------------
int handleCommand(String args) {
    if (args.length() == 0) return -1;
    char c = args.charAt(0);
    char sub = args.length() > 1 ? args.charAt(1) : 0;

    // Pin pulse diagnostics: p0 = pulse D0/measure D1, p1 = pulse D1/measure D0
    if (c == 'p') {
        if (sub == '0') return pingPair(D0, D1);
        if (sub == '1') return pingPair(D1, D0);
    }

    // Digital read diagnostics: r0=D0, r1=D1, r2=D2, r3=D3, r6=D6
    if (c == 'r') {
        if (sub == '0') return digitalRead(D0);
        if (sub == '1') return digitalRead(D1);
        if (sub == '2') return digitalRead(D2);
        if (sub == '3') return digitalRead(D3);
        if (sub == '6') return digitalRead(D6);
    }

    // Analog read diagnostics: a0=A0, a1=A1, a2=A2, a3=A3, a4=A4
    if (c == 'a') {
        if (sub == '0') return analogRead(A0);
        if (sub == '1') return analogRead(A1);
        if (sub == '2') return analogRead(A2);
        if (sub == '3') return analogRead(A3);
        if (sub == '4') return analogRead(A4);
    }

    // Mute / Unmute Buzzer
    if (c == 'm') {
        buzzerMuted = !buzzerMuted;
        buzzerOff();
        return buzzerMuted ? 10 : 11;
    }
    // Test Melodies: 't' or '1' = Welcome, 'k' = Motion chime, 's' = Siren
    if (c == 't' || c == '1') {
        playWelcomeChime();
        return 1;
    }
    if (c == 'k') {
        playMelodyMotion();
        return 5;
    }
    if (c == 's') {
        playIntrusionAlarm();
        return 2;
    }
    // Silence All Alarms
    if (c == '0' || c == 'o') {
        forceAlarmOn = false;
        forceLightOn = false;
        buzzerOff();
        updateLeds(false, false, millis());
        return 0;
    }
    // Toggle Light ON/OFF
    if (c == 'l') {
        forceLightOn = !forceLightOn;
        return forceLightOn ? 20 : 21;
    }
    // Swap Ultrasonic Trig / Echo pins dynamically ('u')
    if (c == 'u') {
        swapUltrasonicPins();
        return (pinTrig == D0) ? 30 : 31;
    }
    // Probe Ultrasonic Distance
    if (c == 'c') {
        distHistory[0] = 150; distHistory[1] = 150; distHistory[2] = 150;
        int d = readUltrasonicDistanceCm();
        playTone(30, 2400);
        return d;
    }
    return -1;
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

    // 5. Analog Inputs (A0-A4)
    pinMode(PIN_POT_A0,  INPUT);
    pinMode(PIN_LDR_A1,  INPUT);
    pinMode(PIN_LM35_A2, INPUT);
    pinMode(PIN_AUX_A3,  INPUT);
    pinMode(PIN_AUX_A4,  INPUT);

    // 6. Play Welcome Melodic Chime (Smooth harmonic sequence)
    playWelcomeChime();

    // 7. Initial Sensor Samples (LM35 calibrated to normal ambient temperature)
    currentPot   = analogRead(PIN_POT_A0);
    currentLight = analogRead(PIN_LDR_A1);
    valAuxA3     = analogRead(PIN_AUX_A3);
    valAuxA4     = analogRead(PIN_AUX_A4);
    int initRawA2 = analogRead(PIN_LM35_A2);
    float initLmMv = ((float)initRawA2 * 3300.0f) / 4095.0f;
    float initCal = initLmMv / 20.4f; // 520mV -> 25.5°C real temperature
    if (initCal < 10.0f || initCal > 60.0f) initCal = 25.0f;
    valLm35Temp  = (int)(initCal + 0.5f);
    lastReportedPot = currentPot;
    lastReportedLdr = currentLight;

    int t = 0, h = 0;
    if (readDHT11(t, h) && t >= 5 && t <= 55) {
        currentTemp = t; currentHum = h;
    } else {
        currentTemp = 25; currentHum = 50;
    }

    // 8. Register Particle Cloud Variables (Full Sensor Suite)
    Particle.variable("temp",   currentTemp);
    Particle.variable("hum",    currentHum);
    Particle.variable("dist",   currentDist);
    Particle.variable("motion", currentMotion);
    Particle.variable("light",  currentLight);
    Particle.variable("pot",    currentPot);
    Particle.variable("temp2",  valLm35Temp);
    Particle.variable("aux3",   valAuxA3);
    Particle.variable("aux4",   valAuxA4);

    // 9. Register Cloud Functions
    Particle.function("alarm", handleCommand);
    Particle.function("cmd",   handleCommand);
}

// ----------------------------------------------------------------------------
// MAIN LOOP
// ----------------------------------------------------------------------------
void loop() {
    Particle.process();
    unsigned long now = millis();

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
        Particle.process();
        bool dhtSuccess = (readDHT11(t, h) && t >= 5 && t <= 55);
        Particle.process();
        if (dhtSuccess) {
            currentTemp = t;
            currentHum  = h;
        }

        // Auxiliary analog samples & LM35 Temperature Calibration
        int rawA2 = analogRead(PIN_LM35_A2);
        valAuxA3 = analogRead(PIN_AUX_A3);
        valAuxA4 = analogRead(PIN_AUX_A4);

        float lm35Mv = ((float)rawA2 * 3300.0f) / 4095.0f;
        // Calibrate LM35: on 9-in-1 shield, ~500-540mV maps to ~25.5°C real ambient room temperature
        float calTemp = lm35Mv / 20.4f;
        if (calTemp < 10.0f || calTemp > 60.0f) {
            calTemp = 25.0f;
        }
        valLm35Temp = (int)(calTemp + 0.5f);

        if (!dhtSuccess && valLm35Temp >= 15 && valLm35Temp <= 45) {
            currentTemp = valLm35Temp;
        }
    }

    // 3. Fast Sensor Cycle (Ultrasonic, IR, Rotation & LDR sampled every 100ms)
    if (now - lastFastLoopTime >= FAST_LOOP_MS) {
        lastFastLoopTime = now;

        // A. Ultrasonic Distance Measurement
        currentDist = readUltrasonicDistanceCm();
        Particle.process();

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

    // 4. Telemetry Broadcast every 10 seconds
    if (now - lastPublishTime >= TELEMETRY_MS) {
        lastPublishTime = now;
        char payload[64];
        snprintf(payload, sizeof(payload), "%d,%d,%d,%d,%d,%d",
                 currentTemp, currentHum, currentDist, currentMotion, currentLight, currentPot);
        Particle.publish("smartroom", payload, PRIVATE);
    }
}
