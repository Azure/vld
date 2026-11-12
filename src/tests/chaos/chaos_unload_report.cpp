// chaos_unload_report.cpp - reporting and resolving leaks whose call stacks
// run through a DLL that other threads keep unloading and reloading.
//
// Loaders leak blocks from chaos_helper_dll and unload it, so the call stacks
// point into code that is gone, or into a newer copy of the DLL at the same
// or another address. Reporters report, count and resolve the whole time.
// Afterwards every leaked block must still be counted.

#include "chaos_common.h"
#include "vld.h"

namespace
{
const DWORD RUN_MS = 3000;
const int LOADERS = 4;
const int REPORTERS = 3;
const int MAX_LEAKS = 4000;
volatile LONG g_stop = 0;
volatile LONG g_leaked = 0;

typedef void* (*HelperMalloc_t)(size_t);

int __cdecl SwallowHook(int, wchar_t*, int*)
{
    return 1;
}

DWORD WINAPI Loader(LPVOID p)
{
    chaos::Random random((unsigned)(INT_PTR)p * 53 + 11);
    while (!InterlockedCompareExchange(&g_stop, 0, 0))
    {
        HMODULE helper = LoadLibraryW(L"chaos_helper_dll.dll");
        chaos::Check(helper != NULL, "LoadLibrary(chaos_helper_dll) failed");
        HelperMalloc_t helperMalloc = (HelperMalloc_t)GetProcAddress(helper, "HelperMalloc");
        chaos::Check(helperMalloc != NULL, "HelperMalloc not found");
        int count = 1 + random.Below(4);
        for (int i = 0; i < count; i++)
        {
            if (InterlockedIncrement(&g_leaked) > MAX_LEAKS)
            {
                InterlockedDecrement(&g_leaked);
                break;
            }
            chaos::Check(helperMalloc(16 + random.Below(200)) != NULL, "HelperMalloc failed");
        }
        FreeLibrary(helper);
    }
    return 0;
}

DWORD WINAPI Reporter(LPVOID p)
{
    chaos::Random random((unsigned)(INT_PTR)p * 29 + 5);
    while (!InterlockedCompareExchange(&g_stop, 0, 0))
    {
        switch (random.Below(4))
        {
        case 0:
            VLDReportLeaks();
            break;
        case 1:
            VLDGetLeaksCount();
            break;
        case 2:
            VLDResolveCallstacks();
            break;
        default:
            VLDRefreshModules();
            break;
        }
    }
    return 0;
}
}

int main()
{
    chaos::StartWatchdog(25000);
    VLDSetReportHook(VLD_RPTHOOK_INSTALL, SwallowHook);
    VLDMarkAllLeaksAsReported();

    chaos::SetPhase("unloading while reporting");
    HANDLE threads[LOADERS + REPORTERS];
    for (int i = 0; i < LOADERS + REPORTERS; i++)
    {
        threads[i] = CreateThread(NULL, 0, i < LOADERS ? Loader : Reporter, (LPVOID)(INT_PTR)i, 0, NULL);
        chaos::Check(threads[i] != NULL, "CreateThread failed");
    }
    Sleep(RUN_MS);
    InterlockedExchange(&g_stop, 1);
    WaitForMultipleObjects(LOADERS + REPORTERS, threads, TRUE, INFINITE);

    chaos::SetPhase("counting leaks from the unloaded DLL");
    UINT counted = VLDGetLeaksCount();
    if (counted != (UINT)g_leaked)
    {
        char message[96];
        sprintf_s(message, "leaked %ld blocks but VLD counts %u", g_leaked, counted);
        chaos::Fail(message);
    }
    chaos::Check(VLDReportLeaks() == counted, "VLDReportLeaks disagrees with VLDGetLeaksCount");

    chaos::SetPhase("cleanup");
    VLDMarkAllLeaksAsReported();
    for (int i = 0; i < LOADERS + REPORTERS; i++)
    {
        CloseHandle(threads[i]);
    }
    VLDSetReportHook(VLD_RPTHOOK_REMOVE, SwallowHook);
    chaos::Pass();
    return 0;
}
