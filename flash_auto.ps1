# ==============================================================================
# Auto-Flasher for Spark Core Smart Room Firmware with TI-89 Support
# ==============================================================================
$WorkspaceDir = $PSScriptRoot
$BinFile = Join-Path $WorkspaceDir "smartroom_core.bin"
$DeviceId = "54ff74066678574924331067"

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host " Spark Core Auto Flasher (TI-89 Titanium Support) " -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "Monitoring Core ($DeviceId) status..." -ForegroundColor Yellow

$maxAttempts = 30
$attempt = 0

while ($attempt -lt $maxAttempts) {
    $attempt++
    Write-Host "[Attempt $attempt/$maxAttempts] Checking device status..." -NoNewline
    $list = particle list $DeviceId 2>&1 | Out-String
    
    if ($list -match "online" -and $list -notmatch "offline") {
        Write-Host " ONLINE!" -ForegroundColor Green
        Write-Host "Initiating Over-The-Air Flash..." -ForegroundColor Yellow
        $flashResult = particle flash $DeviceId $BinFile 2>&1 | Out-String
        Write-Host $flashResult
        
        if ($flashResult -match "Flash device OK" -or $flashResult -match "successfully") {
            Write-Host "Flash Succeeded! Spark Core is restarting with TI-89 driver enabled." -ForegroundColor Green
            Write-Host "`nListening to live telemetry stream..." -ForegroundColor Cyan
            particle subscribe smartroom --device $DeviceId
            exit 0
        }
    } else {
        Write-Host " offline (waiting 3s...)" -ForegroundColor DarkGray
    }
    Start-Sleep -Seconds 3
}

Write-Host "`nTimeout: Core did not report online in cloud." -ForegroundColor Yellow
Write-Host "If the Core is plugged in via USB, you can hold the MODE button until yellow to flash via DFU."
