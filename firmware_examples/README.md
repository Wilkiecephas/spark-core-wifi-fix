# SMART IOT HUB — HARDWARE INTEGRATION & CODE USAGE GUIDE
*Built by TekStep Apps Uganda (tekstepapps.org)*

This guide provides tested firmware sketches, wiring pinouts, and quickstart instructions to connect **Arduino**, **ESP32**, **ESP8266**, and **Spark Core / Particle Photon** microcontrollers directly to the Smart IoT Hub.

---

## 1. Quick Comparison of Connection Protocols

| Platform | Recommended Protocol | Latency | Cloud Needed? | Firmware File |
| :--- | :--- | :--- | :--- | :--- |
| **Arduino (Uno / Nano / Mega)** | WebSerial USB UART | ~15 ms | No (Direct Browser) | [`arduino_uno_hub_webserial.ino`](./arduino_uno_hub_webserial.ino) |
| **ESP32 (NodeMCU / WROOM)** | Wi-Fi REST Telemetry | ~40 ms | Local or Vercel | [`esp32_wifi_rest_telemetry.ino`](./esp32_wifi_rest_telemetry.ino) |
| **ESP32 (Direct Polling)** | Embedded HTTP Server | ~25 ms | No (Direct Local IP) | [`esp32_standalone_server.ino`](./esp32_standalone_server.ino) |
| **ESP8266 (NodeMCU / D1)** | Wi-Fi HTTP Client | ~50 ms | Local or Vercel | Use ESP32 sketch with `<ESP8266WiFi.h>` |
| **Spark Core / Photon** | Particle Cloud REST | ~180 ms | Yes (Worldwide access) | [`spark_core_photon_cloud.ino`](./spark_core_photon_cloud.ino) |

---

## 2. Arduino Uno / Nano / Mega Setup (WebSerial USB)

### Usage Steps:
1. Open [`arduino_uno_hub_webserial.ino`](./arduino_uno_hub_webserial.ino) in the **Arduino IDE**.
2. Select your board under **Tools > Board** (e.g. *Arduino Uno*) and select your **Port**.
3. Click **Upload**.
4. In your browser (Chrome, Edge, Opera), navigate to the Smart IoT Hub dashboard.
5. In the top toolbar, click **"Connect USB COM Port"** (or click "+ Add Device" -> "USB Serial").
6. Select your Arduino's serial port.
7. Telemetry immediately begins streaming live with interactive Sonar Radar, room occupancy, temperature, and light!

### Standard Arduino Pinout:
- **HC-SR04 Ultrasonic**: Trig -> `D7`, Echo -> `D8`, VCC -> `5V`, GND -> `GND`
- **HC-SR501 PIR Motion**: Output -> `D3`, VCC -> `5V`, GND -> `GND`
- **LDR Light Sensor**: Analog Pin `A1` (with 10k resistor to GND)
- **LM35 Temperature**: Analog Pin `A2` (or Potentiometer on `A0`)
- **Piezo Buzzer**: `D5` (Digital PWM)
- **RGB LED**: Red -> `D9`, Green -> `D10`, Blue -> `D11`

---

## 3. ESP32 Wi-Fi REST Setup

### Usage Steps:
1. Open [`esp32_wifi_rest_telemetry.ino`](./esp32_wifi_rest_telemetry.ino) in the **Arduino IDE**.
2. Install the **ArduinoJson** library via **Tools > Manage Libraries**.
3. Edit the top configuration section:
   ```cpp
   const char* WIFI_SSID     = "Your_WiFi_Network";
   const char* WIFI_PASS     = "Your_WiFi_Password";
   const char* TELEMETRY_URL = "http://192.168.1.100:5173/api/telemetry?device=dev_esp32_node";
   const char* DEVICE_ID     = "dev_esp32_node";
   ```
4. Select your ESP32 board and click **Upload**.
5. Once booted, the ESP32 automatically connects to your Wi-Fi and streams JSON packets every 2 seconds.
6. In Smart IoT Hub, your ESP32 will be recognized and displayed in the device list!

---

## 4. Spark Core / Particle Photon Setup (Particle Cloud)

### Usage Steps:
1. Copy the code from [`spark_core_photon_cloud.ino`](./spark_core_photon_cloud.ino).
2. Paste it into the **Particle Web IDE** ([build.particle.io](https://build.particle.io)) or flash via Particle CLI:
   ```bash
   particle flash <device_name> spark_core_photon_cloud.ino
   ```
3. Open your [Particle Console](https://console.particle.io) to obtain:
   - **Device ID** (e.g. `54ff74066678574924331067`)
   - **Access Token** (from Settings > Access Tokens)
4. In the Smart IoT Hub dashboard:
   - Click **"+ Add Device"** or click **"Particle Spark Core / Photon"** in the Blank Workspace.
   - Enter your Device ID and Access Token.
   - Click **Save & Connect**.
5. The hub immediately streams telemetry from your Spark Core over Particle Cloud with worldwide access!

---

## 5. Telemetry JSON Schema Specification

When streaming your own custom sensors, output JSON using this schema:

```json
{
  "deviceId": "my_custom_mcu",
  "temperature": 24.5,
  "humidity": 55.0,
  "distance": 145.0,
  "motion": 0,
  "light": 650,
  "battery": 98.5
}
```

Smart IoT Hub automatically parses these fields and updates the Sonar Scope, environmental gauges, and AI diagnostics.
