@echo off
setlocal
echo ==========================================================
echo  TI-89 Titanium Dashboard Builder (TIGCC)
echo ==========================================================
set PATH=%~dp0tigcc_toolchain;%~dp0tigcc_toolchain\Bin;%PATH%

echo Building smartmon.89z from smartmon.c...
"%~dp0tigcc_toolchain\tprbuilder.exe" "%~dp0smartmon.tpr"

if exist "%~dp0smartmon.89z" (
    echo.
    echo ==========================================================
    echo  SUCCESS: smartmon.89z is ready!
    echo ==========================================================
    dir "%~dp0smartmon.89z" | findstr smartmon.89z
    echo.
    echo Launching Auto-Flasher to upload to TI-89 Titanium...
    powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0flash_calc.ps1"
) else (
    echo.
    echo ERROR: Build failed.
)
pause

