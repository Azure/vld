<#
.SYNOPSIS
    Provisioning script to patch Azure DevOps agent to ARM64 on ARM64 VMs
    and install required build tools.
    
.DESCRIPTION
    This script runs at VM spin-up time (before the agent starts) to:
    1. Replace the x86 agent binaries with native ARM64 binaries
    2. Install Inno Setup 6.7.0 for all users
    
    WHY AGENT PATCHING IS NEEDED:
    Azure DevOps 1ES Hosted Pools ship with an x86 agent on ARM64 machines.
    This x86 agent runs under WoW64 emulation, causing:
    - Incorrect PROCESSOR_ARCHITECTURE environment variable (reports x86)
    - File system redirection hiding native ARM64 tools
    - CMake/MSBuild selecting cross-compilers instead of native compilers
    - Performance overhead from emulation
    
    WHY RUNTIME PATCHING DOESN'T WORK:
    The agent cannot be patched during pipeline execution because Agent.Worker.exe
    and Agent.Listener.exe are running - Windows prevents overwriting files in use.
    
    STEP-BY-STEP OPERATION:
    1. Install Inno Setup 6.7.0 silently (all architectures)
    2. Detect if running on ARM64 machine (using OS architecture, not process arch)
    3. Find agent installation at C:\vss-agent\<version>
    4. Check if agent is already ARM64 (skip if so - idempotent)
    5. Stop any agent services (shouldn't be running at provisioning time)
    6. Backup configuration files (.agent, .credentials, etc.)
    7. Download matching ARM64 agent from official Azure DevOps URL
    8. Extract ARM64 agent over existing installation (overwrite)
    9. Restore configuration files
    10. Verify agent is now ARM64
    
    OBSERVABILITY:
    Logs are written to Azure Blob Storage and can be viewed at:
    https://winbuildpoolarm.blob.core.windows.net/insights-logs-provisioningscriptlogs
    
    Filter by:
    - category: "ProvisioningScriptLogs"
    - Look for script version in output (e.g., "VLD Provisioning Script v1.4.0")
    
.NOTES
    - Runs as provisioning script in 1ES image configuration
    - Agent is NOT running at this point, so files are not locked
    - Idempotent: skips patching if already ARM64
    - Preserves agent configuration files (.credentials, .agent, etc.)
    
    SOURCE: build/scripts/Setup.ps1 in the VLD repository
    DOCS:   build/docs/ARM64-AGENT-PATCHING.md
#>

$ErrorActionPreference = "Stop"
$ScriptVersion = "1.4.0"

# Logging helper
function Write-Log {
    param([string]$Message)
    $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    Write-Host "[$timestamp] $Message"
}

Write-Log "==========================================="
Write-Log "VLD Provisioning Script v$ScriptVersion"
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

#region ARM64 Agent Patching
Write-Log ""
Write-Log "--- ARM64 Agent Patching ---"

# Check if this is an ARM64 machine (check actual OS, not process architecture)
# $env:PROCESSOR_ARCHITECTURE returns x86 when running under WoW64
$osArch = (Get-CimInstance Win32_OperatingSystem).OSArchitecture
Write-Log "OS Architecture: $osArch"

# Also check processor identifier from registry as backup
$procId = (Get-ItemProperty "HKLM:\SYSTEM\CurrentControlSet\Control\Session Manager\Environment").PROCESSOR_IDENTIFIER
Write-Log "Processor identifier: $procId"

$isArm64 = ($osArch -like "*ARM*") -or ($procId -like "*ARM*")

if (-not $isArm64) {
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
        # Normalize path separators
        $relativePath = $entry.FullName -replace '/', '\'
        $destPath = Join-Path $agentPath $relativePath
        
        if ($entry.FullName.EndsWith('/') -or $entry.FullName.EndsWith('\')) {
            # Directory entry
            if (-not (Test-Path $destPath)) {
                New-Item -ItemType Directory -Path $destPath -Force | Out-Null
            }
        }
        else {
            # File entry - ensure parent directory exists
            $destDir = Split-Path $destPath -Parent
            if ($destDir -and -not (Test-Path $destDir)) {
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
#endregion
