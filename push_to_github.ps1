param(
    [string]$WebRepoUrl,
    [string]$CoreFixRepoUrl
)

$gitPath = "C:\Program Files\Git\cmd\git.exe"
if (-not (Test-Path $gitPath)) {
    $gitCmd = Get-Command git -ErrorAction SilentlyContinue
    if ($gitCmd) { $gitPath = $gitCmd.Source }
    else {
        Write-Error "Git was not found on your system. Please install Git."
        exit 1
    }
}

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "  Push SmartRoom Repositories to GitHub" -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan

# 1. Spark Core Wi-Fi Fix Repository
$coreDir = "c:\Users\wilk\Documents\stm32"
Write-Host "`n[1/2] Spark Core Wi-Fi Fix & Firmware Repository:" -ForegroundColor Yellow
if (-not $CoreFixRepoUrl) {
    $CoreFixRepoUrl = Read-Host "Enter GitHub Remote URL for Spark Core Fix repo (or press Enter to skip)"
}

if ($CoreFixRepoUrl) {
    Push-Location $coreDir
    & $gitPath remote remove origin 2>$null
    & $gitPath remote add origin $CoreFixRepoUrl
    Write-Host "Pushing main to $CoreFixRepoUrl..." -ForegroundColor Green
    & $gitPath push -u origin main
    Pop-Location
} else {
    Write-Host "Skipped pushing Spark Core Fix repo." -ForegroundColor Gray
}

# 2. Vercel Web App Repository
$webDir = "c:\Users\wilk\Documents\stm32\smartroom-web"
Write-Host "`n[2/2] SmartRoom Vercel Web Dashboard Repository:" -ForegroundColor Yellow
if (-not $WebRepoUrl) {
    $WebRepoUrl = Read-Host "Enter GitHub Remote URL for Vercel Web App repo (or press Enter to skip)"
}

if ($WebRepoUrl) {
    Push-Location $webDir
    & $gitPath remote remove origin 2>$null
    & $gitPath remote add origin $WebRepoUrl
    Write-Host "Pushing main to $WebRepoUrl..." -ForegroundColor Green
    & $gitPath push -u origin main
    Pop-Location
} else {
    Write-Host "Skipped pushing Vercel Web App repo." -ForegroundColor Gray
}

Write-Host "`nFinished! Both repositories are prepared and synchronized." -ForegroundColor Green
