$port = New-Object System.IO.Ports.SerialPort "COM6", 9600
try {
    $port.Open()
    Start-Sleep -Milliseconds 500
    $data = $port.ReadExisting()
    Write-Host "Serial output: '$data'"
    $port.Close()
} catch {
    Write-Host "Serial error: $($_.Exception.Message)"
}
