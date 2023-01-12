// ignore_functions_test.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "vld.h"
#include <string>

#include <gtest/gtest.h>

class TestIgnoreFunctions : public ::testing::Test
{
    virtual void SetUp()
    {
        VLDMarkAllLeaksAsReported();
    }
    virtual void TearDown()
    {
        // Check that callstack resolved without unresolved functions (required symbols for all dll's)
        EXPECT_EQ(0, VLDResolveCallstacks());
    }
};

std::string GetOSVersion() 
{
    std::string osVersion = "Windows";
    return osVersion;
}

std::string SomeOtherString() 
{
    std::string osVersion = "Windows";
    return osVersion;
}

std::string abcdefg() 
{
    std::string osVersion = "Windows";
    return osVersion;
}

std::string testOtherString() 
{
    std::string osVersion = "Windows";
    return osVersion;
}

std::string NotInTheList() 
{
    std::string osVersion = "NotListed";
    return osVersion;
}

TEST_F(TestIgnoreFunctions, IgnoreFunctionsSuccess)
{
    int leaks = static_cast<int>(VLDGetLeaksCount());
    ASSERT_EQ(0, leaks);

    // All of these strings should be ignored.
    static std::string const osVer = GetOSVersion();
    static std::string const someOtherString = SomeOtherString();
    static std::string const str3 = abcdefg();
    static std::string const str4 = testOtherString();

    leaks = static_cast<int>(VLDGetLeaksCount());
    ASSERT_EQ(0, leaks);
}

TEST_F(TestIgnoreFunctions, IgnoreFunctionsReportsNonListedLeaks)
{
    int leaks = static_cast<int>(VLDGetLeaksCount());
    ASSERT_EQ(0, leaks);

    // All of these strings should be ignored.
    static std::string const osVer = GetOSVersion();
    static std::string const someOtherString = SomeOtherString();
    static std::string const str3 = abcdefg();

    //This should be detected as leak
    static std::string const str4 = NotInTheList();

    leaks = static_cast<int>(VLDGetLeaksCount());
    ASSERT_EQ(1, leaks);
}

TEST_F(TestIgnoreFunctions, IgnoreFunctionsReportsStaticStringLeaks)
{
    int leaks = static_cast<int>(VLDGetLeaksCount());
    ASSERT_EQ(0, leaks);

    // All of these strings should be ignored.
    static std::string const someOtherString = SomeOtherString();
    static std::string const str3 = abcdefg();

    //This should be detected as leak
    static std::string const osVer = "LeakString";
    static std::string const str4 = NotInTheList();

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
