# Source of bundled DbgHelp.h

## Currently bundled file

| Field             | Value                                                                       |
| ----------------- | --------------------------------------------------------------------------- |
| Size on disk      | 124,135 bytes                                                               |
| SHA256            | 55EC5FFEA6C2A099316DF485CD054C46E48FFC7B016F81158F8B5D6FED6D161C            |
| First-line marker | `/*++ BUILD Version: 0000     Increment this if a change has global effects`|

Get the stamped header hash at any time with:

```powershell
Get-FileHash lib\dbghelp\include\DbgHelp.h -Algorithm SHA256
```

## Exact source

The header is copied verbatim from:

    C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\um\DbgHelp.h

on a dev machine with **Windows 10 SDK version 10.0.26100.0** installed.
Same SDK release as the bundled redistributable `dbghelp.dll` files under
`setup/dbghelp/` (those are version `10.0.26100.8249`, drawn from the
Debuggers component of the same SDK installer).

The matching x64 and x86 import libraries live in `lib/dbghelp/lib/x64/`
and `lib/dbghelp/lib/Win32/`; they were copied from the matching paths
under `Lib\10.0.26100.0\um\<arch>\` of the same SDK install.

The structure layouts declared in this header are coupled to those
redistributables. The static assertion near the top of `src/vld.cpp`
catches accidental drift at compile time:

    static char dbghelp32_assert[sizeof(IMAGEHLP_MODULE64) == 3264 ? 1 : -1];

`IMAGEHLP_MODULE64` is byte-identical between the bundled redistributable
DLLs and the SDK header for the 10.0.26100.x release line, so the
assertion holds without modification.

Windows 10 SDK installer download:
https://developer.microsoft.com/en-us/windows/downloads/windows-sdk/

## How to update

1. Install or update **Windows 10 SDK**. Record the SDK version
   (eg `10.0.26100.0`).
2. Copy the new header from:

        C:\Program Files (x86)\Windows Kits\10\Include\<sdk-version>\um\DbgHelp.h

   into this folder, replacing the existing file.
3. Update the "Currently bundled file" table above with the new size and
   SHA256.
4. Update the import libraries under `lib/dbghelp/lib/x64/` and
   `lib/dbghelp/lib/Win32/` from the same SDK install
   (`Lib\<sdk-version>\um\x64\DbgHelp.Lib` and
   `Lib\<sdk-version>\um\x86\DbgHelp.Lib`).
5. Update each bundled `dbghelp.dll` under `setup/dbghelp/` so the
   runtime DLL is from the same SDK release as the header.
6. If `sizeof(IMAGEHLP_MODULE64)` changed (very rare; the struct has
   not grown in 15+ years), update the static assertion in `src/vld.cpp`
   (`dbghelp32_assert`).
7. Rebuild and re-run the full test suite (x86 + x64 + ARM64) before
   committing.
