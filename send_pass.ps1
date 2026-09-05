$port = New-Object System.IO.Ports.SerialPort "COM6", 9600
$port.ReadTimeout = 4000
$port.WriteTimeout = 2000
$port.DtrEnable = $true
$port.RtsEnable = $true
$port.Open()

Write-Host "Sending password..."
$port.Write("crispass`r`n")
Start-Sleep -Seconds 3
$res = $port.ReadExisting()
Write-Host "Response: $res"

$port.Close()
Write-Host "Password sent!"
