# Source of bundled x64 dbghelp.dll

## Current contents

| File         | Version           | Size (bytes) |
| ------------ | ----------------- | ------------ |
| dbghelp.dll  | 10.0.26100.8249   | 2259384      |

Microsoft.DTfW.DHL.manifest is the standard Microsoft side-by-side activation
manifest shipped alongside the redistributable dbghelp.

## Provenance

The DLL is the x64 build of dbghelp from the Microsoft "Debugging Tools for
Windows" component of the Windows 10 SDK (Windows Kits 10), Debuggers package.

On a machine with the Debuggers component installed, the DLL is located at:

    C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\dbghelp.dll

This is the redistributable version of dbghelp and ships under the standard
Microsoft Software License Terms that accompany the Debugging Tools for
Windows. VLD copies it next to every test executable and ships it so that
behavior does not depend on whichever dbghelp the system loader happens to
find first.

The companion import library lives in `lib/dbghelp/lib/x64/DbgHelp.Lib`.

## How to update

1. Install or update the Windows SDK "Debugging Tools for Windows" component.
2. Copy the x64 dbghelp.dll from the path above into this folder, replacing
   the existing file.
3. Re-run the full test suite (x86 + x64 + ARM64) before committing. Past
   updates of this DLL changed how dbghelp resolves static-function names,
   which can break exact-match `IgnoreFunctionsList` entries in
   `ignore_functions_test` and similar tests; expect to look at the test
   output if those start failing.
