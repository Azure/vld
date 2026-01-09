# Build VLD installer for all platforms
# Usage: .\build-installer.ps1

param(
    [switch]$SkipBuild = $false,
    [switch]$ARM64 = $true
)

$ErrorActionPreference = "Stop"

Write-Host "============================================" -ForegroundColor Cyan
Write-Host "  VLD 2.5.11 Installer Build Script" -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan
Write-Host ""

# Check for Inno Setup
$isccPath = $null
$isccLocations = @(
    "C:\Program Files (x86)\Inno Setup 6\ISCC.exe",
    "C:\Program Files\Inno Setup 6\ISCC.exe",
    "C:\Program Files (x86)\Inno Setup 5\ISCC.exe",
    "C:\Program Files\Inno Setup 5\ISCC.exe"
)

foreach ($loc in $isccLocations) {
    if (Test-Path $loc) {
        $isccPath = $loc
        break
    }
}

if (-not $isccPath) {
    Write-Host "ERROR: Inno Setup not found!" -ForegroundColor Red
    Write-Host "Please download and install Inno Setup from:" -ForegroundColor Yellow
    Write-Host "https://jrsoftware.org/isdl.php" -ForegroundColor Yellow
    exit 1
}

Write-Host "Found Inno Setup: $isccPath" -ForegroundColor Green
Write-Host ""

if (-not $SkipBuild) {
    # Build x86
    Write-Host "Building x86 (Win32)..." -ForegroundColor Cyan
    cmake -B build-x86 -S . -G "Visual Studio 17 2022" -A Win32
    cmake --build build-x86 --config Release
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    Write-Host ""

    # Build x64
    Write-Host "Building x64..." -ForegroundColor Cyan
    cmake -B build-x64 -S . -G "Visual Studio 17 2022" -A x64
    cmake --build build-x64 --config Release
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    Write-Host ""

    # Build ARM64 (optional)
    if ($ARM64) {
        Write-Host "Building ARM64..." -ForegroundColor Cyan
        cmake -B build-arm64 -S . -G "Visual Studio 17 2022" -A ARM64
        cmake --build build-arm64 --config Release
        if ($LASTEXITCODE -ne 0) { 
            Write-Host "WARNING: ARM64 build failed, continuing without ARM64 support" -ForegroundColor Yellow
        }
        Write-Host ""
    }
}

# Verify required files exist
Write-Host "Verifying build outputs..." -ForegroundColor Cyan
$requiredFiles = @(
    "build-x86\bin\Release\vld_x86.dll",
    "build-x64\bin\Release\vld_x64.dll"
)

$missingFiles = @()
foreach ($file in $requiredFiles) {
    if (-not (Test-Path $file)) {
        $missingFiles += $file
    }
}

if ($missingFiles.Count -gt 0) {
    Write-Host "ERROR: Required build outputs missing:" -ForegroundColor Red
    $missingFiles | ForEach-Object { Write-Host "  - $_" -ForegroundColor Red }
    exit 1
}

Write-Host "All required files found" -ForegroundColor Green
Write-Host ""

# Build installer
Write-Host "Building installer with Inno Setup..." -ForegroundColor Cyan
& $isccPath "setup\vld-setup.iss"
if ($LASTEXITCODE -ne 0) { 
    Write-Host "ERROR: Installer build failed!" -ForegroundColor Red
    exit $LASTEXITCODE 
}

Write-Host ""
Write-Host "============================================" -ForegroundColor Green
Write-Host "  Installer built successfully!" -ForegroundColor Green
Write-Host "============================================" -ForegroundColor Green
Write-Host ""

$installerPath = "setup\Output\vld-2.5.11-setup.exe"
if (Test-Path $installerPath) {
    $installerSize = (Get-Item $installerPath).Length / 1MB
    Write-Host "Installer location: $installerPath" -ForegroundColor Cyan
    Write-Host "Installer size: $($installerSize.ToString('F2')) MB" -ForegroundColor Cyan
}
