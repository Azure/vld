// chaos_hook_heap_ops.cpp - a report hook that reallocates, destroys and
// creates heaps and reports again while VLD walks its maps.
//
// The hook runs with g_heapMapLock held in the middle of the walk. It moves a
// tracked block with realloc, destroys a private heap that still holds
// blocks, creates a new heap right away (which sometimes gets the destroyed
// heap's handle) and allocates from it, and nests a second report.
// Afterwards every live block must be counted exactly once. The scenario
// repeats so that the handle reuse happens in most runs.

#include "chaos_common.h"
#include "vld.h"

namespace
{
volatile LONG g_inHook = 0;
volatile LONG g_calls = 0;
void* volatile g_moving = NULL;
HANDLE volatile g_doomedHeap = NULL;
HANDLE volatile g_newHeap = NULL;
void* volatile g_newHeapBlock = NULL;
bool g_reused = false;

int __cdecl HeapOpsHook(int, wchar_t*, int*)
{
    if (InterlockedExchange(&g_inHook, 1) != 0)
    {
        return 1;
    }
    switch (InterlockedIncrement(&g_calls))
    {
    case 2:
        g_moving = realloc(g_moving, 64 * 1024);
        break;
    case 4:
    {
        HANDLE doomed = g_doomedHeap;
        HeapDestroy(doomed);
        g_doomedHeap = NULL;
        g_newHeap = HeapCreate(0, 0, 0);
        g_reused = (g_newHeap == doomed);
        break;
    }
    case 6:
        g_newHeapBlock = HeapAlloc(g_newHeap, 0, 123);
        break;
    case 8:
        VLDReportLeaks();
        break;
    case 10:
        VLDGetLeaksCount();
        break;
    default:
        break;
    }
    InterlockedExchange(&g_inHook, 0);
    return 1;
}

void Round(SIZE_T base)
{
    InterlockedExchange(&g_calls, 0);
    g_newHeap = NULL;
    g_newHeapBlock = NULL;
    void* keep[8];
    for (int i = 0; i < 8; i++)
    {
        keep[i] = malloc(24 + i);
    }
    g_moving = malloc(16);
    g_doomedHeap = HeapCreate(0, 0, 0);
    chaos::Check(g_doomedHeap != NULL, "HeapCreate failed");
    for (int i = 0; i < 6; i++)
    {
        HeapAlloc(g_doomedHeap, 0, 40 + i);
    }
    chaos::Check(VLDGetLeaksCount() == base + 15, "setup count is wrong");

    chaos::SetPhase("leak report with a hook that moves blocks and destroys and creates heaps");
    VLDSetReportHook(VLD_RPTHOOK_INSTALL, HeapOpsHook);
    VLDReportLeaks();
    VLDReportLeaks();
    VLDSetReportHook(VLD_RPTHOOK_REMOVE, HeapOpsHook);
    chaos::Check(InterlockedCompareExchange(&g_calls, 0, 0) >= 10, "the hook ran too few times");
    chaos::Check(g_doomedHeap == NULL, "the hook did not destroy the heap");
    chaos::Check(g_newHeapBlock != NULL, "the hook did not allocate from the new heap");

    chaos::SetPhase(g_reused ? "counting after the reports (heap handle reused)" : "counting after the reports");
    chaos::Check(VLDGetLeaksCount() == base + 10, "wrong leak count after the hook's heap operations");
    HeapFree(g_newHeap, 0, g_newHeapBlock);
    chaos::Check(VLDGetLeaksCount() == base + 9, "the new heap's block is not tracked");
    HeapDestroy(g_newHeap);
    free(g_moving);
    for (int i = 0; i < 8; i++)
    {
        free(keep[i]);
    }
    chaos::Check(VLDGetLeaksCount() == base, "leaks left after freeing everything");
}
}

int main()
{
    chaos::StartWatchdog(20000);
    SIZE_T base = VLDGetLeaksCount();
    int reused = 0;
    for (int round = 0; round < 25; round++)
    {
        Round(base);
        reused += g_reused ? 1 : 0;
    }
    char line[64];
    _snprintf_s(line, _TRUNCATE, "heap handle reused in %d of 25 rounds\n", reused);
    chaos::Write(line);
    chaos::Pass();
    return 0;
}