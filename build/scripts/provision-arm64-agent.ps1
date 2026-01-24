<#
.SYNOPSIS
    Provisioning script to patch Azure DevOps agent to ARM64 on ARM64 VMs.
    
.DESCRIPTION
    This script runs at VM spin-up time (before the agent starts) to replace
    the x86 agent binaries with native ARM64 binaries. This eliminates WoW64
    emulation overhead and enables native ARM64 compilation.
    
.NOTES
    - Runs as provisioning script in 1ES image configuration
    - Agent is NOT running at this point, so files are not locked
    - Idempotent: skips patching if already ARM64
    - Preserves agent configuration files (.credentials, .agent, etc.)
#>

$ErrorActionPreference = "Stop"

# Logging helper
function Write-Log {
    param([string]$Message)
    $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    Write-Host "[$timestamp] $Message"
}

Write-Log "==========================================="
Write-Log "ARM64 Agent Provisioning Script"
Write-Log "==========================================="

# Check if this is an ARM64 machine
$arch = $env:PROCESSOR_ARCHITECTURE
Write-Log "Processor architecture: $arch"

if ($arch -ne "ARM64") {
    Write-Log "Not an ARM64 machine. Skipping."
    exit 0
}

# Find existing agent installation
function Find-AgentInstallation {
    $searchPaths = @(
        "C:\vss-agent",
        "C:\Agent",
        "D:\vss-agent",
        "D:\Agent"
    )
    
    foreach ($basePath in $searchPaths) {
        if (Test-Path $basePath) {
            # Look for version folders or direct installation
            $agentExe = Get-ChildItem -Path $basePath -Recurse -Filter "Agent.Listener.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
            if ($agentExe) {
                return $agentExe.Directory.Parent.FullName
            }
        }
    }
    return $null
}

$agentPath = Find-AgentInstallation
if (-not $agentPath) {
    Write-Log "No agent installation found. Skipping."
    exit 0
}

Write-Log "Agent found at: $agentPath"

