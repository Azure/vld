// chaos_noaccess_leak.cpp - a leaked block with inaccessible pages is still
// a leak.
//
// The application makes an interior page of a large leaked block
// PAGE_NOACCESS. Counting and reporting must agree on the leak, with a data
// dump that spans the page.

#include "chaos_common.h"
#include "vld.h"

namespace
{
int __cdecl SwallowHook(int, wchar_t*, int*)
{
    return 1;
}
}

int main()
{
    chaos::StartWatchdog(20000);
    VLDSetReportHook(VLD_RPTHOOK_INSTALL, SwallowHook);
    UINT options = VLDGetOptions();
    VLDMarkAllLeaksAsReported();

    chaos::SetPhase("leaking a block with an inaccessible page");
    const SIZE_T size = 64 * 1024;
    BYTE* block = (BYTE*)HeapAlloc(GetProcessHeap(), 0, size);
    chaos::Check(block != NULL, "HeapAlloc failed");
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    BYTE* page = (BYTE*)(((UINT_PTR)block + 2 * si.dwPageSize) & ~((UINT_PTR)si.dwPageSize - 1));
    DWORD old = 0;
    chaos::Check(VirtualProtect(page, si.dwPageSize, PAGE_NOACCESS, &old) != FALSE, "VirtualProtect failed");

    chaos::SetPhase("counting and reporting");
    VLDSetOptions(options, size, 64);
    UINT counted = VLDGetLeaksCount();
    UINT reported = VLDReportLeaks();

    chaos::SetPhase("cleanup");
    VirtualProtect(page, si.dwPageSize, old, &old);
    VLDSetOptions(options, 256, 64);
    chaos::Check(counted == 1, "VLDGetLeaksCount is not 1");
    chaos::Check(reported == 1, "VLDReportLeaks skipped a leak whose data is not readable");
    HeapFree(GetProcessHeap(), 0, block);
    chaos::Check(VLDGetLeaksCount() == 0, "the freed block is still counted");
    VLDSetReportHook(VLD_RPTHOOK_REMOVE, SwallowHook);
    chaos::Pass();
    return 0;
}
