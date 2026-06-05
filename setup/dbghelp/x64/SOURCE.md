# Source of bundled x64 dbghelp.dll

## Currently bundled file

| Field                    | Value                                                              |
| ------------------------ | ------------------------------------------------------------------ |
| `dbghelp.dll` FileVersion | **10.0.26100.8249** (WinBuild.160101.0800)                         |
| ProductVersion           | 10.0.26100.8249                                                    |
| OriginalFilename         | DBGHELP.DLL                                                        |
| FileDescription          | Windows Image Helper                                               |
| Size on disk             | 2,259,384 bytes                                                    |

Get the stamped version of the local file at any time with:

```powershell
(Get-Item setup\dbghelp\x64\dbghelp.dll).VersionInfo | Format-List FileVersion, ProductVersion
```

`Microsoft.DTfW.DHL.manifest` is the standard side-by-side activation manifest
that ships with the Windows redistributable dbghelp.

## Exact source

The DLL was copied verbatim from:

    C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\dbghelp.dll

on a dev machine with **Windows 10 SDK version 10.0.26100.0** installed, with
the *"Debugging Tools for Windows"* optional component selected during SDK
setup. With that component installed, the x64 Debuggers folder ships exactly
four redistributable binaries:

    dbghelp.dll   (2,259,384 bytes) -- the one VLD bundles
    dbgcore.dll
    srcsrv.dll
    symsrv.dll

The SDK installer ships this dbghelp redistributable under the standard
Microsoft Software License Terms accompanying the Debugging Tools for Windows.

Windows 10 SDK installer download:
https://developer.microsoft.com/en-us/windows/downloads/windows-sdk/

The companion import library lives in `lib/dbghelp/lib/x64/DbgHelp.Lib`; the
public header lives in `lib/dbghelp/include/DbgHelp.h`. See those folders'
SOURCE.md for their provenance (both predate the current SDK and were
inherited from the upstream VLD 2.5.15 release; they are not the
`Lib\10.0.26100.0\um\x64\DbgHelp.Lib` and `Include\10.0.26100.0\um\DbgHelp.h`
that ship with the current SDK).

## How to update

1. Install or update **Windows 10 SDK** ("Debugging Tools for Windows"
   optional component selected).
2. Record the SDK version number (e.g. `10.0.26100.0`) and the file version
   stamp of the new dbghelp.dll (`(Get-Item ...).VersionInfo.FileVersion`).
3. Copy the x64 dbghelp.dll from the Debuggers `x64\` folder into this
   folder, replacing the existing file.
4. Update the "Currently bundled file" table above with the new FileVersion
   and size.
5. Re-run the full test suite (x86 + x64 + ARM64) before committing. Past
   updates of this DLL changed how dbghelp resolves static-function names,
   which can break exact-match `IgnoreFunctionsList` entries in
   `ignore_functions_test` and similar tests; expect to look at the test
   output if those start failing.
