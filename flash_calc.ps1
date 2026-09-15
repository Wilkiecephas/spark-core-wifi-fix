# ==============================================================================
# Auto-Flasher for TI-89 Titanium Dashboard (smartmon.89z)
# ==============================================================================
$WorkspaceDir = $PSScriptRoot
$SourceFile   = Join-Path $WorkspaceDir "smartmon.c"
$TprFile      = Join-Path $WorkspaceDir "smartmon.tpr"
$BinFile      = Join-Path $WorkspaceDir "smartmon.89z"
$TigccDir     = Join-Path $WorkspaceDir "tigcc_toolchain"
$TprBuilder   = Join-Path $TigccDir "tprbuilder.exe"
$TilpExe      = "C:\Program Files (x86)\TiLP\tilp.exe"

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "  TI-89 Titanium Auto-Flasher (DirectLink / TiLP)          " -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan

# 1. Build smartmon.89z
Write-Host "[1/3] Compiling smartmon.c with TIGCC..." -ForegroundColor Yellow
$env:PATH = "$TigccDir;$TigccDir\Bin;" + $env:PATH
& $TprBuilder $TprFile | Out-Null

if (-not (Test-Path $BinFile)) {
    Write-Host "ERROR: smartmon.89z build failed!" -ForegroundColor Red
    exit 1
}

$fileInfo = Get-Item $BinFile
Write-Host "SUCCESS: smartmon.89z built ($($fileInfo.Length) bytes)." -ForegroundColor Green

# 2. Verify TiLP Installation
if (-not (Test-Path $TilpExe)) {
    Write-Host "ERROR: TiLP not found at $TilpExe!" -ForegroundColor Red
    exit 1
}

# 3. Wait for Calculator Connection & Flash
Write-Host "`n[2/3] Waiting for TI-89 Titanium connection..." -ForegroundColor Yellow
Write-Host " -> Make sure your TI-89 Titanium is turned ON (press [ON] key)" -ForegroundColor White
Write-Host " -> Connect the USB cable firmly to the PC" -ForegroundColor White
Write-Host " -> Stay on the HOME screen on the calculator`n" -ForegroundColor White

$maxAttempts = 40
$attempt = 0
$flashed = $false

while ($attempt -lt $maxAttempts) {
    $attempt++
    
    # Check if TI-89 Titanium USB is present in Windows PnP
    $tiDev = Get-PnpDevice -PresentOnly -ErrorAction SilentlyContinue | Where-Object { 
        $_.InstanceId -like "*0451*" -or $_.FriendlyName -like "*Titanium*" -or $_.FriendlyName -like "*TI-89*"
    }

    if ($tiDev) {
        Write-Host "[Attempt $attempt/$maxAttempts] TI-89 Titanium detected: $($tiDev.FriendlyName)!" -ForegroundColor Green
        Write-Host "Initiating transfer via TiLP DirectLink..." -ForegroundColor Yellow

        # Set LPG Shared and GTK2 in PATH so TiLP can load its dynamic libraries
        $env:PATH = "C:\Program Files (x86)\Common Files\LPG Shared\libs;C:\Program Files (x86)\GTK2-Runtime\bin;" + $env:PATH

        # Try DirectLink (Standard USB cable)
        $proc = Start-Process -FilePath $TilpExe -ArgumentList "--calc=ti89t", "--cable=DirectLink", "--no-gui", "--silent", "`"$BinFile`"" -Wait -PassThru -NoNewWindow
        
        $logPath = "$env:USERPROFILE\.tilp.log"
        $logContent = if (Test-Path $logPath) { Get-Content $logPath -Tail 20 | Out-String } else { "" }

        if ($proc.ExitCode -eq 0 -and -not ($logContent -match "ticables-WARNING: no devices found|failed to open the USB device")) {
            Write-Host "`n==========================================================" -ForegroundColor Green
            Write-Host " SUCCESS: smartmon.89z transferred to TI-89 Titanium!" -ForegroundColor Green
            Write-Host "==========================================================" -ForegroundColor Green
            Write-Host "Run on calculator: main\smartmon() or smartmon()" -ForegroundColor Cyan
            $flashed = $true
            break
        } else {
            # TiLP requires libusb0.sys, but modern Windows uses WinUSB.sys
            Write-Host " TiLP desktop requires libusb0 driver, but Windows is using WinUSB for the Titanium." -ForegroundColor DarkYellow
            Write-Host " Launching WebTILP (works out-of-the-box with WinUSB in Chrome/Edge)..." -ForegroundColor Cyan
            Start-Process "https://web.tilp.info"
            Start-Process "explorer.exe" "/select,`"$BinFile`""
            break
        }
    } else {
        # Check if an unknown/uninitialized USB device is present (calc turned off / sleepy)
        $failedDev = Get-PnpDevice -PresentOnly -ErrorAction SilentlyContinue | Where-Object {
            $_.FriendlyName -like "*Device Descriptor Request Failed*"
        }
        if ($failedDev) {
            Write-Host "[Attempt $attempt/$maxAttempts] Calculator detected but asleep/uninitialized. Please turn calculator ON (press [ON]) and re-plug USB..." -ForegroundColor DarkYellow
        } else {
            Write-Host "[Attempt $attempt/$maxAttempts] Waiting for calculator USB connection..." -ForegroundColor DarkGray
        }
    }

    Start-Sleep -Seconds 2
}

if (-not $flashed) {
    Write-Host "`nTiLP Auto-Transfer timed out." -ForegroundColor Yellow
    Write-Host "Launching TiLP GUI so you can click 'Send Files' manually..." -ForegroundColor Cyan
    Start-Process -FilePath $TilpExe -ArgumentList "`"$BinFile`""
}
