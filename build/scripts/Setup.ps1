<#
.SYNOPSIS
    Provisioning script for ARM64 VMs - installs build tools and logs agent diagnostics.
    
.DESCRIPTION
    This script runs at VM spin-up time to:
    1. Install Inno Setup 6.7.0 for all users
    2. Log diagnostic information about the Azure DevOps agent installations
    
    OBSERVABILITY:
    Logs are written to Azure Blob Storage and can be viewed at:
    https://winbuildpoolarm.blob.core.windows.net/insights-logs-provisioningscriptlogs
    
    Filter by:
    - category: "ProvisioningScriptLogs"
    - Look for script version in output (e.g., "VLD Provisioning Script v2.0.0")
    
.NOTES
    - Runs as provisioning script in 1ES image configuration
    
    SOURCE: build/scripts/Setup.ps1 in the VLD repository
#>

$ErrorActionPreference = "Stop"
$ScriptVersion = "2.0.0"
$ScriptSource = "https://github.com/Azure/vld/blob/master/build/scripts/Setup.ps1"

# Logging helper
function Write-Log {
    param([string]$Message)
    $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    Write-Host "[$timestamp] $Message"
}

Write-Log "==========================================="
Write-Log "VLD Provisioning Script v$ScriptVersion"
Write-Log "Source: $ScriptSource"
Write-Log "==========================================="

#region Inno Setup Installation
Write-Log ""
Write-Log "--- Installing Inno Setup 6.7.0 ---"

$innoSetupExe = "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe"
if (Test-Path $innoSetupExe) {
    Write-Log "Inno Setup already installed at: $innoSetupExe"
}
else {
    # Inno Setup installer should be in the same directory as this script
    $scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
    $innoInstaller = Join-Path $scriptDir "innosetup-6.7.0.exe"
    
    if (-not (Test-Path $innoInstaller)) {
        Write-Log "ERROR: Inno Setup installer not found at: $innoInstaller"
        exit 1
    }
    
    Write-Log "Installing Inno Setup from: $innoInstaller"
    
    # Silent install for all users (/ALLUSERS), no restart, default components
    $installArgs = "/VERYSILENT /SUPPRESSMSGBOXES /NORESTART /ALLUSERS /DIR=`"${env:ProgramFiles(x86)}\Inno Setup 6`""
    
    $process = Start-Process -FilePath $innoInstaller -ArgumentList $installArgs -Wait -PassThru
    
    if ($process.ExitCode -eq 0) {
        Write-Log "Inno Setup installed successfully."
    }
    else {
        Write-Log "ERROR: Inno Setup installation failed with exit code: $($process.ExitCode)"
        exit 1
    }
    
    # Verify installation
    if (Test-Path $innoSetupExe) {
        Write-Log "Verified: ISCC.exe exists at $innoSetupExe"
    }
    else {
        Write-Log "ERROR: ISCC.exe not found after installation"
        exit 1
    }
}
#endregion

#region Agent Diagnostics
Write-Log ""
Write-Log "--- Agent Diagnostics ---"

# Check OS architecture
$osArch = (Get-CimInstance Win32_OperatingSystem).OSArchitecture
Write-Log "OS Architecture: $osArch"

# Check processor identifier from registry
$procId = (Get-ItemProperty "HKLM:\SYSTEM\CurrentControlSet\Control\Session Manager\Environment").PROCESSOR_IDENTIFIER
Write-Log "Processor identifier: $procId"

$isArm64 = ($osArch -like "*ARM*") -or ($procId -like "*ARM*")
Write-Log "Is ARM64 machine: $isArm64"

# Get PE architecture from file
function Get-PEArchitecture {
    param([string]$FilePath)
    
    if (-not (Test-Path $FilePath)) {
        return "FileNotFound"
    }
    
    try {
        $bytes = [System.IO.File]::ReadAllBytes($FilePath)
        $peOffset = [BitConverter]::ToInt32($bytes, 0x3C)
        $machineType = [BitConverter]::ToUInt16($bytes, $peOffset + 4)
        
        switch ($machineType) {
            0x014C { return "x86" }
            0x8664 { return "x64" }
            0xAA64 { return "ARM64" }
            default { return "Unknown (0x$($machineType.ToString('X4')))" }
        }
    }
    catch {
        return "Error: $_"
    }
}

# Find and log all agent installations
function Log-AllAgentInstallations {
    $searchPaths = @(
        "C:\vss-agent",
        "C:\Agent",
        "D:\vss-agent",
        "D:\Agent",
        "C:\azagent",
        "D:\azagent"
    )
    
    Write-Log "Searching for agent installations..."
    foreach ($basePath in $searchPaths) {
        Write-Log "  Checking: $basePath"
        if (Test-Path $basePath) {
            Write-Log "    Path exists. Full directory listing:"
            
            # List ALL contents recursively
            Get-ChildItem -Path $basePath -Recurse -ErrorAction SilentlyContinue | ForEach-Object {
                $type = if ($_.PSIsContainer) { "[DIR] " } else { "[FILE]" }
                $size = if ($_.PSIsContainer) { "" } else { " ($($_.Length) bytes)" }
                $arch = ""
                if (-not $_.PSIsContainer -and $_.Extension -eq ".exe") {
                    $arch = " [$(Get-PEArchitecture $_.FullName)]"
                }
                Write-Log "      $type $($_.FullName)$size$arch"
            }
            
            Write-Log "    End of directory listing."
            
            # Find and report Agent.Listener.exe files
            $agentExes = Get-ChildItem -Path $basePath -Recurse -Filter "Agent.Listener.exe" -ErrorAction SilentlyContinue
            Write-Log "    Found $($agentExes.Count) Agent.Listener.exe file(s)"
            foreach ($agentExe in $agentExes) {
                $arch = Get-PEArchitecture $agentExe.FullName
                Write-Log "      $($agentExe.FullName) - Architecture: $arch"
            }
            
            # Find and report node.exe files
            $nodeExes = Get-ChildItem -Path $basePath -Recurse -Filter "node.exe" -ErrorAction SilentlyContinue
            Write-Log "    Found $($nodeExes.Count) node.exe file(s)"
            foreach ($nodeExe in $nodeExes) {
                $arch = Get-PEArchitecture $nodeExe.FullName
                Write-Log "      $($nodeExe.FullName) - Architecture: $arch"
            }
        } else {
            Write-Log "    Path does not exist"
        }
    }
}

Log-AllAgentInstallations

Write-Log ""
Write-Log "==========================================="
Write-Log "Provisioning complete!"
Write-Log "==========================================="

exit 0
#endregion
