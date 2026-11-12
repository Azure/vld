// chaos_option_extremes.cpp - extreme option values must not break tracking
// or reporting.
//
// Leaks are made and reported under trace depths of 0, 1 and 65535 frames,
// data dumps of 0 bytes and of more than the block, with the safe and the
// fast stack walker, with and without duplicate aggregation. Counts must be
// exact and reporting must not crash or hang.

#include "chaos_common.h"
#include "vld.h"
#include <stdlib.h>

namespace
{
int __cdecl SwallowHook(int, wchar_t*, int*)
{
    return 1;
}

__declspec(noinline) void* Recurse(int depth, size_t size)
{
    if (depth == 0)
        return malloc(size);
    void* volatile result = Recurse(depth - 1, size);
    return result;
}
}

int main()
{
    chaos::StartWatchdog(25000);
    VLDSetReportHook(VLD_RPTHOOK_INSTALL, SwallowHook);
    UINT options = VLDGetOptions();

    const UINT frames[] = { 0, 1, 2, 64, 65535 };
    const SIZE_T dumps[] = { 0, 1, 4096, (SIZE_T)-1 };
    const UINT flags[] = { 0, VLD_OPT_SAFE_STACK_WALK, VLD_OPT_AGGREGATE_DUPLICATES, VLD_OPT_SAFE_STACK_WALK | VLD_OPT_AGGREGATE_DUPLICATES };

    chaos::SetPhase("leaking and reporting under extreme options");
    for (int f = 0; f < _countof(frames); f++)
    {
        for (int d = 0; d < _countof(dumps); d++)
        {
            for (int o = 0; o < _countof(flags); o++)
            {
                VLDMarkAllLeaksAsReported();
                VLDSetOptions(VLD_OPT_SAFE_STACK_WALK | VLD_OPT_AGGREGATE_DUPLICATES, 0, 0);
                VLDSetOptions(flags[o], dumps[d], frames[f]);
                void* blocks[5];
                for (int i = 0; i < 5; i++)
                {
                    blocks[i] = Recurse(i * 40, 33);
                    chaos::Check(blocks[i] != NULL, "malloc failed");
                }
                chaos::Check(VLDGetLeaksCount() == 5, "VLDGetLeaksCount is not 5");
                chaos::Check(VLDReportLeaks() == 5, "VLDReportLeaks is not 5");
                VLDResolveCallstacks();
                for (int i = 0; i < 5; i++)
                {
                    free(blocks[i]);
                }
                chaos::Check(VLDGetLeaksCount() == 0, "freed blocks are still counted");
            }
        }
    }

    chaos::SetPhase("cleanup");
    VLDSetOptions(VLD_OPT_SAFE_STACK_WALK | VLD_OPT_AGGREGATE_DUPLICATES, 0, 0);
    VLDSetOptions(options, 256, 64);
    VLDSetReportHook(VLD_RPTHOOK_REMOVE, SwallowHook);
    chaos::Pass();
    return 0;
}
