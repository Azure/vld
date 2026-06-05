# Source of bundled x86 DbgHelp.Lib

## Current contents

| File         | Notes                                                |
| ------------ | ---------------------------------------------------- |
| DbgHelp.Lib  | x86 import library for dbghelp.dll.                  |

## Provenance

The import library is taken from the Microsoft "Debugging Tools for Windows"
component of the Windows 10 SDK (Windows Kits 10), Debuggers package, x86
flavor. On a machine where the Debuggers component installs the x86 lib (some
SDK versions ship it under `lib/x86/` of the Debuggers tree; other versions
omit it and the linker uses the system SDK `um/x86/DbgHelp.lib` instead), it
can be located at one of:

    C:\Program Files (x86)\Windows Kits\10\Debuggers\lib\x86\DbgHelp.Lib
    C:\Program Files (x86)\Windows Kits\10\Lib\<sdk-version>\um\x86\DbgHelp.lib

VLD bundles it so that the build is self-contained and the linker does not
have to discover an SDK install path. The runtime DLL it pairs with lives in
`setup/dbghelp/x86/dbghelp.dll`.

## How to update

1. Install or update the Windows SDK Debuggers component.
2. Copy the new x86 `DbgHelp.Lib` from the path above into this folder.
3. Update `setup/dbghelp/x86/dbghelp.dll` if you bumped the SDK version so the
   import library and runtime DLL stay in sync.
4. Rebuild and re-run the full test suite before committing.
