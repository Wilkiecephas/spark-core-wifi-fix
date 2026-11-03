$portName = "COM6"
$baudRate = 14400

Write-Host "Opening $portName at $baudRate baud to trigger DFU mode..." -ForegroundColor Yellow
try {
    $port = New-Object System.IO.Ports.SerialPort $portName, $baudRate, "None", 8, "One"
    $port.DtrEnable = $true
    $port.RtsEnable = $true
    $port.Open()
    Start-Sleep -Milliseconds 300
    $port.Close()
    Write-Host "Trigger sent successfully!" -ForegroundColor Green
} catch {
    Write-Host "Failed: $($_.Exception.Message)" -ForegroundColor Red
}
