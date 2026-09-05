# ==============================================================================
# Flash Smart Room Monitoring System Firmware to Spark Core
# ==============================================================================
param(
    [ValidateSet("ota", "dfu")]
    [string]$Method = "ota"
)

$ErrorActionPreference = "Stop"
$WorkspaceDir = $PSScriptRoot
$InoFile = Join-Path $WorkspaceDir "smartroom_core.ino"
$BinFile = Join-Path $WorkspaceDir "smartroom_core.bin"
$DeviceId = "54ff74066678574924331067"

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host " Smart Room Monitoring System - Firmware Builder & Flasher" -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "Target Device: $DeviceId"
Write-Host "Firmware:      $InoFile"
Write-Host "Flash Method:  $Method"

Write-Host "`n[1/2] Compiling firmware for Spark Core in Particle Cloud..." -ForegroundColor Yellow
particle compile core $InoFile --saveTo $BinFile

if (-not (Test-Path $BinFile)) {
    Write-Error "Compilation failed - $BinFile was not produced."
    exit 1
}

Write-Host "Compile succeeded! Binary size: $((Get-Item $BinFile).Length) bytes" -ForegroundColor Green

if ($Method -eq "ota") {
    Write-Host "`n[2/2] Flashing Over-The-Air (OTA) via Particle Cloud..." -ForegroundColor Yellow
    particle flash $DeviceId $BinFile
    Write-Host "`nOTA Flash Complete! Spark Core will reboot and breathe cyan." -ForegroundColor Green
} else {
    Write-Host "`n[2/2] Flashing via USB DFU Mode..." -ForegroundColor Yellow
    $DfuTool = Join-Path $WorkspaceDir "dfu-util\dfu-util.exe"
    if (-not (Test-Path $DfuTool)) {
        Write-Error "dfu-util.exe not found at $DfuTool"
        exit 1
    }
    & $DfuTool -d 1d50:607f -a 0 -s 0x08005000:leave -D $BinFile
    Write-Host "`nDFU Flash Complete!" -ForegroundColor Green
}

Write-Host "`n==========================================================" -ForegroundColor Cyan
Write-Host " Done! Listening to live 'smartroom' cloud stream..." -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan
particle subscribe smartroom --device $DeviceId
