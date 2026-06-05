# Source of bundled x86 dbghelp.dll

## Currently bundled file

| Field                    | Value                                                              |
| ------------------------ | ------------------------------------------------------------------ |
| `dbghelp.dll` FileVersion | **10.0.26100.5074** (WinBuild.160101.0800)                         |
| ProductVersion           | 10.0.26100.5074                                                    |
| OriginalFilename         | DBGHELP.DLL                                                        |
| FileDescription          | Windows Image Helper                                               |
| Size on disk             | 1,854,352 bytes                                                    |

Get the stamped version of the local file at any time with:

```powershell
(Get-Item setup\dbghelp\x86\dbghelp.dll).VersionInfo | Format-List FileVersion, ProductVersion
```

`Microsoft.DTfW.DHL.manifest` is the standard side-by-side activation manifest
that ships with the Windows redistributable dbghelp.

## Exact source

As of Windows 10 SDK version **10.0.26100.0** the "Debugging Tools for
Windows" optional component installs ONLY an x64 Debuggers folder. The SDK
no longer ships an x86 dbghelp redistributable on installs verified during
this update:

    C:\Program Files (x86)\Windows Kits\10\Debuggers\
        x64\
            dbghelp.dll  <-- only this exists
        (no x86 folder)
        (no arm64 folder unless installed from an ARM64 machine)

The fallback redistributable x86 dbghelp is the system-shipped 32-bit copy:

    C:\Windows\SysWOW64\dbghelp.dll

That is the Microsoft-published, redistributable Windows Image Helper Library
identical in lineage to the SDK Debuggers redistributable. It was extracted
from a dev machine running:

    Windows 11 Enterprise, build 26200.8390 (25H2 / Cumulative Update KB-level
    determines the dbghelp UBR; on this machine the stamp was 10.0.26100.5074
    even though the host OS reports build 26200.8390).

## How to update

1. Look for a newer dbghelp under the Windows 10 SDK "Debugging Tools for
   Windows" component first. If a future SDK version starts shipping a
   `Debuggers\x86\dbghelp.dll` again, prefer that and document its SDK
   version below.
2. Otherwise fall back to `C:\Windows\SysWOW64\dbghelp.dll` from an
   up-to-date Windows 11 installation. Record the OS build/UBR (eg
   `winver` / `(Get-CimInstance Win32_OperatingSystem).Version`) and the
   stamped FileVersion of the copied DLL.
3. Copy the new file into this folder, replacing the existing one.
4. Update the "Currently bundled file" table above with the new FileVersion
   and size.
5. Re-run the full test suite (x86 + x64 + ARM64) before committing. Past
   updates of this DLL changed how dbghelp resolves static-function names,
   which can break exact-match `IgnoreFunctionsList` entries in
   `ignore_functions_test` and similar tests.
