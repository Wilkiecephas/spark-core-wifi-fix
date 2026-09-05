@echo off
set "PATH=%PATH%;C:\Program Files\Git\cmd;C:\Program Files\Git\bin"
echo ==========================================================
echo   SMARTROOM & SPARK CORE REPOSITORY PUSHER
echo ==========================================================

echo.
echo [1/2] Pushing Spark Core Wi-Fi Suite (stm32)...
"C:\Program Files\Git\cmd\git.exe" -C "c:\Users\wilk\Documents\stm32" push -u origin main

echo.
echo [2/2] Pushing SmartRoom Web Dashboard (smartroom-web)...
"C:\Program Files\Git\cmd\git.exe" -C "c:\Users\wilk\Documents\stm32\smartroom-web" push -u origin main

echo.
echo Completed!
pause
