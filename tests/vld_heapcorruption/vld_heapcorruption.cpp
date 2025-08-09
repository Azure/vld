#include <vld.h>
#include <iostream>
#include <Windows.h>

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

int __cdecl ReportHook(int /*reportHook*/, wchar_t* message, int* /*returnValue*/)
{
    fwprintf(stderr, L"%s", message);
    return 1;
}

int main()
{
    UINT vld_options = VLDGetOptions();
    vld_options |= VLD_OPT_VALIDATE_HEAPFREE;
    VLDSetOptions(vld_options, 15, 25);

    VLDSetReportHook(VLD_RPTHOOK_INSTALL, ReportHook);

    TestHeapMismatch();

    VLDSetReportHook(VLD_RPTHOOK_REMOVE, ReportHook);
}