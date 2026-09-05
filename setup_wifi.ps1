$port = New-Object System.IO.Ports.SerialPort "COM6", 9600
$port.ReadTimeout = 2000
$port.WriteTimeout = 1000
$port.DtrEnable = $true
$port.RtsEnable = $true
$port.Open()

# Read the pending prompt
Start-Sleep -Milliseconds 600
$p = $port.ReadExisting()
Write-Host "Prompt: '$p'"

if ($p -match "Cipher" -or $p -match "Security Cipher") {
    Write-Host "Sending cipher 1 (AES)..."
    $port.Write("1`r`n")
    Start-Sleep -Milliseconds 800
    $p = $port.ReadExisting()
    Write-Host "Prompt 2: '$p'"
}

if ($p -match "Password" -or $p -match "password") {
    Write-Host "Sending password..."
    $port.Write("New2030@duku`r`n")
    Start-Sleep -Seconds 3
    $p = $port.ReadExisting()
    Write-Host "Final response: '$p'"
}

$port.Close()
