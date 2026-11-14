// review_unreadable_no_exception.cpp - PR #74 review comment on vld.cpp:2020.
//
// VLD reads leaked blocks while it holds g_heapMapLock. A block the
// application made unreadable must not raise an exception there: the
// application's vectored handlers run before VLD's __except, under VLD's lock,
// and a guard page raises an exception VLD does not handle at all.

#include "review_common.h"
#include "vld.h"

namespace
{
volatile LONG g_exceptions = 0;

LONG CALLBACK CountExceptions(PEXCEPTION_POINTERS info)
{
    DWORD code = info->ExceptionRecord->ExceptionCode;
    if (code == EXCEPTION_ACCESS_VIOLATION || code == EXCEPTION_GUARD_PAGE)
        InterlockedIncrement(&g_exceptions);
    return EXCEPTION_CONTINUE_SEARCH;
}

int __cdecl SwallowHook(int, wchar_t*, int*)
{
    return 1;
}

void CheckLeak(DWORD protection, const char* what)
{
    chaos::SetPhase(what);
    const SIZE_T size = 64 * 1024;
    BYTE* block = (BYTE*)HeapAlloc(GetProcessHeap(), 0, size);
    chaos::Check(block != NULL, "HeapAlloc failed");
    chaos::Check(VLDGetLeaksCount() == 1, "the block is not counted");
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    BYTE* page = (BYTE*)(((UINT_PTR)block + 2 * si.dwPageSize) & ~((UINT_PTR)si.dwPageSize - 1));
    DWORD old = 0;
    chaos::Check(VirtualProtect(page, si.dwPageSize, protection, &old) != FALSE, "VirtualProtect failed");

    InterlockedExchange(&g_exceptions, 0);
    UINT counted = VLDGetLeaksCount();
    UINT reported = VLDReportLeaks();
    LONG exceptions = InterlockedCompareExchange(&g_exceptions, 0, 0);

    VirtualProtect(page, si.dwPageSize, old, &old);
    chaos::Check(counted == 1 && reported == 1, "the leak is not counted and reported");
    chaos::Check(exceptions == 0, "reading the unreadable block raised an exception in the application");
    VLDMarkAllLeaksAsReported();
    HeapFree(GetProcessHeap(), 0, block);
}
}

int main()
{
    chaos::StartWatchdog(30000);
    VLDSetReportHook(VLD_RPTHOOK_INSTALL, SwallowHook);
    UINT options = VLDGetOptions();
    // Dumps the whole block, across the unreadable page.
    VLDSetOptions(options, 64 * 1024, 64);
    VLDMarkAllLeaksAsReported();
    PVOID handler = AddVectoredExceptionHandler(1, CountExceptions);
    chaos::Check(handler != NULL, "AddVectoredExceptionHandler failed");

    CheckLeak(PAGE_NOACCESS, "a leak with a PAGE_NOACCESS page");
    CheckLeak(PAGE_READWRITE | PAGE_GUARD, "a leak with a PAGE_GUARD page");

    RemoveVectoredExceptionHandler(handler);
    VLDSetOptions(options, 256, 64);
    VLDSetReportHook(VLD_RPTHOOK_REMOVE, SwallowHook);
    chaos::Pass();
    return 0;
}
