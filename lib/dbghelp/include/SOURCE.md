# Source of bundled DbgHelp.h

## Currently bundled file

| Field             | Value                                                                       |
| ----------------- | --------------------------------------------------------------------------- |
| Size on disk      | 109,113 bytes                                                               |
| SHA256            | 868433592662D0F550ED0F48716CF1AC10F974E37E1CF7187E338BE934E92E5B            |
| First-line marker | `/*++ BUILD Version: 0000     Increment this if a change has global effects`|

Get the stamped header hash at any time with:

```powershell
Get-FileHash lib\dbghelp\include\DbgHelp.h -Algorithm SHA256
```

## Exact source

This header was inherited verbatim from the upstream **VLD 2.5.15** release
commit `4d6e7da` and has not been refreshed in this repository since. It
does NOT match any header from the currently-installed Windows 10 SDK
version 10.0.26100.0:

    C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\um\DbgHelp.h
        size  = 124,135 bytes
        hash  = 55EC5FFEA6C2A099316DF485CD054C46E48FFC7B016F81158F8B5D6FED6D161C
        DOES NOT MATCH

The bundled file is therefore from an older Windows SDK release (the exact
version was not recorded by upstream; the commit message of `4d6e7da` is
just "Release v2.5.15"). The header pins structure layouts that the static
assertion near the top of `src/vld.cpp` checks at compile time:

    static char dbghelp32_assert[sizeof(IMAGEHLP_MODULE64) == 3264 ? 1 : -1];

If this header is replaced with a newer SDK version that changes
`IMAGEHLP_MODULE64`, that assertion will fire and the build will fail until
the bundled `dbghelp.dll` redistributables under `setup/dbghelp/` are
updated in step.

## How to update

1. Install or update **Windows 10 SDK** ("Debugging Tools for Windows"
   optional component selected). Record the SDK version, eg `10.0.26100.0`.
2. Copy the new header from one of:

        C:\Program Files (x86)\Windows Kits\10\Include\<sdk-version>\um\DbgHelp.h
        C:\Program Files (x86)\Windows Kits\10\Debuggers\inc\DbgHelp.h
            (some SDK versions only ship the header under Debuggers\inc\)

   into this folder, replacing the existing file.
3. Update the "Currently bundled file" table above with the new size and
   SHA256.
4. Update each bundled `dbghelp.dll` in `setup/dbghelp/` and the link-time
   copies in `lib/dbghelp/lib/` so they stay in sync with this header.
5. Update the static assertion in `src/vld.cpp` (`dbghelp32_assert`) if
   `sizeof(IMAGEHLP_MODULE64)` changed.
6. Rebuild and re-run the full test suite before committing.
