import serial
import time
import sys

PORT = "COM18"
BAUD = 115200

print(f"Connecting to ESP32-CAM on {PORT} at {BAUD} baud...")
try:
    ser = serial.Serial(PORT, BAUD, timeout=1)
    print("Connected! Listening for ESP32 boot & runtime logs (Ctrl+C to stop)...")
    print("-" * 60)
    while True:
        try:
            line = ser.readline().decode('utf-8', errors='ignore').strip()
            if line:
                print(line)
        except Exception as e:
            time.sleep(0.01)
except KeyboardInterrupt:
    print("\nExiting monitor.")
except Exception as e:
    print(f"Serial error: {e}")
