# Spark Core (STM32F103 + TI CC3000) Wi-Fi Fix & SmartRoom Firmware Suite

A complete hardware recovery kit, cloud reconnection fix, and native IoT firmware for the **Spark Core (Gen 1)** microcontroller (`STM32F103CB` ARM Cortex-M3 + `TI CC3000` Wi-Fi module).

Includes native driver support for the **SZ-HS100 Analog Humidity Sensor**, DHT11, HC-SR04 ultrasonic distance, PIR motion detection, and dual-cloud telemetry (ThingSpeak + Particle Cloud).

---

## 🛠️ The 4 Wi-Fi & Cloud Connection Root Causes Solved

If your Spark Core blinks cyan with red flashes, blinks magenta endlessly, or resets continuously, it is experiencing one or more of these modern infrastructure incompatibility issues:

### 1. The TI CC3000 DNS Recursion Failure (Permanent Hang)
- **Symptom:** Core connects to local 2.4 GHz Wi-Fi (blinking green), turns cyan, and then either hangs indefinitely or flashes red and reboots.
- **Root Cause:** Modern Particle Cloud uses multi-level AWS CNAME alias records (`device.spark.io` $\rightarrow$ `device.tcp.particle.io` $\rightarrow$ `...` $\rightarrow$ `52.71.103.125`). The legacy 2013 TI CC3000 Wi-Fi firmware cannot process recursive CNAME responses and halts until the WLAN watchdog triggers a reset.
- **The Fix:** We embed the direct IPv4 address `52.71.103.125` directly into the SPI server key (`core_server_key_ip.der`) at offset 384, completely bypassing the CC3000 DNS resolver.

### 2. Server Key Port 65535 Bug
- **Symptom:** Immediate red flash upon attempting cloud handshake.
- **Root Cause:** Official `particle-cli` assets have offset 450 set to `0xFF 0xFF` (Port 65535) instead of `0x16 0x33` (Port 5683 CoAP). The Core attempts to connect to port 65535, causing an immediate TCP connection reset.
- **The Fix:** Byte offset 450 is patched to `5683` (`0x16 0x33`).

### 3. Modular vs Monolithic Firmware Crash
- **Symptom:** Immediate SOS crash (hard fault) on boot.
- **Root Cause:** Modern Particle CLI expects Gen 2 (Photon STM32F205) modular binaries. Flashing modular system parts to a Spark Core overwrites internal flash and crashes the Cortex-M3 core.
- **The Fix:** Compile monolithic binaries targeted for Core (`0x08005000`) and flash via `dfu-util`.

### 4. Device Private Key Sector Alignment
- **Root Cause:** Device private RSA key on external SPI flash SST25x (`0x00002000`) must be strictly padded to 1024 bytes. Unpadded keys corrupt subsequent sectors.
- **The Fix:** Use `core_private_padded.der` aligned to 1024 bytes.

---

## ⚡ Quick 1-Click Restoration

### Using the Automated Script:
1. Put the Spark Core in **DFU Mode** (Hold `MODE` + `RESET`, release `RESET`, release `MODE` when blinking yellow).
2. Double-click [`restore_spark_core.bat`](./restore_spark_core.bat) (or run `./restore_spark_core.ps1` in PowerShell).
3. The script automatically:
   - Flashes the aligned device private key to `0x00002000` (SPI Flash alt 1).
   - Flashes the Direct-IP patched server key (`core_server_key_ip.der`) to `0x00001000` (SPI Flash alt 1).
   - Flashes monolithic firmware (`tinker_core.bin` or `smartroom_core.bin`) to `0x08005000` (Internal Flash alt 0).
   - Reboots the Core.

---

## 📡 Configuring 2.4 GHz Wi-Fi

The CC3000 **only supports 2.4 GHz 802.11b/g networks** with WPA2_AES or WPA/WEP.

1. Put the Core in **Listening Mode** (Hold `MODE` for 3 seconds until blinking blue).
2. Copy [`wifi_credentials.example.json`](./wifi_credentials.example.json) to `wifi_credentials.json` and fill in your SSID and password.
3. Run:
   ```powershell
   .\setup_wifi.ps1
   ```
4. The Core will connect (blinking green), handshake with Particle Cloud (fast blinking cyan), and enter **breathing cyan** (online!).

---

## 🌡️ SZ-HS100 Analog Humidity Sensor Integration

[`smartroom_core.ino`](./smartroom_core.ino) includes native driver support for the **SZ-HS100 relative humidity analog sensor**:

### Hardware Wiring:
| Sensor Lead | Spark Core Pin | Description |
| :--- | :--- | :--- |
| **VCC (Red)** | **`3V3`** | Power supply (3.3V DC). If powering from 5V, use a 2/3 voltage divider to keep A0 $\le$ 3.3V. |
| **GND (Black)** | **`GND`** | Ground reference |
| **OUT (Green)** | **`A0`** | Analog voltage output proportional to Relative Humidity |

### Features:
- **10-Sample ADC Oversampling:** Suppresses power supply switching transients.
- **Linear Calibration:** Maps 12-bit ADC (0-4095) to 0-100% RH.
- **Dynamic Mode Switching:** Switch between DHT11 digital humidity and SZ-HS100 analog humidity on-the-fly via Particle Cloud command `cmd` (`"sz"` / `"analog"` vs `"dht"`).
- **Dual Telemetry:** Real-time variables `hum` (active RH%) and `szHum` published to ThingSpeak and Particle Cloud.

---

## 📁 Repository Contents

- [`smartroom_core.ino`](./smartroom_core.ino): Smart room native firmware with SZ-HS100 analog humidity sensor, DHT11, HC-SR04, PIR, LDR, Buzzer, RGB alerts, and ThingSpeak client.
- [`smartroom_core.bin`](./smartroom_core.bin): Pre-compiled ready-to-flash monolithic binary.
- [`tinker_core.ino`](./tinker_core.ino) & [`tinker_core.bin`](./tinker_core.bin): Factory Tinker firmware.
- [`restore_spark_core.ps1`](./restore_spark_core.ps1): Complete automated recovery script.
- [`restore_spark_core.bat`](./restore_spark_core.bat): Windows batch launcher for recovery.
- [`setup_wifi.ps1`](./setup_wifi.ps1): Serial Wi-Fi credentials dispatcher.
- [`enter_dfu.ps1`](./enter_dfu.ps1): Automatically triggers DFU mode over USB Serial.
- [`core_server_key_ip.der`](./core_server_key_ip.der): Patched server public key with direct IPv4 `52.71.103.125` and port `5683`.
- [`dfu-util/`](./dfu-util/): Portable Windows DFU flasher utility.
- [`SPARK_CORE_FIX_GUIDE.md`](./SPARK_CORE_FIX_GUIDE.md): In-depth architectural root-cause technical analysis.

---

## 🌐 Companion Web App

The accompanying web dashboard is maintained in a dedicated Vercel-ready repository:
👉 **[SmartRoom Cloud Web Dashboard](https://github.com/...)** (See `smartroom-web` repository for live telemetry, 2D pinout schematic, and calibration controls).

---

## 📄 License
MIT License. Created for the Kyambogo University Faculty of Engineering IoT Microcontroller Project.
