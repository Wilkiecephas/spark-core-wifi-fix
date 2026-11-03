import subprocess
import os
import shutil
import tempfile

cli = r"C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe"

stm32_tasks = [
    r"simulation\STM32\Task2_BusScanner_STM32",
    r"simulation\STM32\Task3_HelloLCD_STM32",
    r"simulation\STM32\Task4_DigitalClock_STM32",
    r"simulation\STM32\Task7_STM32Master_ArduinoSlave",
    r"simulation\STM32\Task9_MiniProject_STM32"
]

for t in stm32_tasks:
    src = os.path.join(t, "sketch.ino")
    out_dir = os.path.abspath(os.path.join(t, "build"))
    os.makedirs(out_dir, exist_ok=True)
    
    with tempfile.TemporaryDirectory() as td:
        sketch_dir = os.path.join(td, "sketch")
        os.makedirs(sketch_dir)
        shutil.copy(src, os.path.join(sketch_dir, "sketch.ino"))
        
        cmd = [cli, "compile", "--fqbn", "STMicroelectronics:stm32:GenF1:pnum=BLUEPILL_F103C8", sketch_dir, "--output-dir", out_dir]
        p = subprocess.run(cmd, capture_output=True, text=True)
        if p.returncode == 0:
            print(f"COMPILED STM32: {t}")
            toml_path = os.path.join(t, "wokwi.toml")
            with open(toml_path, "w", encoding="utf-8") as tf:
                tf.write("[wokwi]\nversion = 1\nfirmware = \"build/sketch.ino.bin\"\nelf = \"build/sketch.ino.elf\"\n")
        else:
            print(f"FAILED STM32: {t} -> {p.stderr}")
