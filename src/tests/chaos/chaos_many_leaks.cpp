// chaos_many_leaks.cpp - reporting many leaks must finish in bounded time.
//
// Leaks tens of thousands of blocks from a few call sites, with and without
// duplicate aggregation, and reports, counts and resolves them, each under
// the watchdog. Quadratic work in the report path shows up as a hang.

#include "chaos_common.h"
#include "vld.h"
#include <stdlib.h>

namespace
{
const int LEAKS = 20000;
void* g_blocks[LEAKS];

int __cdecl SwallowHook(int, wchar_t*, int*)
{
    return 1;
}

__declspec(noinline) void* SiteA(size_t size) { return malloc(size); }
__declspec(noinline) void* SiteB(size_t size) { return malloc(size); }
__declspec(noinline) void* SiteC(size_t size) { return malloc(size); }
}

int main()
{
    chaos::StartWatchdog(28000);
    VLDSetReportHook(VLD_RPTHOOK_INSTALL, SwallowHook);
    UINT options = VLDGetOptions();
    VLDMarkAllLeaksAsReported();

    chaos::SetPhase("leaking");
    for (int i = 0; i < LEAKS; i++)
    {
        switch (i % 3)
        {
        case 0: g_blocks[i] = SiteA(24); break;
        case 1: g_blocks[i] = SiteB(40); break;
        default: g_blocks[i] = SiteC(24 + (i % 5) * 8); break;
        }
        chaos::Check(g_blocks[i] != NULL, "malloc failed");
    }

    chaos::SetPhase("counting");
    chaos::Check(VLDGetLeaksCount() == LEAKS, "VLDGetLeaksCount is wrong");

    chaos::SetPhase("reporting with aggregation");
    VLDSetOptions(options | VLD_OPT_AGGREGATE_DUPLICATES, 16, 16);
    chaos::Check(VLDReportLeaks() == LEAKS, "VLDReportLeaks with aggregation is wrong");

    chaos::SetPhase("resolving");
    VLDResolveCallstacks();

    chaos::SetPhase("reporting without aggregation");
    VLDSetOptions(options & ~VLD_OPT_AGGREGATE_DUPLICATES, 0, 16);
    chaos::Check(VLDReportLeaks() == LEAKS, "VLDReportLeaks without aggregation is wrong");

    chaos::SetPhase("freeing");
    for (int i = 0; i < LEAKS; i++)
    {
        free(g_blocks[i]);
    }
    chaos::Check(VLDGetLeaksCount() == 0, "freed blocks are still counted");

    VLDSetOptions(options, 256, 64);
    VLDSetReportHook(VLD_RPTHOOK_REMOVE, SwallowHook);
    chaos::Pass();
    return 0;
}
