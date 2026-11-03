@echo off
setlocal enabledelayedexpansion

echo ========================================================
echo   ESP32-CAM 1-CLICK FIRMWARE FLASHER (Merged Binary)
echo   Built for Smart IoT Hub & Telegram Bot Live Demo
echo ========================================================

set PYTHON_CMD=C:\Python314\python.exe
set BIN_FILE=%~dp0esp32_cam_firmware\build\esp32_cam_firmware.ino.merged.bin

if not exist "%BIN_FILE%" (
    echo [ERROR] Firmware binary not found at %BIN_FILE%
    pause
    exit /b 1
)

echo.
echo Searching for connected ESP32-CAM COM port...

for /f "tokens=*" %%p in ('powershell -NoProfile -Command "[System.IO.Ports.SerialPort]::GetPortNames() | Where-Object { $_ -ne 'COM3' }"') do (
    set DETECTED_PORT=%%p
)

if "%DETECTED_PORT%"=="" (
    echo [WARNING] No USB serial COM port detected!
    echo Please plug the ESP32-CAM FTDI adapter into your USB port.
    echo (Make sure GPIO 0 is grounded to GND to enter flash mode).
    echo.
    set /p DETECTED_PORT="Enter COM port manually (e.g. COM18): "
)

if "%DETECTED_PORT%"=="" (
    echo No port specified. Exiting.
    pause
    exit /b 1
)

echo.
echo [1/2] Connecting to ESP32 on %DETECTED_PORT% at 460800 baud...
"%PYTHON_CMD%" -m esptool --chip esp32 --port %DETECTED_PORT% --baud 460800 write-flash -z 0x0 "%BIN_FILE%"

if %ERRORLEVEL% equ 0 (
    echo.
    echo ========================================================
    echo   FLASH SUCCESSFUL!
    echo ========================================================
    echo 1. Remove the GPIO 0 to GND jumper wire.
    echo 2. Press the RST button on the back of the ESP32-CAM.
    echo 3. The ESP32-CAM will connect to your iPhone hotspot,
    echo    start the video stream, and link with Telegram.
    echo ========================================================
) else (
    echo.
    echo [ERROR] Flash failed!
    echo Make sure:
    echo 1. GPIO 0 is connected to GND before pressing RST.
    echo 2. No other program (serial monitor) is using %DETECTED_PORT%.
)

echo.
pause
