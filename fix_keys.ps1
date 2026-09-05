Write-Host "Flashing Particle cloud server key with direct IP (52.71.103.125:5683) to SPI flash..." -ForegroundColor Cyan
.\dfu-util\dfu-util.exe -d 1d50:607f -a 1 -s 0x00001000 -v -D core_server_key_ip.der

Write-Host "`n>>> Done! Please press RESET on your Spark Core now. <<<" -ForegroundColor Green
