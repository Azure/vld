# Source of bundled DbgHelp.h

## Current contents

| File       | Notes                                                          |
| ---------- | -------------------------------------------------------------- |
| DbgHelp.h  | Public header for the Microsoft Image Help (dbghelp) library.  |

## Provenance

The header is taken from the Microsoft "Debugging Tools for Windows" component
of the Windows 10 SDK (Windows Kits 10). On a machine with the Debuggers
component installed, the header is located at:

    C:\Program Files (x86)\Windows Kits\10\Debuggers\inc\DbgHelp.h

VLD bundles it explicitly because the structure layouts it declares
(`IMAGEHLP_MODULE64` in particular) must stay in sync with the bundled
`dbghelp.dll` redistributables under `setup/dbghelp/`. The static assertion
near the top of `src/vld.cpp` that checks `sizeof(IMAGEHLP_MODULE64)` will
break the build if the bundled header and the system one drift apart.

## How to update

1. Install or update the Windows SDK "Debugging Tools for Windows" component.
2. Copy the new header from the path above into this folder, replacing the
   existing file.
3. Update each bundled `dbghelp.dll` in `setup/dbghelp/` and the link-time
   copies in `lib/dbghelp/lib/` so they stay in sync with this header.
4. Update the static assertion in `src/vld.cpp` (`dbghelp32_assert`) to the
   new `sizeof(IMAGEHLP_MODULE64)` if it changed.
5. Rebuild and re-run the full test suite before committing.
