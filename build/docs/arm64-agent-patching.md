# ARM64 Agent Patching for Azure DevOps Builds

## Overview

This document explains why ARM64 builds require special handling in Azure DevOps
pipelines and how we address this using a 1ES provisioning script.

## The Problem

Azure DevOps 1ES Hosted Pools for ARM64 machines ship with an **x86 (32-bit) agent**
(`Agent.Worker.exe`), not a native ARM64 agent. This x86 agent runs under Windows'
WoW64 (Windows on Windows 64-bit) emulation layer on ARM64 hardware.

### Why the x86 Agent is Problematic

1. **Environment Variables Lie**: When running under WoW64, `%PROCESSOR_ARCHITECTURE%`
   returns `x86` instead of `ARM64`, which confuses build tools.

2. **File System Redirection**: WoW64 redirects `System32` to `SysWOW64`, so native
   ARM64 tools like `msbuild.exe` and `cl.exe` are not directly accessible.

3. **Build Tool Selection**: CMake and MSBuild may select x86-to-ARM64 cross-compilers
   (`HostX86\arm64\cl.exe`) instead of native ARM64 compilers (`Hostarm64\arm64\cl.exe`).

4. **Performance**: Running the build toolchain under emulation adds overhead.

## Approaches Considered

### 1. WoW64 Escape via SysNative (Workaround)

We initially tried escaping WoW64 by invoking native binaries through 
`%SystemRoot%\SysNative`, which redirects to the real `System32` from a 32-bit process.

**Drawbacks:**
- Requires wrapping every native tool invocation
- Complex pipeline logic with conditional templates
- Some tools don't work correctly when launched this way
- Environment variables still report x86 architecture

### 2. Runtime Agent Patching in Pipeline (Failed)

We attempted to patch the agent at runtime (in `devops.yml`) by downloading the ARM64
agent and overwriting the x86 binaries.

**Why This Cannot Work:**
- The agent process (`Agent.Worker.exe`, `Agent.Listener.exe`) is **running** while
  executing the pipeline steps
- Windows prevents overwriting files that are in use: 
  `"The process cannot access the file because it is being used by another process"`
- There is no way to stop the agent mid-pipeline and restart it

### 3. 1ES Provisioning Script (Current Solution) ✓

The provisioning script runs **before the agent starts**, during VM spin-up. At this
point, no agent files are locked, allowing us to replace the x86 agent with ARM64.

## Current Implementation

### Provisioning Script Location

The script is hosted in Azure Blob Storage and configured in the 1ES image settings:

- **Script URL**: `https://winbuildpoolarm.blob.core.windows.net/provisioning-for-1es-image/Setup.ps1`
- **Source**: `build/scripts/Setup.ps1` in this repository

### What the Script Does

1. **Detects ARM64 machine** - Uses OS architecture (not process architecture) to
   correctly identify ARM64 even when running under WoW64
2. **Finds agent installation** - Searches common paths (`C:\vss-agent\<version>`)
3. **Checks current architecture** - Reads PE header to determine if agent is x86/x64/ARM64
4. **Skips if already ARM64** - Idempotent; safe to run multiple times
5. **Stops agent services** - Ensures files aren't locked (shouldn't be running anyway)
6. **Backs up config files** - Preserves `.agent`, `.credentials`, etc.
7. **Downloads ARM64 agent** - From official Azure DevOps agent download URL
8. **Extracts with overwrite** - Replaces x86 binaries with ARM64
9. **Restores config files** - Preserves agent registration
10. **Verifies the patch** - Confirms agent is now ARM64

### Observability

Provisioning script logs are available in Azure Blob Storage:

**Log Location**: `https://winbuildpoolarm.blob.core.windows.net/insights-logs-provisioningscriptlogs`

Look for entries with:
- `category: "ProvisioningScriptLogs"`
- `operationName: "StandardOutput"`
- Script version in message (e.g., "ARM64 Agent Provisioning Script v1.2.0")

### Pipeline Verification

The pipeline includes a verification step that checks the agent architecture at
runtime. If the agent is not ARM64, the build will fail with instructions to
investigate the provisioning script logs.

## Maintenance

### Updating the Script

1. Edit `build/scripts/Setup.ps1`
2. Increment `$ScriptVersion` variable
3. Upload to blob storage
4. Update the version in 1ES image configuration (forces re-provisioning)

### Troubleshooting

If ARM64 builds fail with architecture verification errors:

1. Check provisioning logs at the URL above
2. Look for the script version to ensure latest is running
3. Common issues:
   - Script not finding agent installation path
   - Download failures (network/firewall)
   - Permission issues during extraction
   - Agent version mismatch (ARM64 version not available)

## References

- [Azure DevOps Agent Releases](https://github.com/microsoft/azure-pipelines-agent/releases)
- [1ES Hosted Pools Documentation](https://eng.ms/docs/cloud-ai-platform/devdiv/one-engineering-system-1es/1es-docs)
- [WoW64 Implementation Details](https://docs.microsoft.com/en-us/windows/win32/winprog64/wow64-implementation-details)

## Additional ARM64 Build Considerations

### MSBuild Task Selection

Even with a native ARM64 agent, the Azure DevOps `MSBuild@1` task still uses the
**x86 MSBuild executable** (`MSBuild\Current\Bin\msbuild.exe`). This causes x86
compilers to be selected even when targeting ARM64.

**Solution**: For ARM64 builds, the pipeline invokes the ARM64 MSBuild directly:
```
C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\arm64\MSBuild.exe
```

This ensures the native ARM64 compiler (`Hostarm64\arm64\cl.exe`) is used instead
of the cross-compiler (`HostX86\arm64\cl.exe`).

### Compiler Path Summary

| MSBuild Used | Compiler Path | Performance |
|--------------|---------------|-------------|
| x86 MSBuild (`Bin\msbuild.exe`) | `HostX86\arm64\cl.exe` | Slow (x86 emulation) |
| ARM64 MSBuild (`Bin\arm64\msbuild.exe`) | `Hostarm64\arm64\cl.exe` | Fast (native) |
