// chaos_report_hook_race.cpp - report hooks installed and removed while other
// threads print leak reports.
//
// VLDSetReportHook changes the hook set under VLD's options lock; every
// printed report line walks that set. A leak report running concurrently with
// hook installs/removals must neither crash nor lose the permanent hook.

#include "chaos_common.h"
#include "vld.h"

namespace
{
const DWORD RUN_MS = 3000;
volatile LONG g_stop = 0;
volatile LONG g_leakedLines = 0;

template <int N>
int __cdecl Hook(int, wchar_t* message, int*)
{
    if (message != NULL && wcsstr(message, L"Leak Hash") != NULL)
    {
        InterlockedIncrement(&g_leakedLines);
    }
    return 1;
}

int __cdecl PermanentHook(int, wchar_t* message, int*)
{
    if (message != NULL && wcsstr(message, L"Leak Hash") != NULL)
    {
        InterlockedIncrement(&g_leakedLines);
    }
    return 1;
}

VLD_REPORT_HOOK const g_hooks[] = { Hook<0>, Hook<1>, Hook<2>, Hook<3>, Hook<4>, Hook<5>, Hook<6>, Hook<7> };

DWORD WINAPI Churn(LPVOID p)
{
    chaos::Random random((unsigned)(INT_PTR)p * 7919u + 1);
    while (!InterlockedCompareExchange(&g_stop, 0, 0))
    {
        VLD_REPORT_HOOK hook = g_hooks[random.Below(_countof(g_hooks))];
        VLDSetReportHook(VLD_RPTHOOK_INSTALL, hook);
        VLDSetReportHook(VLD_RPTHOOK_REMOVE, hook);
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

DWORD WINAPI Worker(LPVOID p)
{
    INT_PTR index = (INT_PTR)p;
    if (index < 3)
    {
        return Churn(p);
    }
    if (index < 5)
    {
        return Reporter(p);
    }
    Sleep(RUN_MS);
    InterlockedExchange(&g_stop, 1);
    return 0;
}
}

int main()
{
    chaos::StartWatchdog(20000);
    chaos::Check(VLDSetReportHook(VLD_RPTHOOK_INSTALL, PermanentHook) == 0, "install failed");
    void* leaks[4];
    for (int i = 0; i < 4; i++)
    {
        leaks[i] = malloc(32 + i);
    }

    chaos::SetPhase("hook install/remove racing VLDReportLeaks");
    chaos::RunThreads(6, Worker);

    chaos::SetPhase("cleanup");
    for (int i = 0; i < 4; i++)
    {
        free(leaks[i]);
    }
    VLDSetReportHook(VLD_RPTHOOK_REMOVE, PermanentHook);
    chaos::Check(InterlockedCompareExchange(&g_leakedLines, 0, 0) > 0, "no leak report reached the permanent hook");
    chaos::Pass();
    return 0;
}
