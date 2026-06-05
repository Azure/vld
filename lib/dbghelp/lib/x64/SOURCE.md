# Source of bundled x64 DbgHelp.Lib

## Currently bundled file

| Field        | Value                                                                       |
| ------------ | --------------------------------------------------------------------------- |
| Size on disk | 57,836 bytes                                                                |
| SHA256       | 1D29FC9E1C4D22E7D399D13C3295330704BE210AE0E087798903859EEFF31BBA            |

Get the stamped hash at any time with:

```powershell
Get-FileHash lib\dbghelp\lib\x64\DbgHelp.Lib -Algorithm SHA256
```

## Exact source

The import library is copied verbatim from:

    C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\um\x64\DbgHelp.Lib

on a dev machine with **Windows 10 SDK version 10.0.26100.0** installed.
Same SDK release as the bundled header (`lib/dbghelp/include/DbgHelp.h`)
and the x64 runtime DLL (`setup/dbghelp/x64/dbghelp.dll`, file version
`10.0.26100.8249` from the Debuggers component of the same SDK installer).

VLD bundles this `.Lib` so the link step does not have to discover an SDK
install path.

## How to update

1. Install or update **Windows 10 SDK**. Record the SDK version.
2. Copy the new import library from:

        C:\Program Files (x86)\Windows Kits\10\Lib\<sdk-version>\um\x64\DbgHelp.Lib

   into this folder, replacing the existing file.
3. Update the "Currently bundled file" table above with the new size and
   SHA256.
4. Refresh `setup/dbghelp/x64/dbghelp.dll` and `lib/dbghelp/include/DbgHelp.h`
   from the same SDK installer in the same commit so the import library,
   the header, and the runtime DLL all come from the same SDK version.
5. Rebuild and re-run the full test suite before committing.
