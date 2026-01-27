# Copyright (C) Microsoft Corporation. All rights reserved.
# Builds the VLD installer with version information from git

param(
    [string]$Configuration = "RelWithDebInfo",
    [switch]$SkipBuild,
    [switch]$Force
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "VLD Installer Build Script" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan

# Check for uncommitted changes (unless -Force)
if (-not $Force) {
    Push-Location $repoRoot
    $status = git status --porcelain 2>$null
    if ($status) {
        Write-Host "ERROR: Working tree has uncommitted changes." -ForegroundColor Red
        Write-Host "Commit your changes or use -Force to build anyway." -ForegroundColor Yellow
        Write-Host $status
        Pop-Location
        exit 1
    }
    Pop-Location
}

# Get git describe
Push-Location $repoRoot
$gitDescribe = git describe --tags --always 2>$null
if (-not $gitDescribe) {
    $gitDescribe = git rev-parse --short HEAD 2>$null
}
$gitSha = git rev-parse HEAD 2>$null
Pop-Location

Write-Host "Git describe: $gitDescribe" -ForegroundColor Green
Write-Host "Git SHA: $gitSha" -ForegroundColor Green

# Generate version files
Write-Host "`nGenerating version files..." -ForegroundColor Cyan
& "$repoRoot\version\gen_version_files.ps1" -GitDescribe $gitDescribe

# Read version for installer filename
$versionContent = (Get-Content "$repoRoot\version\version.txt" -First 1).Trim()
$versionParts = $versionContent.Split(".")
$versionShort = "{0}.{1}.{2}" -f $versionParts[0], $versionParts[1], $versionParts[2]

# Build VLD if not skipping
if (-not $SkipBuild) {
    Write-Host "`nBuilding VLD for all architectures..." -ForegroundColor Cyan
    
    $archs = @(
        @{ Name = "x64"; Generator = "Visual Studio 17 2022"; Arch = "x64" },
        @{ Name = "x86"; Generator = "Visual Studio 17 2022"; Arch = "Win32" },
        @{ Name = "ARM64"; Generator = "Visual Studio 17 2022"; Arch = "ARM64" }
    )
    
    foreach ($arch in $archs) {
        $buildDir = Join-Path $repoRoot "build_$($arch.Name.ToLower())"
        Write-Host "`n--- Building $($arch.Name) ---" -ForegroundColor Yellow
        
        if (-not (Test-Path $buildDir)) {
            New-Item -ItemType Directory -Path $buildDir -Force | Out-Null
        }
        
        Push-Location $buildDir
        cmake -G $arch.Generator -A $arch.Arch ..
        cmake --build . --config $Configuration
        Pop-Location
    }
}

# Find Inno Setup
$isccPath = "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe"
if (-not (Test-Path $isccPath)) {
    $isccPath = "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe"
}
if (-not (Test-Path $isccPath)) {
    Write-Host "ERROR: Inno Setup compiler not found." -ForegroundColor Red
    Write-Host "Install from: https://jrsoftware.org/isdl.php" -ForegroundColor Yellow
    exit 1
}

# Compile installer
Write-Host "`nCompiling installer..." -ForegroundColor Cyan
$setupScript = Join-Path $repoRoot "setup\vld-setup.iss"

& $isccPath $setupScript

if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: Installer compilation failed." -ForegroundColor Red
    exit 1
}

$outputExe = Join-Path $repoRoot "setup\Output\vld-$versionShort-setup.exe"
Write-Host "`n========================================" -ForegroundColor Green
Write-Host "SUCCESS!" -ForegroundColor Green
Write-Host "Installer: $outputExe" -ForegroundColor Green
Write-Host "Version: $versionContent" -ForegroundColor Green
Write-Host "Git: $gitDescribe" -ForegroundColor Green
Write-Host "SHA: $gitSha" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Green

Write-Host "`nTo reproduce this build:" -ForegroundColor Cyan
Write-Host "  git checkout $gitSha" -ForegroundColor White
Write-Host "  .\setup\build-installer.ps1" -ForegroundColor White
