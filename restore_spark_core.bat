@echo off
title Spark Core Recovery Tool
cd /d "%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0restore_spark_core.ps1"
pause
