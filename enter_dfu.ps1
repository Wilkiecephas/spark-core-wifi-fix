$port = New-Object System.IO.Ports.SerialPort "COM6", 14400
$port.Open()
Start-Sleep -Milliseconds 100
$port.Close()
