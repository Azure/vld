// dllmain.cpp : Defines the entry point for the DLL application.
#include "stdafx.h"
#include <stdlib.h>
#include <assert.h>

#include <string>

// Here we static initialize a string within the DLL
// Previous versions of VLD would flag this as a leak, make sure it does not
static std::string my_string("xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");

BOOL APIENTRY DllMain(HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved
    )
{
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH: {
        void* leak = malloc(9);
        break;
    }
    case DLL_THREAD_ATTACH: {
        break;
    }
    case DLL_THREAD_DETACH: {
        break;
    }
    case DLL_PROCESS_DETACH: {
        //assert(1 == VLDGetThreadLeaksCount(GetCurrentThreadId()));
        break;
    }
    }
    return TRUE;
}

