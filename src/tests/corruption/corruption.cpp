// corruption.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "Tests.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wchar.h>
// This hooks vld into this app
#include "vld.h"

#include <gtest/gtest.h>

enum CorruptionType
{
	eAllocMismatch,
	eHeapMismatch,
	eCount
};

int __cdecl ReportHook(int /*reportHook*/, wchar_t *message, int* /*returnValue*/)
{
    static bool deallocationStack = false;
    fwprintf(stderr, L"%s", message);
    // Exit once VLD dumps the deallocation call stack (its first frame), before
    // the mismatched free reaches the heap: corrupting it can raise an exception
    // with the heap lock held, and gtest's SEH handling then deadlocks starting
    // a thread that needs that lock.
    if (wcsstr(message, L"Deallocation Call stack.") != NULL)
    {
        deallocationStack = true;
    }
    else if (deallocationStack && wcschr(message, L'!') != NULL)
    {
        fflush(stderr);
        TerminateProcess(GetCurrentProcess(), 3);
    }
    return 1;
}

bool IgnoreExitCode(int exit_status)
{
    UNREFERENCED_PARAMETER(exit_status);
    return true;
}

void TestCorruption( CorruptionType check )
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
    EXPECT_EXIT({
        VLDSetReportHook(VLD_RPTHOOK_INSTALL, ReportHook);
        TestCorruption(eHeapMismatch);
        VLDSetReportHook(VLD_RPTHOOK_REMOVE, ReportHook);
    }, testing::ExitedWithCode(3), "CRITICAL ERROR!: VLD reports that memory was allocated in one heap and freed in another.");
}

int main(int argc, char **argv) {
    UINT vld_options = VLDGetOptions();
    vld_options |= VLD_OPT_VALIDATE_HEAPFREE;
    VLDSetOptions(vld_options, 15, 25);

    ::testing::InitGoogleTest(&argc, argv);
    int res = RUN_ALL_TESTS();
    VLDMarkAllLeaksAsReported();
    return res;
}
