// dyn_symbol_test.cpp
//
// Regression test for VLD's symbol resolution of DYNAMICALLY LOADED modules.
//
// When a DLL is loaded via LoadLibrary AFTER VLD has started, VLD's
// LdrRegisterDllNotification LOADED callback (VldDllLoadNotification) fires.
// That callback calls RegisterLoadedModule, which invokes SymLoadModuleExW so
// dbghelp can later resolve function names inside that DLL. Without the
// SymLoadModuleExW call, SymFromAddrW returns the address as a string instead
// of the function name and leak callstacks for dynamically loaded DLLs degrade
// to address-only frames.
//
// This test exercises the path: LoadLibrary("dynamic.dll"), GetProcAddress for
// SimpleLeak_Malloc, call it (which leaks 6 allocations from named functions
// inside dynamic.dll), exit. The destructor's leak report MUST show resolved
// function names from dynamic.dll (e.g. "dynamic.dll!LeakMemoryMalloc"), not
// just addresses (e.g. "dynamic.dll!0x000...").
//
// CTest enforces this via:
//   PASS_REGULAR_EXPRESSION = dynamic\.dll!LeakMemoryMalloc
//   FAIL_REGULAR_EXPRESSION = dynamic\.dll!0x[0-9A-Fa-f]+\(

#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include "vld.h"

typedef void (__cdecl *SimpleLeak_Malloc_t)(void);

int main()
{
    HMODULE hdyn = LoadLibrary(_T("dynamic.dll"));
    if (hdyn == NULL) {
        fprintf(stderr, "LoadLibrary(dynamic.dll) failed: %lu\n", GetLastError());
        return 1;
    }
    // Make sure the module is in VLD's tracked set. Without the dbghelp
    // pre-registration in RegisterLoadedModule, this enables VLD tracking but
    // symbol resolution would still be broken.
    VLDEnableModule(hdyn);

    SimpleLeak_Malloc_t func = (SimpleLeak_Malloc_t)GetProcAddress(hdyn, "SimpleLeak_Malloc");
    if (func == NULL) {
        fprintf(stderr, "GetProcAddress(SimpleLeak_Malloc) failed: %lu\n", GetLastError());
        return 2;
    }
    func(); // leaks 6 allocations from dynamic.dll!LeakMemoryMalloc

    printf("dyn_symbol_test: triggered leaks from dynamic.dll; leak report should show resolved frame names\n");
    return 0;
}
