// review_symbol_cache_latch.cpp - PR #74 review comment on callstack.cpp:374.
//
// A leak query that finds a frame missing from the symbol cache while it holds
// g_heapMapLock gets the frame's address instead of its name. That must not
// decide anything for good. Here a thread that loads and unloads a DLL empties
// the cache while a query looks at fresh leaks of a function listed in
// IgnoreFunctionsList; once the unloads stop, none of them may count.

#include "review_common.h"
#include "vld.h"

namespace
{
const int ROUNDS = 4;
const int LEAKS_PER_ROUND = 25000;

volatile LONG g_stop = 0;
volatile LONG g_unloads = 0;

#pragma optimize("", off)
extern "C" __declspec(noinline) void* ReviewIgnoredLeaker()
{
    void* block = HeapAlloc(GetProcessHeap(), 0, 16);
    return block;
}
#pragma optimize("", on)

DWORD WINAPI Churn(LPVOID)
{
    while (InterlockedCompareExchange(&g_stop, 0, 0) == 0)
    {
        HMODULE module = LoadLibraryW(L"msimg32.dll");
        if (module != NULL)
            FreeLibrary(module);
        InterlockedIncrement(&g_unloads);
    }
    return 0;
}
}

int main()
{
    chaos::StartWatchdog(120000);
    VLDMarkAllLeaksAsReported();
    chaos::Check(GetModuleHandleW(L"msimg32.dll") == NULL, "msimg32.dll is already loaded; pick another DLL");

    // A query marks an ignored leak as reported the first time it looks at
    // it, so every round leaks blocks no query has seen yet, then queries them
    // once while the DLL loads and unloads.
    for (int round = 0; round < ROUNDS; round++)
    {
        chaos::SetPhase("leaking from an ignored function");
        for (int i = 0; i < LEAKS_PER_ROUND; i++)
        {
            chaos::Check(ReviewIgnoredLeaker() != NULL, "HeapAlloc failed");
        }

        chaos::SetPhase("querying while a DLL loads and unloads");
        InterlockedExchange(&g_stop, 0);
        LONG unloads = InterlockedCompareExchange(&g_unloads, 0, 0);
        HANDLE churn = CreateThread(NULL, 0, Churn, NULL, 0, NULL);
        chaos::Check(churn != NULL, "CreateThread failed");
        while (InterlockedCompareExchange(&g_unloads, 0, 0) < unloads + 2)
        {
            Sleep(1);
        }
        VLDGetLeaksCount();
        InterlockedExchange(&g_stop, 1);
        WaitForSingleObject(churn, INFINITE);
        CloseHandle(churn);
    }

    chaos::SetPhase("querying after the unloads stopped");
    char message[512];
    UINT leaks = VLDGetLeaksCount();
    sprintf_s(message, "%u leaks of the ignored function are counted after %d queries raced %ld unloads",
        leaks, ROUNDS, g_unloads);
    chaos::Check(leaks == 0, message);

    VLDMarkAllLeaksAsReported();
    chaos::Pass();
    return 0;
}
