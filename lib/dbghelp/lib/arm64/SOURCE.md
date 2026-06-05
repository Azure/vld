# Source of bundled ARM64 dbghelp.dll (link-time copy)

## Current contents

| File         | Version           | Size (bytes) |
| ------------ | ----------------- | ------------ |
| dbghelp.dll  | 10.0.26100.8249   | 4271528      |

## Provenance

This is the same Microsoft-redistributable ARM64 dbghelp.dll bundled under
`setup/dbghelp/arm64/dbghelp.dll`; see `setup/dbghelp/arm64/SOURCE.md` for
the full provenance.

The redistributable was obtained from the Microsoft "Debugging Tools for
Windows" component of the Windows 10 SDK (Windows Kits 10), Debuggers
package. On a machine with the Debuggers component installed, the DLL
is located at:

    C:\Program Files (x86)\Windows Kits\10\Debuggers\arm64\dbghelp.dll

ARM64 has no companion `.lib` file in the Windows SDK; the linker resolves
dbghelp imports against the system import library at link time, and at run
time the bundled DLL is loaded from the directory next to the executable.

## How to update

1. Update `setup/dbghelp/arm64/dbghelp.dll` first using the procedure
   documented in `setup/dbghelp/arm64/SOURCE.md`.
2. Mirror the same DLL into this folder.
3. Re-run the full ARM64 test suite before committing.
