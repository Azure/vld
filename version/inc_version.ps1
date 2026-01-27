# Copyright (C) Microsoft Corporation. All rights reserved.
# Increments the version patch number and optionally sets build ID

param(
    [int]$BuildNo = 0
)

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path

# Read the version.txt content
$versionTxtPath = Join-Path $scriptDir "version.txt"
$versionTxtContent = (Get-Content $versionTxtPath -First 1).Trim()

# Split the 4 numbers that make up the version
$fileVersion = $versionTxtContent.Split(".")
$major = [int]$fileVersion[0]
$minor = [int]$fileVersion[1]
$patch = [int]$fileVersion[2]

# Increment patch and set build number
$newVersion = "{0}.{1}.{2}.{3}" -f $major, $minor, ($patch + 1), $BuildNo

# Output the new version
$newVersion | Set-Content $versionTxtPath -NoNewline

Write-Host "Old version: $versionTxtContent"
Write-Host "New version: $newVersion"
