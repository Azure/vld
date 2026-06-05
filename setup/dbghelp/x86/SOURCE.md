# Source of bundled x86 dbghelp.dll

## Current contents

| File         | Version           | Size (bytes) |
| ------------ | ----------------- | ------------ |
| dbghelp.dll  | 10.0.26100.5074   | 1854352      |

Microsoft.DTfW.DHL.manifest is the standard Microsoft side-by-side activation
manifest shipped alongside the redistributable dbghelp.

## Provenance

The Microsoft Windows Kits 10 "Debugging Tools for Windows" SDK component does
not include an x86 dbghelp.dll on modern installations. The closest available
redistributable source is the system-shipped 32-bit dbghelp.dll in:

    C:\Windows\SysWOW64\dbghelp.dll

That is a Microsoft-redistributed copy of the Windows Image Helper Library
identical in lineage to the SDK Debuggers redistributable; it is the same
binary Microsoft ships to every Windows machine. VLD copies it next to every
x86 test executable so that x86 test behavior does not depend on whatever
dbghelp the system loader happens to find first.

The companion import library lives in `lib/dbghelp/lib/Win32/DbgHelp.Lib`.

## How to update

1. Look for a newer dbghelp under the Windows SDK "Debugging Tools for
   Windows" component first. If a `Debuggers\x86\dbghelp.dll` exists on the
   SDK install, prefer that; otherwise fall back to `C:\Windows\SysWOW64\
   dbghelp.dll` from an up-to-date Windows installation.
2. Copy it into this folder, replacing the existing file.
3. Re-run the full test suite (x86 + x64 + ARM64) before committing. Past
   updates of this DLL changed how dbghelp resolves static-function names,
   which can break exact-match `IgnoreFunctionsList` entries in
   `ignore_functions_test` and similar tests; expect to look at the test
   output if those start failing.
