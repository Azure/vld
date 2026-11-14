// review_validate_heapfree.cpp - PR #74 review comment on vld.cpp:1715.
//
// With ValidateHeapFree, freeing a block on a heap other than its own makes
// VLD print both call stacks. They must name the functions, which needs
// dbghelp, so VLD must print them without holding g_heapMapLock.
//
// The child frees a block on the wrong heap; its report hook exits with 3
// once both stacks named their function, before the free reaches the heap.

#include "review_common.h"
#include "vld.h"

namespace
{
wchar_t g_report[64 * 1024];
size_t g_length = 0;
// Messages since the deallocation stack's "Call Stack:" heading; VLD prints a
// whole call stack in one message.
int g_afterDeallocationHeading = -1;

int __cdecl Hook(int, wchar_t* message, int*)
{
    size_t length = wcslen(message);
    if (g_length + length + 1 < _countof(g_report))
    {
        memcpy(g_report + g_length, message, length * sizeof(wchar_t));
        g_length += length;
        g_report[g_length] = L'\0';
    }
    const wchar_t* deallocation = wcsstr(g_report, L"Deallocation Call stack.");
    if (deallocation == NULL)
        return 1;
    if (g_afterDeallocationHeading < 0)
    {
        if (wcsstr(message, L"Call Stack:") != NULL)
            g_afterDeallocationHeading = 0;
        return 1;
    }
    const wchar_t* allocation = wcsstr(g_report, L"Allocation Call stack.");
    const wchar_t* allocator = wcsstr(g_report, L"ReviewAllocOnHeap");
    bool allocationNamed = allocation != NULL && allocator != NULL && allocation < allocator && allocator < deallocation;
    bool deallocationNamed = wcsstr(deallocation, L"ReviewFreeOnOtherHeap") != NULL;
    TerminateProcess(GetCurrentProcess(), !allocationNamed ? 4 : !deallocationNamed ? 5 : 3);
    return 1;
}

#pragma optimize("", off)
__declspec(noinline) void* ReviewAllocOnHeap(HANDLE heap)
{
    void* block = HeapAlloc(heap, 0, 64);
    return block;
}

__declspec(noinline) void ReviewFreeOnOtherHeap(HANDLE heap, void* block)
{
    HeapFree(heap, 0, block);
}
#pragma optimize("", on)

int Child()
{
    VLDSetOptions(VLDGetOptions() | VLD_OPT_VALIDATE_HEAPFREE, 64, 32);
    VLDSetReportHook(VLD_RPTHOOK_INSTALL, Hook);
    HANDLE heapA = HeapCreate(0, 0, 0);
    HANDLE heapB = HeapCreate(0, 0, 0);
    if (heapA == NULL || heapB == NULL)
        return 2;
    void* block = ReviewAllocOnHeap(heapA);
    if (block == NULL)
        return 2;
    ReviewFreeOnOtherHeap(heapB, block);
    // The hook did not see both stacks named.
    TerminateProcess(GetCurrentProcess(), 5);
    return 5;
}
}

int main()
{
    if (review::ChildMode() != NULL)
        return Child();

    chaos::StartWatchdog(60000);
    chaos::SetPhase("freeing a block on the wrong heap in a child");
    DWORD exitCode = review::RunChild(L"heapfree", 50000);
    char message[512];
    sprintf_s(message, "the child exited with %lu: 3 means both stacks were printed with function names, 4 that the "
        "allocation stack was not, 5 that the deallocation stack was not", exitCode);
    chaos::Check(exitCode == 3, message);
    chaos::Pass();
    return 0;
}
