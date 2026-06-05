# Source of bundled x86 DbgHelp.Lib

## Currently bundled file

| Field        | Value                                                                       |
| ------------ | --------------------------------------------------------------------------- |
| Size on disk | 60,310 bytes                                                                |
| SHA256       | 107C8DB3ED4588056D57A255DC591955509A8F5F0F824863335A17480B4B8E7A            |

Get the stamped hash at any time with:

```powershell
Get-FileHash lib\dbghelp\lib\Win32\DbgHelp.Lib -Algorithm SHA256
```

## Exact source

The import library is copied verbatim from:

    C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\um\x86\DbgHelp.Lib

on a dev machine with **Windows 10 SDK version 10.0.26100.0** installed.
Same SDK release as the bundled header (`lib/dbghelp/include/DbgHelp.h`).

Note on the runtime DLL pairing: as of SDK 10.0.26100.0 the Debuggers
component does not install an x86 redistributable `dbghelp.dll`, so
`setup/dbghelp/x86/dbghelp.dll` is sourced from the system-shipped
`C:\Windows\SysWOW64\dbghelp.dll` instead. See
`setup/dbghelp/x86/SOURCE.md` for details. The system and SDK x86 dbghelp
binaries share the same export surface, so linking against this SDK
import library and loading the system DLL at runtime works.

VLD bundles this `.Lib` so the link step does not have to discover an SDK
install path.

## How to update

1. Install or update **Windows 10 SDK**. Record the SDK version.
2. Copy the new import library from:

        C:\Program Files (x86)\Windows Kits\10\Lib\<sdk-version>\um\x86\DbgHelp.Lib

   into this folder, replacing the existing file.
3. Update the "Currently bundled file" table above with the new size and
   SHA256.
4. Refresh `setup/dbghelp/x86/dbghelp.dll` and
   `lib/dbghelp/include/DbgHelp.h` in the same commit so the import
   library, the header, and the runtime DLL all stay aligned.
5. Rebuild and re-run the full test suite before committing.
