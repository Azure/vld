// review_crt_noaccess_leak.cpp - PR #74 review comment on vld.cpp:2156.
//
// A leaked debug CRT block whose header page the application made
// inaccessible is still a leak: VLD must count and report it, like a heap
// block it cannot read, not drop it as freed. Only Debug builds have debug CRT
// blocks; elsewhere the block is a plain heap block, counted either way.

#include "review_common.h"
#include "vld.h"
#include <stdlib.h>

namespace
{
int __cdecl SwallowHook(int, wchar_t*, int*)
{
    return 1;
}
}

int main()
{
    chaos::StartWatchdog(30000);
    VLDSetReportHook(VLD_RPTHOOK_INSTALL, SwallowHook);
    VLDMarkAllLeaksAsReported();

    chaos::SetPhase("leaking a large CRT block");
    // Large enough for the heap to give it pages of its own.
    const size_t size = 1024 * 1024;
    BYTE* data = (BYTE*)malloc(size);
    chaos::Check(data != NULL, "malloc failed");
    memset(data, 0x5A, size);
    // Looks up the block's symbols now, so that the queries below need no
    // allocation that could touch the heap's pages around the block.
    chaos::Check(VLDGetLeaksCount() == 1, "the CRT block is not counted");

    chaos::SetPhase("making the page with the block's header inaccessible");
    SYSTEM_INFO si;
    GetSystemInfo(&si);
#ifdef _DEBUG
    BYTE* first = data - 1;
#else
    // No header; the heap can give the block a page of its own, so the byte
    // before it can be on a page that is not even committed.
    BYTE* first = data;
#endif
    BYTE* page = (BYTE*)((UINT_PTR)first & ~((UINT_PTR)si.dwPageSize - 1));
    DWORD old = 0;
    chaos::Check(VirtualProtect(page, si.dwPageSize, PAGE_NOACCESS, &old) != FALSE, "VirtualProtect failed");
    UINT counted = VLDGetLeaksCount();
    UINT reported = VLDReportLeaks();
    VirtualProtect(page, si.dwPageSize, old, &old);

    chaos::Check(counted == 1, "VLDGetLeaksCount dropped a CRT block whose header is not readable");
    chaos::Check(reported == 1, "VLDReportLeaks dropped a CRT block whose header is not readable");
    free(data);
    chaos::Check(VLDGetLeaksCount() == 0, "the freed block is still counted");
    VLDSetReportHook(VLD_RPTHOOK_REMOVE, SwallowHook);
    chaos::Pass();
    return 0;
}
