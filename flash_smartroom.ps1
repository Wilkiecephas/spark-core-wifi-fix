# ==============================================================================
# Flash Smart Room Monitoring System Firmware to Spark Core
# SYSTEM_MODE(MANUAL) — Direct WiFi → ThingSpeak (no Particle Cloud)
# ==============================================================================
param(
    [ValidateSet("ota", "dfu")]
    [string]$Method = "dfu"   # Default: DFU (OTA requires cloud which we bypass)
)

$ErrorActionPreference = "Stop"
$WorkspaceDir = $PSScriptRoot
$InoFile = Join-Path $WorkspaceDir "smartroom_core.ino"
$BinFile = Join-Path $WorkspaceDir "smartroom_core.bin"
$DeviceId = "54ff74066678574924331067"

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host " Smart Room — Cloud-Free Firmware (ThingSpeak Direct WiFi)" -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "Target Device: $DeviceId"
Write-Host "Firmware:      $InoFile"
Write-Host "Flash Method:  $Method"
Write-Host "Mode:          SYSTEM_MODE(MANUAL) — no Particle Cloud" -ForegroundColor Yellow

Write-Host "`n[1/2] Compiling firmware for Spark Core..." -ForegroundColor Yellow
particle compile core $InoFile --saveTo $BinFile

if (-not (Test-Path $BinFile)) {
    Write-Error "Compilation failed - $BinFile was not produced."
    exit 1
}

Write-Host "Compile succeeded! Binary size: $((Get-Item $BinFile).Length) bytes" -ForegroundColor Green

if ($Method -eq "ota") {
    Write-Host "`n[2/2] Flashing Over-The-Air via Particle Cloud..." -ForegroundColor Yellow
    Write-Host "NOTE: OTA requires cloud access. If quota is exhausted use -Method dfu instead." -ForegroundColor Yellow
    particle flash $DeviceId $BinFile
    Write-Host "`nOTA Flash Complete!" -ForegroundColor Green
} else {
    Write-Host "`n[2/2] Flashing via USB DFU Mode..." -ForegroundColor Yellow
    Write-Host "Make sure Spark Core is in DFU mode (yellow flashing LED)." -ForegroundColor Yellow
    Write-Host "Hold MODE button + tap RESET, release RESET, keep holding MODE until LED flashes yellow." -ForegroundColor DarkYellow
    $DfuTool = Join-Path $WorkspaceDir "dfu-util\dfu-util.exe"
    if (-not (Test-Path $DfuTool)) {
        Write-Error "dfu-util.exe not found at $DfuTool"
        exit 1
    }
    & $DfuTool -d 1d50:607f -a 0 -s 0x08005000:leave -D $BinFile
    Write-Host "`nDFU Flash Complete!" -ForegroundColor Green
}

Write-Host "`n==========================================================" -ForegroundColor Cyan
Write-Host " Done! Core will connect to WiFi and POST to ThingSpeak." -ForegroundColor Cyan
Write-Host " RGB LED: Orange=connecting, Green=WiFi ready & live" -ForegroundColor Cyan
Write-Host " Monitor: https://thingspeak.com/channels/3475948" -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan
