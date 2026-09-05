# Spark Core (Gen 1) Recovery & Cloud Reconnection Guide

This document summarizes the root causes, architectural findings, and exact repair procedure for restoring an unbricked Spark Core (`STM32F103CB` + `TI CC3000`) and bringing it online on modern networks.

---

## 1. Root Causes Diagnosed

### A. Wrong Target Firmware (Photon vs Core)
- **Photon Architecture:** STM32F205 (Cortex-M4), modular architecture (`system-part1`, `system-part2`, `user-part`), Broadcom BCM43362 Wi-Fi over SDIO.
- **Spark Core Architecture:** STM32F103CB (Cortex-M3, 128KB Flash, 20KB RAM), monolithic firmware at `0x08005000`, TI CC3000 Wi-Fi over SPI.
- **Result:** Flashing Photon binaries to the Spark Core caused immediate HardFault crashes on boot. The CC3000 Wi-Fi module never initialized.

### B. Outdated Particle CLI Flashing Regressions
- Modern `particle-cli` (v3.50.1+) expects modular Gen 2/Gen 3 binary headers.
- Commands like `particle flash --usb tinker` throw `TypeError: Cannot read properties of undefined (reading 'find')`.
- **Fix:** Compile monolithic firmware via `particle compile core <file> --saveTo <binary.bin>` and flash directly with `dfu-util` at `0x08005000`.

### C. Server Port 65535 Mismatch
- The Particle CLI's embedded default server key asset has port offset 450 set to `0xFF 0xFF` (`65535`).
- The Spark Core C++ firmware reads this offset directly and attempts to open a TCP connection to `device.spark.io:65535` instead of port `5683`, causing an immediate connection reset (red flash).
- **Fix:** Byte offset 450 in the server key must be explicitly patched to `0x16 0x33` (`5683`).

### D. The CNAME DNS Recursion Failure (The Final Hurdle)
- When attempting to connect to `device.spark.io`, modern Particle Cloud infrastructure uses a multi-level AWS CNAME alias chain:
  `device.spark.io` $\rightarrow$ `device.tcp.particle.io` $\rightarrow$ `device-service-tcp...eks-production-gotham...` $\rightarrow$ `52.71.103.125`.
- The TI CC3000 chip's legacy 2013 DNS client cannot process recursive CNAME records. It hangs on `Resolving device.spark.io` until the WLAN watchdog (`ARM_WLAN_WD`) resets the chip.
- **Fix:** Embed the direct IPv4 address `52.71.103.125` at byte offset 384 (`0x00` type = IP, `0x04` len, `52.71.103.125`). This bypasses the CC3000 DNS client completely.

---

## 2. Memory & Sector Layout

| Memory / Interface | Address | Size | Content |
| :--- | :--- | :--- | :--- |
| **Internal Flash (alt 0)** | `0x08000000` | 20 KB | Factory Bootloader (read-only) |
| **Internal Flash (alt 0)** | `0x08005000` | 108 KB | User Application (`tinker_core.bin`) |
| **SPI Flash SST25x (alt 1)** | `0x00001000` | 2048 B | Server Public Key (`core_server_key_ip.der`) |
| **SPI Flash SST25x (alt 1)** | `0x00002000` | 1024 B | Device Private Key (`core_private_padded.der`) |

---

## 3. Quick 1-Click Restoration

### Using the Auto-Start Script:
Double-click [`restore_spark_core.bat`](file:///c:/Users/wilk/Documents/stm32/restore_spark_core.bat) (or run [`restore_spark_core.ps1`](file:///c:/Users/wilk/Documents/stm32/restore_spark_core.ps1) in PowerShell).

### Manual Commands (via `dfu-util`):

1. **Flash Device Private Key (aligned to 1024 bytes):**
   ```powershell
   .\dfu-util\dfu-util.exe -d 1d50:607f -a 1 -s 0x00002000 -v -D core_private_padded.der
   ```
2. **Flash Direct IP Server Public Key (52.71.103.125:5683):**
   ```powershell
   .\dfu-util\dfu-util.exe -d 1d50:607f -a 1 -s 0x00001000 -v -D core_server_key_ip.der
   ```
3. **Flash Monolithic Tinker Firmware:**
   ```powershell
   .\dfu-util\dfu-util.exe -d 1d50:607f -a 0 -s 0x08005000:leave -D tinker_core.bin
   ```
4. **Register Public Key with Particle Cloud API:**
   ```powershell
   # Automated via restore_spark_core.ps1 or via POST to /v1/provisioning/:deviceId
   ```
5. **Configure 2.4 GHz Wi-Fi:**
   Hold `MODE` for 3 seconds (blinking blue), then send credentials via serial (9600 baud) or `setup_a32.ps1`.
