#include <vld.h>
#include "CppUnitTest.h"
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <filesystem>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
namespace fs = std::filesystem;

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

namespace vld_tests
{
    TEST_CLASS(corruption)
    {
    public:

        TEST_METHOD_INITIALIZE(Setup)
        {
            VLDMarkAllLeaksAsReported();
        }

        TEST_METHOD(AllocMismatch)
        {
            VLDMarkAllLeaksAsReported();
            TestAllocationMismatch_malloc_delete();
            TestAllocationMismatch_malloc_deleteVec();
            TestAllocationMismatch_new_free();
            TestAllocationMismatch_newVec_free();
        }

        TEST_METHOD(HeapMismatch)
        {
            PROCESS_INFORMATION processInformation = { 0 };
            STARTUPINFO startupInfo = { 0 };
            startupInfo.cb = sizeof(startupInfo);

            auto exe = fs::current_path().append("vld_heapcorruption.exe");

            // Create the process
            BOOL result = CreateProcess(exe.c_str(), NULL,
                NULL, NULL, FALSE,
                NORMAL_PRIORITY_CLASS | CREATE_NO_WINDOW,
                NULL, NULL, &startupInfo, &processInformation);
            Assert::AreNotEqual(0, result);

            // Successfully created the process.  Wait for it to finish.
            Assert::AreEqual(WAIT_OBJECT_0, WaitForSingleObject(processInformation.hProcess, INFINITE));

            // Get the exit code.
            DWORD exitCode = 0;
            result = GetExitCodeProcess(processInformation.hProcess, &exitCode);
            Assert::AreNotEqual(0, result);

            // Close the handles.
            CloseHandle(processInformation.hProcess);
            CloseHandle(processInformation.hThread);
            Assert::AreEqual(STATUS_HEAP_CORRUPTION, exitCode);
        }

        TEST_METHOD_CLEANUP(TearDown)
        {
            // Check that callstack resolved without unresolved functions (required symbols for all dll's)
            Assert::AreEqual(0, VLDResolveCallstacks());
        }
    };
}