# Get agent architecture from PE header
function Get-AgentArchitecture {
    param([string]$AgentPath)
    
    $exePath = Join-Path $AgentPath "bin\Agent.Listener.exe"
    if (-not (Test-Path $exePath)) {
        # Try without bin subfolder
        $exePath = Get-ChildItem -Path $AgentPath -Recurse -Filter "Agent.Listener.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($exePath) { $exePath = $exePath.FullName }
    }
    
    if (-not $exePath -or -not (Test-Path $exePath)) {
        return "Unknown"
    }
    
    try {
        $bytes = [System.IO.File]::ReadAllBytes($exePath)
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

$currentArch = Get-AgentArchitecture -AgentPath $agentPath
Write-Log "Current agent architecture: $currentArch"

if ($currentArch -eq "ARM64") {
    Write-Log "Agent is already ARM64. No patching needed."
    exit 0
}

if ($currentArch -ne "x86" -and $currentArch -ne "x64") {
    Write-Log "WARNING: Unexpected architecture '$currentArch'. Proceeding anyway."
}

# Get agent version from folder name or .agent file
function Get-AgentVersion {
    param([string]$AgentPath)
    
    # Try to get from folder name (e.g., C:\vss-agent\4.266.2)
    $folderName = Split-Path $AgentPath -Leaf
    if ($folderName -match '^\d+\.\d+\.\d+$') {
        return $folderName
    }
    
    # Try parent folder
    $parentName = Split-Path (Split-Path $AgentPath -Parent) -Leaf
    if ($parentName -match '^\d+\.\d+\.\d+$') {
        return $parentName
    }
    
    # Try .agent file
    $agentFile = Join-Path $AgentPath ".agent"
    if (Test-Path $agentFile) {
        try {
            $config = Get-Content $agentFile -Raw | ConvertFrom-Json
            if ($config.agentVersion) {
                return $config.agentVersion
            }
        }
        catch { }
    }
    
    # Try run.cmd for version info
    $runCmd = Join-Path $AgentPath "run.cmd"
    if (Test-Path $runCmd) {
        # Default to a known recent version
        return "4.266.2"
    }
    
    return $null
}

$agentVersion = Get-AgentVersion -AgentPath $agentPath
if (-not $agentVersion) {
    Write-Log "ERROR: Could not determine agent version."
    exit 1
}

Write-Log "Agent version: $agentVersion"

# Stop agent service if running
$stoppedServices = @()
$agentServices = Get-Service -Name "vstsagent*" -ErrorAction SilentlyContinue
foreach ($svc in $agentServices) {
    if ($svc.Status -eq 'Running') {
        Write-Log "Stopping agent service: $($svc.Name)"
        Stop-Service -Name $svc.Name -Force
        $stoppedServices += $svc.Name
        Write-Log "  Stopped."
    }
}

# Also kill any agent processes just in case
$agentProcesses = Get-Process -Name "Agent.Listener", "Agent.Worker" -ErrorAction SilentlyContinue
foreach ($proc in $agentProcesses) {
    Write-Log "Killing agent process: $($proc.Name) (PID: $($proc.Id))"
    Stop-Process -Id $proc.Id -Force
}

# Define config files to preserve
$configFiles = @(
    ".agent",
    ".credentials",
    ".credentials_rsaparams",
    ".env",
    ".path",
    ".proxy",
    ".proxybypass",
    ".proxyCredentials"
)

# Backup config files
$backupPath = Join-Path $env:TEMP "agent-config-backup-$(Get-Date -Format 'yyyyMMddHHmmss')"
New-Item -ItemType Directory -Path $backupPath -Force | Out-Null

Write-Log "Backing up configuration files to $backupPath"
foreach ($file in $configFiles) {
    $sourcePath = Join-Path $agentPath $file
    if (Test-Path $sourcePath) {
        Copy-Item -Path $sourcePath -Destination $backupPath -Force
        Write-Log "  Backed up: $file"
    }
}

# Download ARM64 agent
$downloadUrl = "https://download.agent.dev.azure.com/agent/$agentVersion/vsts-agent-win-arm64-$agentVersion.zip"
$downloadPath = Join-Path $env:TEMP "vsts-agent-arm64-$agentVersion.zip"

Write-Log "Downloading ARM64 agent from: $downloadUrl"

try {
    $webClient = New-Object System.Net.WebClient
    $webClient.DownloadFile($downloadUrl, $downloadPath)
    Write-Log "Download complete: $downloadPath"
}
catch {
    Write-Log "ERROR: Failed to download ARM64 agent: $_"
    exit 1
}

# Verify download
if (-not (Test-Path $downloadPath) -or (Get-Item $downloadPath).Length -lt 1MB) {
    Write-Log "ERROR: Download failed or file is too small."
    exit 1
}

Write-Log "Download size: $((Get-Item $downloadPath).Length / 1MB) MB"

# Extract with overwrite
Write-Log "Extracting ARM64 agent with overwrite..."

try {
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    
    $zip = [System.IO.Compression.ZipFile]::OpenRead($downloadPath)
    $totalFiles = $zip.Entries.Count
    $extracted = 0
    
    foreach ($entry in $zip.Entries) {
        $destPath = Join-Path $agentPath $entry.FullName
        
        if ($entry.FullName.EndsWith('/')) {
            # Directory entry
            if (-not (Test-Path $destPath)) {
                New-Item -ItemType Directory -Path $destPath -Force | Out-Null
            }
        }
        else {
            # File entry
            $destDir = Split-Path $destPath -Parent
            if (-not (Test-Path $destDir)) {
                New-Item -ItemType Directory -Path $destDir -Force | Out-Null
            }
            
            [System.IO.Compression.ZipFileExtensions]::ExtractToFile($entry, $destPath, $true)
            $extracted++
        }
    }
    
    $zip.Dispose()
    Write-Log "Extracted $extracted files."
}
catch {
    Write-Log "ERROR: Extraction failed: $_"
    exit 1
}

# Restore config files
Write-Log "Restoring configuration files..."
foreach ($file in $configFiles) {
    $backupFile = Join-Path $backupPath $file
    if (Test-Path $backupFile) {
        $destFile = Join-Path $agentPath $file
        Copy-Item -Path $backupFile -Destination $destFile -Force
        Write-Log "  Restored: $file"
    }
}

# Cleanup
Write-Log "Cleaning up temporary files..."
Remove-Item -Path $downloadPath -Force -ErrorAction SilentlyContinue
Remove-Item -Path $backupPath -Recurse -Force -ErrorAction SilentlyContinue

# Verify the patch
$newArch = Get-AgentArchitecture -AgentPath $agentPath
Write-Log "New agent architecture: $newArch"

if ($newArch -eq "ARM64") {
    Write-Log "==========================================="
    Write-Log "SUCCESS: Agent patched to ARM64!"
    Write-Log "==========================================="
    
    # Restart any services we stopped
    foreach ($svcName in $stoppedServices) {
        Write-Log "Restarting service: $svcName"
        Start-Service -Name $svcName
        Write-Log "  Started."
    }
    
    exit 0
}
else {
    Write-Log "ERROR: Patch verification failed. Architecture is $newArch"
    
    # Still try to restart services even on failure
    foreach ($svcName in $stoppedServices) {
        Write-Log "Restarting service: $svcName"
        Start-Service -Name $svcName -ErrorAction SilentlyContinue
    }
    
    exit 1
}
