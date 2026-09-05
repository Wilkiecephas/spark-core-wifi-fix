# Ensure Git is in Path for current session
$env:Path = [Environment]::GetEnvironmentVariable("Path","Machine") + ";" + [Environment]::GetEnvironmentVariable("Path","User") + ";C:\Program Files\Git\cmd;C:\Program Files\Git\bin"

$gitCmd = if (Test-Path "C:\Program Files\Git\cmd\git.exe") { "C:\Program Files\Git\cmd\git.exe" } else { "git" }

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "  SMARTROOM & SPARK CORE REPOSITORY PUSHER" -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan

# 1. Push stm32 (Spark Core Wi-Fi Suite)
Write-Host "`n[1/2] Pushing Spark Core Wi-Fi Suite (stm32)..." -ForegroundColor Yellow
Write-Host "Remote: https://github.com/Wilkiecephas/spark-core-wifi-fix.git" -ForegroundColor DarkGray
& $gitCmd -C "c:\Users\wilk\Documents\stm32" push -u origin main

if ($LASTEXITCODE -eq 0) {
    Write-Host "SUCCESS: stm32 pushed to spark-core-wifi-fix!" -ForegroundColor Green
} else {
    Write-Host "Pushing stm32 encountered an issue. Check credentials." -ForegroundColor Red
}

# 2. Push smartroom-web (Vercel Web App)
Write-Host "`n[2/2] Pushing SmartRoom Web Dashboard (smartroom-web)..." -ForegroundColor Yellow
Write-Host "Remote: https://github.com/Wilkiecephas/smartroom-web.git" -ForegroundColor DarkGray
& $gitCmd -C "c:\Users\wilk\Documents\stm32\smartroom-web" push -u origin main

if ($LASTEXITCODE -eq 0) {
    Write-Host "SUCCESS: smartroom-web pushed to smartroom-web!" -ForegroundColor Green
} else {
    Write-Host "Pushing smartroom-web encountered an issue. Check credentials." -ForegroundColor Red
}

Write-Host "`nAll operations completed!" -ForegroundColor Cyan
