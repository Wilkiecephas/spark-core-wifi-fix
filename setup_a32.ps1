$port = New-Object System.IO.Ports.SerialPort "COM6", 9600
$port.ReadTimeout = 2000
$port.WriteTimeout = 1000
$port.DtrEnable = $true
$port.RtsEnable = $true
$port.Open()

Write-Host "Sending 'w' to initiate Wi-Fi setup..."
$port.Write("w")
Start-Sleep -Milliseconds 600
$p = $port.ReadExisting()
Write-Host "Prompt 1: $p"

Write-Host "Sending SSID: A32"
$port.Write("A32`r`n")
Start-Sleep -Milliseconds 600
$p = $port.ReadExisting()
Write-Host "Prompt 2: $p"

Write-Host "Sending Security: 3 (WPA2)"
$port.Write("3`r`n")
Start-Sleep -Milliseconds 600
$p = $port.ReadExisting()
Write-Host "Prompt 3: $p"

Write-Host "Sending Cipher: 1 (AES)"
$port.Write("1`r`n")
Start-Sleep -Milliseconds 600
$p = $port.ReadExisting()
Write-Host "Prompt 4: $p"

Write-Host "Sending Password: crispass"
$port.Write("crispass`r`n")
Start-Sleep -Seconds 3
$p = $port.ReadExisting()
Write-Host "Final Response: $p"

$port.Close()
Write-Host "Done!"
