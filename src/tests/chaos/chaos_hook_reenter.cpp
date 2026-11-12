// chaos_hook_reenter.cpp - a report hook that allocates, frees and queries VLD
// while VLD is walking its block maps to print the report.
//
// Report hooks run on the reporting thread with g_heapMapLock held, in the
// middle of the walk over a heap's blocks. A hook that formats a string
// allocates and frees (inserting into and erasing from the map being
// walked); one that frees a tracked block erases another entry. Neither may
// crash VLD or corrupt its counts.

#include "chaos_common.h"
#include <string>
#include "vld.h"

namespace
{
void* volatile g_other = NULL;
volatile LONG g_inHook = 0;
volatile LONG g_calls = 0;

int __cdecl ReenterHook(int, wchar_t* message, int*)
{
    if (InterlockedExchange(&g_inHook, 1) != 0)
    {
        return 1;
    }
    InterlockedIncrement(&g_calls);
    std::wstring copy(message);
    copy += L" (seen by the hook)";
    void* scratch = malloc(copy.size() * sizeof(wchar_t));
    free(scratch);
    void* other = InterlockedExchangePointer((PVOID volatile*)&g_other, NULL);
    if (other != NULL)
    {
        free(other);
        VLDGetLeaksCount();
    }
    InterlockedExchange(&g_inHook, 0);
    return 1;
}
}

int main()
{
    chaos::StartWatchdog(20000);
    SIZE_T base = VLDGetLeaksCount();

    void* keep[16];
    for (int i = 0; i < 16; i++)
    {
        keep[i] = malloc(40 + i);
    }
    g_other = malloc(77);

    chaos::SetPhase("leak report with an allocating, freeing, querying hook");
    VLDSetReportHook(VLD_RPTHOOK_INSTALL, ReenterHook);
    VLDReportLeaks();
    VLDReportLeaks();
    VLDSetReportHook(VLD_RPTHOOK_REMOVE, ReenterHook);
    chaos::Check(InterlockedCompareExchange(&g_calls, 0, 0) > 0, "the hook never ran");
    chaos::Check(g_other == NULL, "the hook did not free the extra block");

    chaos::SetPhase("counting after the reports");
    chaos::Check(VLDGetLeaksCount() == base + 16, "wrong leak count after the hook freed a block");
    for (int i = 0; i < 16; i++)
    {
        free(keep[i]);
    }
    chaos::Check(VLDGetLeaksCount() == base, "leaks left after freeing everything");
    chaos::Pass();
    return 0;
}
