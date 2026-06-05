# Source of bundled x86 DbgHelp.Lib

## Currently bundled file

| Field        | Value                                                                       |
| ------------ | --------------------------------------------------------------------------- |
| Size on disk | 56,180 bytes                                                                |
| SHA256       | (run `Get-FileHash lib\dbghelp\lib\Win32\DbgHelp.Lib -Algorithm SHA256`)    |

## Exact source

This import library was inherited verbatim from the upstream **VLD 2.5.15**
release commit `4d6e7da` and has not been refreshed since. It does NOT match
the x86 DbgHelp.Lib that ships with the currently-installed Windows 10 SDK
version 10.0.26100.0:

    C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\um\x86\DbgHelp.Lib
        size = 60,310 bytes
        DOES NOT MATCH

The bundled file is therefore from an older SDK release (the exact version
was not recorded by upstream). It is paired by symbol-level compatibility
with the runtime DLL in `setup/dbghelp/x86/dbghelp.dll` and with the bundled
header in `lib/dbghelp/include/DbgHelp.h`.

## How to update

1. Install or update **Windows 10 SDK** ("Debugging Tools for Windows"
   optional component selected). Record the SDK version.
2. Copy the new import library from:

        C:\Program Files (x86)\Windows Kits\10\Lib\<sdk-version>\um\x86\DbgHelp.Lib

   into this folder, replacing the existing file.
3. Update the "Currently bundled file" table above with the new size.
4. Update `setup/dbghelp/x86/dbghelp.dll` and `lib/dbghelp/include/DbgHelp.h`
   in the same commit so the import library, the header, and the runtime
   DLL all come from the same SDK version.
5. Rebuild and re-run the full test suite before committing.
