// chaos_helper_dll.cpp - helper DLL shared by the VLD chaos tests.
//
// Links VLD so that its allocations are tracked. Exposes allocation entry
// points whose call stacks run through this DLL, and optional allocation
// activity in DLL_THREAD_ATTACH/DETACH (which run under the loader lock).

#include <windows.h>
#include <stdlib.h>
#include "vld.h"

static volatile LONG g_armed = 0;
static volatile LONG g_inThreadAttachAlloc = 0;
static HANDLE g_inThreadAttach = NULL;
static volatile LONG g_threadChurn = 0;
static volatile LONG g_threadChurnLive = 0;
static DWORD g_tlsIndex = TLS_OUT_OF_INDEXES;

#pragma optimize("", off)
extern "C" __declspec(dllexport) __declspec(noinline) void* HelperMalloc(size_t size)
{
    return malloc(size);
}

extern "C" __declspec(dllexport) __declspec(noinline) void HelperFree(void* p)
{
    free(p);
}

extern "C" __declspec(dllexport) __declspec(noinline) void* HelperHeapAlloc(HANDLE heap, size_t size)
{
    return HeapAlloc(heap, 0, size);
}
#pragma optimize("", on)

// The next DLL_THREAD_ATTACH signals inThreadAttach, then allocates while the
// new thread holds the loader lock.
extern "C" __declspec(dllexport) void ArmThreadAttach(HANDLE inThreadAttach)
{
    g_inThreadAttach = inThreadAttach;
    InterlockedExchange(&g_armed, 1);
}

extern "C" __declspec(dllexport) LONG IsInThreadAttachAlloc()
{
    return InterlockedCompareExchange(&g_inThreadAttachAlloc, 0, 0);
}

// While enabled, every thread allocates in DLL_THREAD_ATTACH and frees in
// DLL_THREAD_DETACH.
extern "C" __declspec(dllexport) void SetThreadChurn(LONG enabled)
{
    InterlockedExchange(&g_threadChurn, enabled);
}

// Blocks allocated in thread attach whose threads have not detached yet.
extern "C" __declspec(dllexport) LONG ThreadChurnLiveBlocks()
{
    return InterlockedCompareExchange(&g_threadChurnLive, 0, 0);
}

BOOL APIENTRY DllMain(HMODULE, DWORD reason, LPVOID)
{
    switch (reason)
    {
    case DLL_PROCESS_ATTACH:
        g_tlsIndex = TlsAlloc();
        break;
    case DLL_PROCESS_DETACH:
        if (g_tlsIndex != TLS_OUT_OF_INDEXES)
        {
            TlsFree(g_tlsIndex);
        }
        break;
    case DLL_THREAD_ATTACH:
        if (InterlockedExchange(&g_armed, 0) != 0)
        {
            InterlockedExchange(&g_inThreadAttachAlloc, 1);
            SetEvent(g_inThreadAttach);
            void* p = malloc(16);
            free(p);
            InterlockedExchange(&g_inThreadAttachAlloc, 0);
        }
        if (InterlockedCompareExchange(&g_threadChurn, 0, 0) != 0 && g_tlsIndex != TLS_OUT_OF_INDEXES)
        {
            void* block = malloc(48);
            if (block != NULL)
            {
                InterlockedIncrement(&g_threadChurnLive);
            }
            TlsSetValue(g_tlsIndex, block);
        }
        break;
    case DLL_THREAD_DETACH:
        if (g_tlsIndex != TLS_OUT_OF_INDEXES)
        {
            void* block = TlsGetValue(g_tlsIndex);
            if (block != NULL)
            {
                free(block);
                InterlockedDecrement(&g_threadChurnLive);
            }
            TlsSetValue(g_tlsIndex, NULL);
        }
        break;
    }
    return TRUE;
}
