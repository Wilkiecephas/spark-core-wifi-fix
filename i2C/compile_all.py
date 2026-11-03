import subprocess
import os
import shutil
import tempfile

cli = r"C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe"

tasks = [
    r"simulation\Task2_BusScanner",
    r"simulation\Task3_HelloLCD",
    r"simulation\Task4_DigitalClock",
    r"simulation\Task5_ObservingBus",
    r"simulation\Task7_MasterSlave",
    r"simulation\Task9_MiniProject"
]

for t in tasks:
    src = os.path.join(t, "sketch.ino")
    out_dir = os.path.abspath(os.path.join(t, "build"))
    os.makedirs(out_dir, exist_ok=True)
    
    with tempfile.TemporaryDirectory() as td:
        sketch_dir = os.path.join(td, "sketch")
        os.makedirs(sketch_dir)
        shutil.copy(src, os.path.join(sketch_dir, "sketch.ino"))
        
        cmd = [cli, "compile", "--fqbn", "arduino:avr:uno", sketch_dir, "--output-dir", out_dir]
        p = subprocess.run(cmd, capture_output=True, text=True)
        if p.returncode == 0:
            print(f"COMPILED: {t}")
            toml_path = os.path.join(t, "wokwi.toml")
            with open(toml_path, "w", encoding="utf-8") as tf:
                tf.write("[wokwi]\nversion = 1\nfirmware = \"build/sketch.ino.hex\"\nelf = \"build/sketch.ino.elf\"\n")
        else:
            print(f"FAILED: {t} -> {p.stderr}")
