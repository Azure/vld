# Source of bundled ARM64 dbghelp.dll

## Current contents

| File         | Version           | Size (bytes) |
| ------------ | ----------------- | ------------ |
| dbghelp.dll  | 10.0.26100.8249   | 4271528      |

Microsoft.DTfW.DHL.manifest is the standard Microsoft side-by-side activation
manifest shipped alongside the redistributable dbghelp.

## Provenance

The DLL is the ARM64 build of dbghelp from the Microsoft "Debugging Tools for
Windows" component of the Windows 10 SDK (Windows Kits 10), Debuggers package.

On a machine with the Debuggers component installed, the DLL is located at:

    C:\Program Files (x86)\Windows Kits\10\Debuggers\arm64\dbghelp.dll

This is the redistributable version of dbghelp and ships under the standard
Microsoft Software License Terms that accompany the Debugging Tools for
Windows. It is loaded by the VLD test executables and by anything that ships
with VLD on ARM64 so that VLD does not have to depend on the system dbghelp,
whose behavior changes across Windows updates.

The matching link-time copy of this file lives in `lib/dbghelp/lib/arm64/`.

## How to update

1. Install or update the Windows SDK "Debugging Tools for Windows" component.
2. Copy the ARM64 dbghelp.dll from the path above into this folder, replacing
   the existing file.
3. Mirror the same file into `lib/dbghelp/lib/arm64/dbghelp.dll`.
4. Re-run the full test suite on ARM64 before committing.
