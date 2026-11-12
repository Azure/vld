// chaos_api_storm.cpp - every VLD control API at once, concurrently with
// allocation traffic.
//
// Threads toggle VLDDisable/VLDEnable/VLDRestore, VLDGlobalDisable/Enable,
// change options, mark leaks as reported, count and resolve call stacks
// while others allocate, reallocate and free. Afterwards VLD must still
// track allocations exactly.

#include "chaos_common.h"
#include "vld.h"

namespace
{
const DWORD RUN_MS = 3000;
volatile LONG g_stop = 0;

DWORD WINAPI Allocator(LPVOID p)
{
    chaos::Random random((unsigned)(INT_PTR)p * 97 + 3);
    void* slots[32] = { 0 };
    while (!InterlockedCompareExchange(&g_stop, 0, 0))
    {
        unsigned i = random.Below(32);
        switch (random.Below(3))
        {
        case 0:
            free(slots[i]);
            slots[i] = malloc(1 + random.Below(300));
            break;
        case 1:
            slots[i] = realloc(slots[i], 1 + random.Below(3000));
            break;
        default:
            free(slots[i]);
            slots[i] = NULL;
            break;
        }
    }
    for (int i = 0; i < 32; i++)
    {
        free(slots[i]);
    }
    return 0;
}

DWORD WINAPI Toggler(LPVOID p)
{
    chaos::Random random((unsigned)(INT_PTR)p * 13 + 1);
    UINT options = VLDGetOptions();
    while (!InterlockedCompareExchange(&g_stop, 0, 0))
    {
        switch (random.Below(8))
        {
        case 0:
            VLDDisable();
            free(malloc(8));
            VLDRestore();
            break;
        case 1:
            VLDEnable();
            break;
        case 2:
            VLDGlobalDisable();
            VLDGlobalEnable();
            break;
        case 3:
            VLDSetOptions(VLD_OPT_SAFE_STACK_WALK, 256, 32);
            VLDSetOptions(options, 256, 64);
            break;
        case 4:
            VLDGetLeaksCount();
            break;
        case 5:
            VLDResolveCallstacks();
            break;
        case 6:
            VLDGetThreadLeaksCount(GetCurrentThreadId());
            break;
        default:
            VLDRefreshModules();
            break;
        }
    }
    return 0;
}

DWORD WINAPI Worker(LPVOID p)
{
    INT_PTR index = (INT_PTR)p;
    if (index < 6)
    {
        return Allocator(p);
    }
    if (index < 9)
    {
        return Toggler(p);
    }
    Sleep(RUN_MS);
    InterlockedExchange(&g_stop, 1);
    return 0;
}
}

int main()
{
    chaos::StartWatchdog(25000);
    UINT options = VLDGetOptions();
    SIZE_T base = VLDGetLeaksCount();

    chaos::SetPhase("API storm with allocation traffic");
    chaos::RunThreads(10, Worker);

    chaos::SetPhase("tracking after the storm");
    VLDGlobalEnable();
    VLDEnable();
    VLDSetOptions(options, 256, 64);
    SIZE_T after = VLDGetLeaksCount();
    chaos::Check(after == base, "the storm left leaks behind");
    void* sentinel = malloc(47);
    chaos::Check(VLDGetLeaksCount() == base + 1, "a new allocation is not tracked after the storm");
    free(sentinel);
    chaos::Check(VLDGetLeaksCount() == base, "a freed allocation is still counted after the storm");
    chaos::Pass();
    return 0;
}
