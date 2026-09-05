$port = New-Object System.IO.Ports.SerialPort "COM6", 115200
$port.ReadTimeout = 500
$port.WriteTimeout = 500
$port.DtrEnable = $true
$port.RtsEnable = $true

try {
    $port.Open()
    Write-Host "Listening on COM6 at 115200 baud..." -ForegroundColor Cyan
    $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    while ($stopwatch.ElapsedMilliseconds -lt 15000) {
        try {
            $line = $port.ReadLine()
            if ($line) {
                Write-Host $line
            }
        } catch [System.TimeoutException] {
            # timeout is normal while waiting for lines
        } catch {
            Write-Host "Read error: $($_.Exception.Message)"
            break
        }
    }
    $port.Close()
} catch {
    Write-Host "Could not open port: $($_.Exception.Message)"
}
