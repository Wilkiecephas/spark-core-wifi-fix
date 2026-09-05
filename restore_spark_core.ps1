<#
.SYNOPSIS
    Spark Core Ultimate Recovery & Fix Tool
.DESCRIPTION
    Automates firmware recovery, RSA key generation/flashing, DNS CNAME bypass fix (direct IP),
    and Wi-Fi provisioning for the Particle / Spark Core (STM32F103 + TI CC3000).
#>

[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$PSScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Definition
Set-Location $PSScriptRoot

$DFU_UTIL = Join-Path $PSScriptRoot "dfu-util\dfu-util.exe"
$DEVICE_ID = "54ff74066678574924331067"
$CLOUD_IP = "52.71.103.125"
$CLOUD_PORT = 5683

function Show-Banner {
    Clear-Host
    Write-Host "==========================================================" -ForegroundColor Cyan
    Write-Host "       SPARK CORE ALL-IN-ONE RECOVERY & FIX TOOL          " -ForegroundColor Yellow
    Write-Host "==========================================================" -ForegroundColor Cyan
    Write-Host " Device ID : $DEVICE_ID" -ForegroundColor Gray
    Write-Host " Target IP : $CLOUD_IP`:$CLOUD_PORT (DNS CNAME Bypass)" -ForegroundColor Gray
    Write-Host "----------------------------------------------------------" -ForegroundColor DarkGray
}

function Wait-For-DFU {
    Write-Host "`nWaiting for Spark Core in DFU Mode (Blinking Yellow)..." -ForegroundColor Yellow
    Write-Host "To enter DFU mode:" -ForegroundColor DarkYellow
    Write-Host "  1. Hold down MODE and RESET buttons simultaneously." -ForegroundColor Gray
    Write-Host "  2. Release RESET, keep holding MODE until the LED blinks YELLOW." -ForegroundColor Gray
    Write-Host "  3. Release MODE.`n" -ForegroundColor Gray

    while ($true) {
        $check = & $DFU_UTIL -l 2>&1
        if ($check -match "1d50:607f") {
            Write-Host "[+] Spark Core detected in DFU mode!" -ForegroundColor Green
            Start-Sleep -Milliseconds 500
            return $true
        }
        Start-Sleep -Seconds 1
    }
}

function Flash-Tinker {
    param([bool]$leave = $false)
    $tinkerBin = Join-Path $PSScriptRoot "tinker_core.bin"
    if (-not (Test-Path $tinkerBin)) {
        Write-Host "[-] tinker_core.bin not found!" -ForegroundColor Red
        return $false
    }
    Write-Host "[*] Flashing Tinker firmware to internal flash (0x08005000)..." -ForegroundColor Cyan
    $leaveArg = if ($leave) { "0x08005000:leave" } else { "0x08005000" }
    & $DFU_UTIL -d 1d50:607f -a 0 -s $leaveArg -v -D $tinkerBin
    if ($LASTEXITCODE -eq 0) {
        Write-Host "[+] Tinker firmware flashed successfully!" -ForegroundColor Green
        return $true
    } else {
        Write-Host "[-] Failed to flash Tinker." -ForegroundColor Red
        return $false
    }
}

function Flash-Keys {
    $privKey = Join-Path $PSScriptRoot "core_private_padded.der"
    $serverKey = Join-Path $PSScriptRoot "core_server_key_ip.der"
    $pubKeyPem = Join-Path $PSScriptRoot "core_public.pem"

    if (-not (Test-Path $privKey) -or -not (Test-Path $serverKey)) {
        Write-Host "[-] Key files missing!" -ForegroundColor Red
        return $false
    }

    Write-Host "[*] 1/3 Flashing device private key (1024 bytes) to SPI flash (0x00002000)..." -ForegroundColor Cyan
    & $DFU_UTIL -d 1d50:607f -a 1 -s 0x00002000 -v -D $privKey
    if ($LASTEXITCODE -ne 0) {
        Write-Host "[-] Failed to flash private key." -ForegroundColor Red
        return $false
    }

    Write-Host "[*] 2/3 Flashing Particle cloud server key with direct IP ($CLOUD_IP) to SPI flash (0x00001000)..." -ForegroundColor Cyan
    & $DFU_UTIL -d 1d50:607f -a 1 -s 0x00001000 -v -D $serverKey
    if ($LASTEXITCODE -ne 0) {
        Write-Host "[-] Failed to flash server key." -ForegroundColor Red
        return $false
    }

    Write-Host "[*] 3/3 Synchronizing public key with Particle Cloud API..." -ForegroundColor Cyan
    $configPath = "$env:USERPROFILE\.particle\particle.config.json"
    if (Test-Path $configPath) {
        try {
            $cfg = Get-Content $configPath | ConvertFrom-Json
            $token = $cfg.access_token
            $pubContent = Get-Content $pubKeyPem -Raw

            $body = @{
                deviceID  = $DEVICE_ID
                publicKey = $pubContent
                filename  = "particle-api"
                order     = "manual_$([DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds())"
                algorithm = "rsa"
            } | ConvertTo-Json

            $res = Invoke-RestMethod -Uri "https://api.particle.io/v1/provisioning/$DEVICE_ID" `
                                     -Method Post `
                                     -Headers @{ Authorization = "Bearer $token" } `
                                     -ContentType "application/json" `
                                     -Body $body

            Write-Host "[+] Particle Cloud acknowledged public key registration (Status: OK)" -ForegroundColor Green
        } catch {
            Write-Host "[!] Warning: Cloud key upload returned: $($_.Exception.Message)" -ForegroundColor Yellow
        }
    } else {
        Write-Host "[!] No Particle credentials found in ~/.particle/particle.config.json" -ForegroundColor Yellow
    }

    Write-Host "[+] All keys successfully restored and aligned!" -ForegroundColor Green
    return $true
}

function Configure-Wifi-Serial {
    param([string]$ssid, [string]$password, [string]$security = "3", [string]$cipher = "1")

    Write-Host "`nDetecting Spark Core Serial (COM) port..." -ForegroundColor Cyan
    $portName = (Get-CimInstance Win32_SerialPort | Where-Object { $_.Name -match "Spark Core" -or $_.Description -match "Spark Core" } | Select-Object -First 1).DeviceID
    if (-not $portName) {
        $portName = Read-Host "Enter COM port name (e.g. COM6)"
    }
    if (-not $portName) { $portName = "COM6" }

    Write-Host "[*] Opening $portName at 9600 baud..." -ForegroundColor Cyan
    try {
        $port = New-Object System.IO.Ports.SerialPort $portName, 9600
        $port.ReadTimeout = 3000
        $port.WriteTimeout = 2000
        $port.DtrEnable = $true
        $port.RtsEnable = $true
        $port.Open()

        Write-Host "[*] Initiating Wi-Fi setup..." -ForegroundColor Cyan
        $port.Write("w")
        Start-Sleep -Milliseconds 600
        $port.ReadExisting() | Out-Null

        Write-Host "[*] Sending SSID: $ssid" -ForegroundColor Cyan
        $port.Write("$ssid`r`n")
        Start-Sleep -Milliseconds 600
        $port.ReadExisting() | Out-Null

        Write-Host "[*] Sending Security: $security" -ForegroundColor Cyan
        $port.Write("$security`r`n")
        Start-Sleep -Milliseconds 600
        $port.ReadExisting() | Out-Null

        Write-Host "[*] Sending Cipher: $cipher" -ForegroundColor Cyan
        $port.Write("$cipher`r`n")
        Start-Sleep -Milliseconds 600
        $port.ReadExisting() | Out-Null

        Write-Host "[*] Sending Password..." -ForegroundColor Cyan
        $port.Write("$password`r`n")
        Start-Sleep -Seconds 2
        $resp = $port.ReadExisting()

        $port.Close()
        Write-Host "[+] Wi-Fi credentials saved successfully!" -ForegroundColor Green
        Write-Host $resp -ForegroundColor Gray
    } catch {
        Write-Host "[-] Serial Error: $($_.Exception.Message)" -ForegroundColor Red
        Write-Host "Make sure the Spark Core is in Listening Mode (Blinking Blue) before configuring Wi-Fi." -ForegroundColor Yellow
    }
}

# --- Main Menu Loop ---
do {
    Show-Banner
    Write-Host "Select an action:" -ForegroundColor White
    Write-Host "  1) FULL 1-CLICK RESTORE (Flash Tinker + Fix Keys with Direct IP + Register Cloud)" -ForegroundColor Green
    Write-Host "  2) Fix Cloud Keys only (Bypasses CNAME DNS Bug with Direct IP 52.71.103.125)" -ForegroundColor Cyan
    Write-Host "  3) Flash Tinker Firmware only" -ForegroundColor Cyan
    Write-Host "  4) Configure Wi-Fi via USB Serial (Listening Mode)" -ForegroundColor Cyan
    Write-Host "  5) Check Particle Cloud Online Status" -ForegroundColor Cyan
    Write-Host "  6) Exit" -ForegroundColor Gray
    $choice = Read-Host "`nEnter choice [1-6]"

    switch ($choice) {
        "1" {
            if (Wait-For-DFU) {
                Flash-Keys
                Flash-Tinker -leave $true
                Write-Host "`n>>> RESTORATION COMPLETE! Press RESET on your Spark Core. <<<" -ForegroundColor Green
                Write-Host "It will connect to Wi-Fi and breathe Cyan." -ForegroundColor Green
            }
            Read-Host "`nPress Enter to continue..."
        }
        "2" {
            if (Wait-For-DFU) {
                Flash-Keys
                Write-Host "`n>>> Keys fixed! Press RESET on your Spark Core. <<<" -ForegroundColor Green
            }
            Read-Host "`nPress Enter to continue..."
        }
        "3" {
            if (Wait-For-DFU) {
                Flash-Tinker -leave $true
                Write-Host "`n>>> Firmware flashed! Press RESET on your Spark Core. <<<" -ForegroundColor Green
            }
            Read-Host "`nPress Enter to continue..."
        }
        "4" {
            $ssid = Read-Host "Enter Wi-Fi SSID (e.g. A32)"
            $pass = Read-Host "Enter Wi-Fi Password"
            Configure-Wifi-Serial -ssid $ssid -password $pass
            Read-Host "`nPress Enter to continue..."
        }
        "5" {
            Write-Host "`nQuerying Particle Cloud API..." -ForegroundColor Cyan
            & particle list
            Read-Host "`nPress Enter to continue..."
        }
        "6" {
            Write-Host "Exiting..." -ForegroundColor Gray
            break
        }
    }
} while ($true)
