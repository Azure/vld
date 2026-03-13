// ignore_modules_dll.cpp - A simple DLL that intentionally leaks memory.
// Used by ignore_modules_test to verify IgnoreModulesList functionality.

#include <stdlib.h>
#include "ignore_modules_dll.h"

extern "C" {

static volatile void* g_leaked = NULL;

void LeakFromDll(void)
{
    // Intentionally leak 64 bytes. The allocation originates from this
    // module (ignore_modules_dll.dll), so when the module is added to
    // IgnoreModulesList, VLD should not report this leak.
    g_leaked = malloc(64);
}

}
