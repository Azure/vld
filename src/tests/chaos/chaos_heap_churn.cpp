// chaos_heap_churn.cpp - private heaps created and destroyed with live blocks on
// several threads while other threads count leaks.
//
// VLD maps every heap it sees created and reports and unmaps its blocks when
// the heap is destroyed. Destroying heaps that still hold blocks, from many
// threads at once, must not hang, crash or leave stale blocks counted.

#include "chaos_common.h"
#include "vld.h"

namespace
{
const DWORD RUN_MS = 3000;
volatile LONG g_stop = 0;
volatile LONG g_heapsDestroyed = 0;

int __cdecl SwallowReport(int, wchar_t*, int*)
{
    return 1;
}

DWORD WINAPI HeapWorker(LPVOID p)
{
    chaos::Random random((unsigned)(INT_PTR)p * 31 + 7);
    while (!InterlockedCompareExchange(&g_stop, 0, 0))
    {
        HANDLE heap = HeapCreate(random.Below(2) ? HEAP_NO_SERIALIZE : 0, 0, 0);
        chaos::Check(heap != NULL, "HeapCreate failed");
        int blocks = 1 + random.Below(40);
        void* last = NULL;
        for (int i = 0; i < blocks; i++)
        {
            last = HeapAlloc(heap, 0, 8 + random.Below(512));
            chaos::Check(last != NULL, "HeapAlloc failed");
            if (random.Below(3) == 0)
            {
                last = HeapReAlloc(heap, 0, last, 16 + random.Below(1024));
                chaos::Check(last != NULL, "HeapReAlloc failed");
            }
        }
        if (random.Below(2))
        {
            HeapFree(heap, 0, last);
        }
        chaos::Check(HeapDestroy(heap) != FALSE, "HeapDestroy failed");
        InterlockedIncrement(&g_heapsDestroyed);
    }
    return 0;
}

DWORD WINAPI Counter(LPVOID)
{
    while (!InterlockedCompareExchange(&g_stop, 0, 0))
    {
        VLDGetLeaksCount();
    }
    return 0;
}

DWORD WINAPI Worker(LPVOID p)
{
    INT_PTR index = (INT_PTR)p;
    if (index < 6)
    {
        return HeapWorker(p);
    }
    if (index < 8)
    {
        return Counter(p);
    }
    Sleep(RUN_MS);
    InterlockedExchange(&g_stop, 1);
    return 0;
}
}

int main()
{
    chaos::StartWatchdog(25000);
    SIZE_T base = VLDGetLeaksCount();
    // Destroyed heaps report their remaining blocks by design.
    VLDSetReportHook(VLD_RPTHOOK_INSTALL, SwallowReport);

    chaos::SetPhase("heap create/alloc/destroy churn with leak counts");
    chaos::RunThreads(9, Worker);

    VLDSetReportHook(VLD_RPTHOOK_REMOVE, SwallowReport);
    chaos::SetPhase("final count");
    chaos::Check(InterlockedCompareExchange(&g_heapsDestroyed, 0, 0) > 0, "no heap was destroyed");
    chaos::Check(VLDGetLeaksCount() == base, "blocks of destroyed heaps are still counted");
    chaos::Pass();
    return 0;
}
