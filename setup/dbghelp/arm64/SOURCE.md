# Source of bundled ARM64 dbghelp.dll

## Currently bundled file

| Field                    | Value                                                              |
| ------------------------ | ------------------------------------------------------------------ |
| `dbghelp.dll` FileVersion | **10.0.26100.8249** (WinBuild.160101.0800)                         |
| ProductVersion           | 10.0.26100.8249                                                    |
| OriginalFilename         | DBGHELP.DLL                                                        |
| FileDescription          | Windows Image Helper                                               |
| Size on disk             | 4,271,528 bytes                                                    |

Get the stamped version of the local file at any time with:

```powershell
(Get-Item setup\dbghelp\arm64\dbghelp.dll).VersionInfo | Format-List FileVersion, ProductVersion
```

`Microsoft.DTfW.DHL.manifest` is the standard side-by-side activation manifest
that ships with the Windows redistributable dbghelp.

## Exact source

The DLL is the ARM64 build of the redistributable Windows Image Helper Library
from the **Windows 10 SDK version 10.0.26100.0**, *"Debugging Tools for
Windows"* optional component, installed on an ARM64 host.

On an ARM64 dev machine with that component installed, the file lives at:

    C:\Program Files (x86)\Windows Kits\10\Debuggers\arm64\dbghelp.dll

The x64 dev machines used to develop on this repo do not have the ARM64
Debuggers folder (SDK installers select per-host architecture by default),
so the ARM64 DLL is contributed from an ARM64 box and committed verbatim.

The SDK installer ships this dbghelp redistributable under the standard
Microsoft Software License Terms accompanying the Debugging Tools for Windows.

Windows 10 SDK installer download:
https://developer.microsoft.com/en-us/windows/downloads/windows-sdk/

The matching link-time copy of this file lives in `lib/dbghelp/lib/arm64/`.
There is no ARM64 import library or header bundled separately in this repo;
ARM64 link-time symbol resolution uses the system import library found by
the toolchain.

## How to update

1. On an ARM64 dev machine, install or update the **Windows 10 SDK**
   ("Debugging Tools for Windows" optional component selected).
2. Record the SDK version number and the file version stamp of the new
   dbghelp.dll.
3. Copy the ARM64 `dbghelp.dll` from the path above into this folder,
   replacing the existing file.
4. Mirror the same file into `lib/dbghelp/lib/arm64/dbghelp.dll`.
5. Update the "Currently bundled file" table above with the new FileVersion
   and size.
6. Re-run the full test suite on ARM64 before committing.
