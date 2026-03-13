// ignore_modules_test.cpp - Integration test for IgnoreModulesList.
//
// This test verifies that VLD's IgnoreModulesList configuration option
// correctly suppresses leak reports from specified modules.
//
// The test dynamically loads ignore_modules_dll.dll, which intentionally
// leaks memory. A custom vld.ini in the test's output directory lists
// "ignore_modules_dll.dll" in IgnoreModulesList. The test verifies that
// allocations originating from the ignored module are not counted as leaks,
// while allocations from non-ignored code are still detected.

#include <gtest/gtest.h>
#include <Windows.h>
#include <tchar.h>
#include "vld.h"

typedef void (*LeakFromDllFunc)(void);

class TestIgnoreModules : public ::testing::Test
{
    virtual void SetUp()
    {
        VLDMarkAllLeaksAsReported();
    }

    virtual void TearDown()
    {
        EXPECT_EQ(0, VLDResolveCallstacks());
    }
};

// Test 1: Allocations from a module in IgnoreModulesList should not be
// reported as leaks.
TEST_F(TestIgnoreModules, IgnoredModuleLeaksAreNotReported)
{
    int leaks_before = static_cast<int>(VLDGetLeaksCount());
    ASSERT_EQ(0, leaks_before);

    HMODULE hdll = LoadLibrary(_T("ignore_modules_dll.dll"));
    ASSERT_NE(static_cast<HMODULE>(NULL), hdll)
        << "Failed to load ignore_modules_dll.dll. "
        << "Make sure it is in the same directory as the test executable.";

    LeakFromDllFunc LeakFromDll =
        (LeakFromDllFunc)GetProcAddress(hdll, "LeakFromDll");
    ASSERT_NE(static_cast<LeakFromDllFunc>(NULL), LeakFromDll);

    // This leaks 64 bytes inside ignore_modules_dll.dll.
    // Because the module is in IgnoreModulesList, VLD should ignore it.
    LeakFromDll();

    int leaks = static_cast<int>(VLDGetLeaksCount());
    if (0 != leaks)
        VLDReportLeaks();
    ASSERT_EQ(0, leaks)
        << "Expected 0 leaks because ignore_modules_dll.dll is in IgnoreModulesList, "
        << "but VLD reported " << leaks << " leak(s).";

    FreeLibrary(hdll);
}

// Prevent tail-call optimization so the test function appears in the stack.
#pragma optimize("", off)

static void* LeakFromTestExe()
{
    return malloc(32);
}

#pragma optimize("", on)

// Test 2: Allocations from non-ignored modules should still be reported.
// The test leaks from both the ignored DLL and the test exe. Only the
// exe leak should be counted.
TEST_F(TestIgnoreModules, NonIgnoredModuleLeaksAreStillReported)
{
    int leaks_before = static_cast<int>(VLDGetLeaksCount());
    ASSERT_EQ(0, leaks_before);

    HMODULE hdll = LoadLibrary(_T("ignore_modules_dll.dll"));
    ASSERT_NE(static_cast<HMODULE>(NULL), hdll);

    LeakFromDllFunc LeakFromDll =
        (LeakFromDllFunc)GetProcAddress(hdll, "LeakFromDll");
    ASSERT_NE(static_cast<LeakFromDllFunc>(NULL), LeakFromDll);

    // Leak from ignored DLL - should NOT be counted.
    LeakFromDll();

    // Leak from test exe - should be counted.
    static volatile void* p = LeakFromTestExe();
    (void)p;

    int leaks = static_cast<int>(VLDGetLeaksCount());
    if (1 != leaks)
        VLDReportLeaks();
    ASSERT_EQ(1, leaks)
        << "Expected exactly 1 leak (from the test exe), "
        << "but VLD reported " << leaks << " leak(s).";

    FreeLibrary(hdll);
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    int res = RUN_ALL_TESTS();
    VLDMarkAllLeaksAsReported();
    return res;
}
