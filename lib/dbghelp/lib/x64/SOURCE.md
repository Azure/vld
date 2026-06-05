# Source of bundled x64 DbgHelp.Lib

## Current contents

| File         | Notes                                                |
| ------------ | ---------------------------------------------------- |
| DbgHelp.Lib  | x64 import library for dbghelp.dll.                  |

## Provenance

The import library is taken from the Microsoft "Debugging Tools for Windows"
component of the Windows 10 SDK (Windows Kits 10), Debuggers package, x64
flavor. On a machine where the Debuggers component installs the x64 lib (some
SDK versions ship it under `lib/x64/` of the Debuggers tree; other versions
omit it and the linker uses the system SDK `um/x64/DbgHelp.lib` instead), it
can be located at one of:

    C:\Program Files (x86)\Windows Kits\10\Debuggers\lib\x64\DbgHelp.Lib
    C:\Program Files (x86)\Windows Kits\10\Lib\<sdk-version>\um\x64\DbgHelp.lib

VLD bundles it so that the build is self-contained and the linker does not
have to discover an SDK install path. The runtime DLL it pairs with lives in
`setup/dbghelp/x64/dbghelp.dll`.

## How to update

1. Install or update the Windows SDK Debuggers component.
2. Copy the new x64 `DbgHelp.Lib` from the path above into this folder.
3. Update `setup/dbghelp/x64/dbghelp.dll` if you bumped the SDK version so the
   import library and runtime DLL stay in sync.
4. Rebuild and re-run the full test suite before committing.
