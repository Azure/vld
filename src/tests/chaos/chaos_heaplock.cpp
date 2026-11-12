// chaos_heaplock.cpp - an application holding HeapLock while it allocates.
//
// Lockers hold HeapLock on the process heap and allocate from it, so VLD's
// hooks run with the heap locked. Reporters make VLD resolve symbols and
// walk stacks, where dbghelp allocates from the process heap. With the safe
// stack walker the hooks take g_DbgHelp, so this checks that VLD never
// waits for g_DbgHelp while a dbghelp caller waits for the heap. Only
// HeapAlloc runs under the lock: the debug CRT's malloc takes the CRT heap
// lock before the process heap lock, so malloc there deadlocks without VLD.

#include "chaos_common.h"
#include "vld.h"

namespace
{
const DWORD RUN_MS = 3000;
volatile LONG g_stop = 0;
volatile LONG g_leaked = 0;

int __cdecl SwallowHook(int, wchar_t*, int*)
{
    return 1;
}

DWORD WINAPI Locker(LPVOID p)
{
    chaos::Random random((unsigned)(INT_PTR)p * 41 + 9);
    HANDLE heap = GetProcessHeap();
    while (!InterlockedCompareExchange(&g_stop, 0, 0))
    {
        chaos::Check(HeapLock(heap) != FALSE, "HeapLock failed");
        void* a = HeapAlloc(heap, 0, 16 + random.Below(300));
        void* b = HeapAlloc(heap, 0, 16 + random.Below(300));
        HeapFree(heap, 0, a);
        if (random.Below(8) == 0)
            InterlockedIncrement(&g_leaked);
        else
            HeapFree(heap, 0, b);
        HeapUnlock(heap);
    }
    return 0;
}

DWORD WINAPI Reporter(LPVOID p)
{
    chaos::Random random((unsigned)(INT_PTR)p * 23 + 1);
    while (!InterlockedCompareExchange(&g_stop, 0, 0))
    {
        switch (random.Below(3))
        {
        case 0: VLDReportLeaks(); break;
        case 1: VLDResolveCallstacks(); break;
        default: VLDRefreshModules(); break;
        }
    }
    return 0;
}
}

int main()
{
    chaos::StartWatchdog(25000);
    VLDSetReportHook(VLD_RPTHOOK_INSTALL, SwallowHook);
    UINT options = VLDGetOptions();
    VLDSetOptions(options | VLD_OPT_SAFE_STACK_WALK, 256, 64);
    VLDMarkAllLeaksAsReported();

    chaos::SetPhase("allocating under HeapLock while reporting");
    HANDLE threads[6];
    for (int i = 0; i < 6; i++)
    {
        threads[i] = CreateThread(NULL, 0, i < 4 ? Locker : Reporter, (LPVOID)(INT_PTR)i, 0, NULL);
        chaos::Check(threads[i] != NULL, "CreateThread failed");
    }
    Sleep(RUN_MS);
    InterlockedExchange(&g_stop, 1);
    WaitForMultipleObjects(6, threads, TRUE, INFINITE);

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
    VLDSetOptions(options, 256, 64);
    VLDSetReportHook(VLD_RPTHOOK_REMOVE, SwallowHook);
    for (int i = 0; i < 6; i++)
    {
        CloseHandle(threads[i]);
    }
    chaos::Pass();
    return 0;
}
