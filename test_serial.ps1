$port = New-Object System.IO.Ports.SerialPort "COM6", 9600
$port.ReadTimeout = 1000
$port.WriteTimeout = 1000
$port.DtrEnable = $true
$port.RtsEnable = $true
$port.Open()
$port.Write("i")
Start-Sleep -Milliseconds 500
$res = $port.ReadExisting()
$port.Close()
Write-Host "Response from Spark Core: '$res'"
