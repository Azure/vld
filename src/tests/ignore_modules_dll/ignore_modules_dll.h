// ignore_modules_dll.h - A simple DLL that intentionally leaks memory.
// Used by ignore_modules_test to verify IgnoreModulesList functionality.

#pragma once

#ifdef IGNORE_MODULES_DLL_EXPORTS
#define IGNORE_MODULES_DLL_API __declspec(dllexport)
#else
#define IGNORE_MODULES_DLL_API __declspec(dllimport)
#endif

extern "C" {

// Leaks exactly 1 allocation of 64 bytes.
IGNORE_MODULES_DLL_API void LeakFromDll(void);

}
