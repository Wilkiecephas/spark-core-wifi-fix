# Wokwi I2C Simulation Lab: STM32 & Arduino

**Department of Electrical & Electronic Engineering — Kyambogo University**  
**Course:** TETE 3102 Microcontrollers and Applications  
**Group:** 4  
**Group Members:**  
- Mukama Robert (`26/U/ETE/21285/PE`)  
- Nabudere Zerubabel (`25/U/ETE/05465/PE`)  
- Nanfuka Shakirah (`25/U/ETE/05468/PE`)  
- Kidega Wilkie Cephas (`25/U/ETE/05460/PE`)  

---

## 1. Overview of Simulation Environments

All simulations are configured with full hardware models, accurate pin connections, and declared library dependencies (`libraries.txt`).

### Microcontroller Pin Mappings

| Microcontroller | I2C SDA Pin | I2C SCL Pin | Default Logic Level | Onboard / Status LED |
| :--- | :--- | :--- | :--- | :--- |
| **STM32 Blue Pill** (`board-stm32-bluepill`) | **`B7`** | **`B6`** | 3.3V (5V tolerant) | `PC13` (active-low), `B0` |
| **Arduino Uno** (`wokwi-arduino-uno`) | **`A4`** / `SDA` | **`A5`** / `SCL` | 5.0V | `13` |

---

## 2. Directory Structure

```
simulation/
├── STM32/
│   ├── Task2_BusScanner_STM32/          <- STM32 Blue Pill I2C address scanner (0x01-0x7F)
│   ├── Task3_HelloLCD_STM32/            <- STM32 + PCF8574 LCD1602 Display ("Hello, Uganda!")
│   ├── Task4_DigitalClock_STM32/        <- STM32 + DS1307 RTC + LCD1602 Live Clock
│   ├── Task7_STM32Master_ArduinoSlave/  <- STM32 Master & Arduino Slave on unified I2C bus
│   └── Task9_MiniProject_STM32/         <- STM32 Autonomous Environmental & Time Station
│
├── Task2_BusScanner/                    <- Arduino Uno I2C address scanner
├── Task3_HelloLCD/                      <- Arduino Uno + PCF8574 LCD1602 Display
├── Task4_DigitalClock/                  <- Arduino Uno + DS1307 RTC + LCD1602 Clock
├── Task5_ObservingBus/                  <- Arduino Uno I2C bus traffic observation
├── Task7_MasterSlave/                   <- Arduino Uno Master/Slave telemetry system
└── Task9_MiniProject/                   <- Arduino Uno Environmental & Sensor Station
```

---

## 3. How to Run in Antigravity IDE

1. **Activate the Wokwi Extension:**
   - Press `Ctrl+Shift+P` (or `F1`) -> select **`Developer: Reload Window`** to make sure all newly installed commands are registered.
2. **Open the Simulation Canvas:**
   - In the Antigravity IDE file explorer, open any **`diagram.json`** (e.g. `simulation/STM32/Task3_HelloLCD_STM32/diagram.json` or `simulation/Task4_DigitalClock/diagram.json`).
   - The interactive Wokwi graphical circuit editor will open.
3. **Start Simulation:**
   - Click the green **Play (▶)** button on the top simulation toolbar.
   - Or press `Ctrl+Shift+P` -> type and run **`Wokwi: Start Simulator`**.
4. **Serial Monitor & Interactions:**
   - A Serial Monitor pane will open showing I2C scan logs, timestamps, or telemetry.
   - You can click on the potentiometers to twist them, view the LCD screen updates, and observe the LEDs.

---

## 4. Libraries Installed

Every project folder includes a `libraries.txt` file which tells the Wokwi compiler which packages to download and link:
- **`LiquidCrystal I2C`** (PCF8574 I2C Backpack driver)
- **`RTClib`** (Maxim DS1307 / DS3231 Real-Time Clock driver)
- **`Adafruit SSD1306`** (I2C OLED driver)
- **`Adafruit GFX Library`** (Core graphics primitives)
- **`Adafruit BusIO`** (Unified I2C/SPI abstraction)

---

## 5. Task 7: STM32 Master to Arduino Slave Highlights

The `Task7_STM32Master_ArduinoSlave` folder contains:
- **`diagram.json`**: Places both the **STM32 Blue Pill** and the **Arduino Uno** on the exact same breadboard, sharing:
  - `B7` (STM32 SDA) connected to `A4` (Arduino SDA) and LCD `SDA`.
  - `B6` (STM32 SCL) connected to `A5` (Arduino SCL) and LCD `SCL`.
  - Common ground (`GND`).
- **`sketch.ino`**: Firmware for the STM32 acting as Master, querying Arduino for analog readings and commanding the slave LED.
- **`arduino_slave.ino`**: Firmware for Arduino Uno responding to address `0x08`.
