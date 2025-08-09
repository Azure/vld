#include "pch.h"
#include <vld.h>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

enum CorruptionType
{
    eAllocMismatch,
    eHeapMismatch,
    eCount
};

int __cdecl ReportHook(int /*reportHook*/, wchar_t* message, int* /*returnValue*/)
{
    fwprintf(stderr, L"%s", message);
    return 1;
}

bool IgnoreExitCode(int)
{
    return true;
}

void TestAllocationMismatch_malloc_delete()
{
    int* pint = (int*)malloc(56);
    // use the wrong function to deallocate
    delete pint;
}

void TestAllocationMismatch_malloc_deleteVec()
{
    int* pint = (int*)malloc(56);
    // use the wrong function to deallocate
    delete[] pint;
}

void TestAllocationMismatch_new_free()
{
    int* pint = new int(45); // scalar new
    // use the wrong function to deallocate
    free(pint);
}

void TestAllocationMismatch_newVec_free()
{
    int* pint = new int[3]; // vector new
    // use the wrong function to deallocate
    free(pint);
}

void TestHeapMismatch()
{
    HANDLE test_heap = HeapCreate(HEAP_GENERATE_EXCEPTIONS | HEAP_CREATE_ENABLE_EXECUTE,
        0, // initialize reserved size;
        0); // maximum size can grow
    HeapSetInformation(test_heap, HeapEnableTerminationOnCorruption, NULL, 0);
    HANDLE default_heap = GetProcessHeap();
    HeapSetInformation(default_heap, HeapEnableTerminationOnCorruption, NULL, 0);
    // Have to initialize vld with something, so it doesn't dismiss
    // the default heap as having no memory allocs in it.
    void* aptr = HeapAlloc(default_heap, 0, 42);
    // Allocate it in the new heap
    void* vptr = HeapAlloc(test_heap, 0, 56);
    // Free this using the WRONG heap!
    HeapFree(default_heap, 0, vptr);

    HeapDestroy(test_heap);
}

void TestCorruption(CorruptionType check)
{
    // Mark all previous memory leaks as reported which are not part of the current test.
    VLDMarkAllLeaksAsReported();

    if (check == eAllocMismatch)
    {
        TestAllocationMismatch_malloc_delete();
        TestAllocationMismatch_malloc_deleteVec();
        TestAllocationMismatch_new_free();
        TestAllocationMismatch_newVec_free();
    }
    else if (check == eHeapMismatch)
    {
        TestHeapMismatch();
    }
}

TEST(Corruption, AllocMismatch)
{
    TestCorruption(eAllocMismatch);
}

TEST(CorruptionDeathTest, HeapMismatch)
{
    UINT vld_options = VLDGetOptions();
    vld_options |= VLD_OPT_VALIDATE_HEAPFREE;
    VLDSetOptions(vld_options, 15, 25);

    EXPECT_EXIT({
        VLDSetReportHook(VLD_RPTHOOK_INSTALL, ReportHook);
        TestCorruption(eHeapMismatch);
        VLDSetReportHook(VLD_RPTHOOK_REMOVE, ReportHook);
        }, IgnoreExitCode, "CRITICAL ERROR!: VLD reports that memory was allocated in one heap and freed in another.");
}
