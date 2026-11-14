// chaos_exit_pressure.cpp - the process exits while other threads allocate,
// free, report leaks and resolve call stacks.
//
// ExitProcess terminates the other threads wherever they are, including
// inside VLD's hooks holding its locks, and then VLD reports leaks from its
// DLL_PROCESS_DETACH. The process must exit with the code main returns,
// without crashing or hanging. A report hook swallows the leak report: the
// blocks held by the killed threads are expected leaks.

#include "chaos_common.h"
#include "vld.h"

namespace
{
DWORD WINAPI Churn(LPVOID p)
{
    chaos::Random random((unsigned)(INT_PTR)p * 977 + 5);
    HANDLE processHeap = GetProcessHeap();
    void* slots[64] = { 0 };
    for (;;)
    {
        unsigned i = random.Below(64);
        switch (random.Below(5))
        {
        case 0:
            free(slots[i]);
            slots[i] = malloc(1 + random.Below(400));
            break;
        case 1:
            slots[i] = realloc(slots[i], 1 + random.Below(4000));
            break;
        case 2:
        {
            void* block = HeapAlloc(processHeap, 0, 1 + random.Below(200));
            HeapFree(processHeap, 0, block);
            break;
        }
        case 3:
        {
            HANDLE heap = HeapCreate(0, 0, 0);
            HeapAlloc(heap, 0, 64);
            HeapDestroy(heap);
            break;
        }
        default:
            if (random.Below(50) == 0)
            {
                VLDGetLeaksCount();
            }
            break;
        }
    }
}

int __cdecl SwallowHook(int, wchar_t*, int*)
{
    return 1;
}

DWORD WINAPI Reporter(LPVOID p)
{
    for (;;)
    {
        if (((INT_PTR)p % 2) == 0)
        {
            VLDReportLeaks();
        }
        else
        {
            VLDResolveCallstacks();
        }
    }
}
}

int main()
{
    chaos::StartWatchdog(25000);
    chaos::SetPhase("exit while threads allocate, report and resolve call stacks");
    VLDSetReportHook(VLD_RPTHOOK_INSTALL, SwallowHook);
    for (INT_PTR i = 0; i < 10; i++)
    {
        HANDLE thread = CreateThread(NULL, 0, i < 8 ? Churn : Reporter, (LPVOID)i, 0, NULL);
        chaos::Check(thread != NULL, "CreateThread failed");
        CloseHandle(thread);
    }
    Sleep(400);
    chaos::Write("CHAOS PASS\n");
    return 0;
}
