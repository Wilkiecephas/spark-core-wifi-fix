#include "Particle.h"

// System mode AUTOMATIC connects Wi-Fi and Cloud
SYSTEM_MODE(AUTOMATIC);

// Log all system, network, and cloud events over USB Serial
SerialLogHandler logHandler(LOG_LEVEL_ALL);

void setup() {
    Serial.begin(115200);
}

void loop() {
    static uint32_t last = 0;
    if (millis() - last > 3000) {
        last = millis();
        IPAddress ip = WiFi.localIP();
        Log.info("WiFi Ready: %d | IP: %d.%d.%d.%d | Cloud Connected: %d",
            WiFi.ready(), ip[0], ip[1], ip[2], ip[3], Particle.connected());
    }
}
