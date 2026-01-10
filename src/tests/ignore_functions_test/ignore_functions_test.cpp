// ignore_functions_test.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "vld.h"

#include <gtest/gtest.h>

class TestIgnoreFunctions : public ::testing::Test
{
    virtual void SetUp()
    {
        VLDMarkAllLeaksAsReported();
    }
    virtual void TearDown()
    {
        // Force resolve all callstacks first
        int unresolved = VLDResolveCallstacks();
        // Check that callstack resolved without unresolved functions (required symbols for all dll's)
        EXPECT_EQ(0, unresolved) << "VLDResolveCallstacks returned " << unresolved << " unresolved functions";
    }
};

// Use new/malloc directly to have deterministic allocation counts.
// These functions are added to IgnoreFunctionsList in vld.ini
//
// IMPORTANT: We must disable optimizations for these functions to prevent
// tail call optimization. When a function's last action is "return new ...",
// the compiler can replace the "call new; ret" sequence with a direct "jmp new",
// eliminating this function's stack frame entirely. This causes VLD to not see
// these function names in the call stack, so they won't match the IgnoreFunctionsList.
// Disabling optimization ensures proper stack frames are created.
#pragma optimize("", off)
void* GetOSVersion() 
{
    void* ptr = new char[32];  // 1 allocation
    return ptr;
}

void* SomeOtherString() 
{
    void* ptr = new char[32];  // 1 allocation
    return ptr;
}

void* abcdefg() 
{
    void* ptr = new char[32];  // 1 allocation
    return ptr;
}

void* testOtherString() 
{
    void* ptr = new char[32];  // 1 allocation
    return ptr;
}

// This function is NOT in IgnoreFunctionsList
void* NotInTheList() 
{
    void* ptr = new char[32];  // 1 allocation - should be detected as leak
    return ptr;
}
#pragma optimize("", on)

TEST_F(TestIgnoreFunctions, IgnoreFunctionsSuccess)
{
    int leaks = static_cast<int>(VLDGetLeaksCount());
    ASSERT_EQ(0, leaks);

    // All of these allocations should be ignored because the functions
    // are listed in IgnoreFunctionsList in vld.ini
    static void* p1 = GetOSVersion();       // ignored
    static void* p2 = SomeOtherString();    // ignored
    static void* p3 = abcdefg();            // ignored
    static void* p4 = testOtherString();    // ignored
    (void)p1; (void)p2; (void)p3; (void)p4; // suppress unused warnings

    leaks = static_cast<int>(VLDGetLeaksCount());
    ASSERT_EQ(0, leaks);
}

TEST_F(TestIgnoreFunctions, IgnoreFunctionsReportsNonListedLeaks)
{
    int leaks = static_cast<int>(VLDGetLeaksCount());
    ASSERT_EQ(0, leaks);

    // These allocations should be ignored (functions in IgnoreFunctionsList)
    static void* p1 = GetOSVersion();       // ignored
    static void* p2 = SomeOtherString();    // ignored
    static void* p3 = abcdefg();            // ignored
    (void)p1; (void)p2; (void)p3;

    // This should be detected as leak - NotInTheList is NOT in IgnoreFunctionsList
    static void* p4 = NotInTheList();       // NOT ignored - 1 leak
    (void)p4;

    leaks = static_cast<int>(VLDGetLeaksCount());
    ASSERT_EQ(1, leaks);
}

TEST_F(TestIgnoreFunctions, IgnoreFunctionsReportsStaticStringLeaks)
{
    int leaks = static_cast<int>(VLDGetLeaksCount());
    ASSERT_EQ(0, leaks);

    // These allocations should be ignored (functions in IgnoreFunctionsList)
    static void* p1 = SomeOtherString();    // ignored
    static void* p2 = abcdefg();            // ignored
    (void)p1; (void)p2;

    // These should be detected as leaks
    static void* p3 = new char[64];         // NOT ignored - 1 leak (inline allocation)
    static void* p4 = NotInTheList();       // NOT ignored - 1 leak
    (void)p3; (void)p4;

    leaks = static_cast<int>(VLDGetLeaksCount());
    ASSERT_EQ(2, leaks);
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    int res = RUN_ALL_TESTS();
    VLDMarkAllLeaksAsReported();
    return res;
}
