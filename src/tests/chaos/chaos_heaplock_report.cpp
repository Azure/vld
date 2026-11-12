// chaos_heaplock_report.cpp - leak reports that allocate from the process heap
// while other threads hold HeapLock and allocate.
//
// VLD sends every report line to the report hooks and the debugger while it
// walks its maps. A report hook may allocate, and OutputDebugStringW converts
// on the process heap. Lockers hold HeapLock on the process heap and allocate
// from it, so VLD must never print or call a hook while a hooked allocation
// can wait for it.

#include "chaos_common.h"
#include "vld.h"

namespace
{
const DWORD PHASE_MS = 2000;
volatile LONG g_stop = 0;
volatile LONG g_leaked = 0;
volatile LONG g_hookCalls = 0;

int __cdecl AllocatingHook(int, wchar_t* message, int* returnValue)
{
    InterlockedIncrement(&g_hookCalls);
    size_t length = wcslen(message) + 1;
    wchar_t* copy = (wchar_t*)malloc(length * sizeof(wchar_t));
    if (copy != NULL)
    {
        wcscpy_s(copy, length, message);
        free(copy);
    }
    void* block = HeapAlloc(GetProcessHeap(), 0, 64);
    HeapFree(GetProcessHeap(), 0, block);
    *returnValue = 0;
    return 1;
}

DWORD WINAPI Locker(LPVOID p)
{
    chaos::Random random((unsigned)(INT_PTR)p * 59 + 3);
    HANDLE heap = GetProcessHeap();
    while (!InterlockedCompareExchange(&g_stop, 0, 0))
    {
        chaos::Check(HeapLock(heap) != FALSE, "HeapLock failed");
        void* a = HeapAlloc(heap, 0, 16 + random.Below(300));
        void* b = HeapAlloc(heap, 0, 16 + random.Below(300));
        HeapFree(heap, 0, a);
        if (random.Below(16) == 0)
            InterlockedIncrement(&g_leaked);
        else
            HeapFree(heap, 0, b);
        HeapUnlock(heap);
    }
    return 0;
}

DWORD WINAPI Reporter(LPVOID)
{
    while (!InterlockedCompareExchange(&g_stop, 0, 0))
    {
        VLDReportLeaks();
    }
    return 0;
}

void RunPhase(const char* phase)
{
    chaos::SetPhase(phase);
    InterlockedExchange(&g_stop, 0);
    HANDLE threads[5];
    for (int i = 0; i < 5; i++)
    {
        threads[i] = CreateThread(NULL, 0, i < 4 ? Locker : Reporter, (LPVOID)(INT_PTR)i, 0, NULL);
        chaos::Check(threads[i] != NULL, "CreateThread failed");
    }
    Sleep(PHASE_MS);
    InterlockedExchange(&g_stop, 1);
    WaitForMultipleObjects(5, threads, TRUE, INFINITE);
    for (int i = 0; i < 5; i++)
    {
        CloseHandle(threads[i]);
    }
}
}

int main()
{
    chaos::StartWatchdog(25000);
    UINT options = VLDGetOptions();
    VLDMarkAllLeaksAsReported();

    VLDSetReportOptions(VLD_OPT_REPORT_TO_DEBUGGER, NULL);
    RunPhase("reporting to the debugger while lockers allocate under HeapLock");

    VLDSetReportHook(VLD_RPTHOOK_INSTALL, AllocatingHook);
    RunPhase("an allocating report hook while lockers allocate under HeapLock");
    chaos::Check(g_hookCalls > 0, "the report hook was never called");

    chaos::SetPhase("counting");
    UINT counted = VLDGetLeaksCount();
    if (counted != (UINT)g_leaked)
    {
        char message[96];
        sprintf_s(message, "leaked %ld blocks but VLD counts %u", g_leaked, counted);
        chaos::Fail(message);
    }

    chaos::SetPhase("cleanup");
    VLDMarkAllLeaksAsReported();
    VLDSetReportHook(VLD_RPTHOOK_REMOVE, AllocatingHook);
    VLDSetReportOptions(VLD_OPT_REPORT_TO_STDOUT, NULL);
    VLDSetOptions(options, 256, 64);
    chaos::Pass();
    return 0;
}
