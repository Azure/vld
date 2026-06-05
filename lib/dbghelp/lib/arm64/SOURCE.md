# Source of bundled ARM64 dbghelp.dll (link-time copy)

## Currently bundled file

| Field                    | Value                                                              |
| ------------------------ | ------------------------------------------------------------------ |
| `dbghelp.dll` FileVersion | **10.0.26100.8249** (WinBuild.160101.0800)                         |
| Size on disk             | 4,271,528 bytes                                                    |

Get the stamped version of the local file at any time with:

```powershell
(Get-Item lib\dbghelp\lib\arm64\dbghelp.dll).VersionInfo | Format-List FileVersion, ProductVersion
```

## Exact source

This is a verbatim mirror of `setup/dbghelp/arm64/dbghelp.dll`. The
upstream source is the ARM64 build of redistributable dbghelp from the
**Windows 10 SDK version 10.0.26100.0**, *"Debugging Tools for Windows"*
optional component, installed on an ARM64 host:

    C:\Program Files (x86)\Windows Kits\10\Debuggers\arm64\dbghelp.dll

See `setup/dbghelp/arm64/SOURCE.md` for the full provenance and licensing
notes.

ARM64 has no companion `.lib` file shipped from the Debuggers component;
the linker resolves dbghelp imports against the system import library at
link time, and at run time the bundled DLL is loaded from the directory
next to the executable.

## How to update

1. Update `setup/dbghelp/arm64/dbghelp.dll` first using the procedure
   documented in `setup/dbghelp/arm64/SOURCE.md`.
2. Mirror the same DLL into this folder.
3. Update the "Currently bundled file" table above with the new FileVersion
   and size so both SOURCE.md files agree.
4. Re-run the full ARM64 test suite before committing.
