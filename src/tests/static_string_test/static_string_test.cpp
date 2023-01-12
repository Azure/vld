// static_string_test.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "vld.h"
#include "static_string.h"

#include <gtest/gtest.h>

class StaticStringTest : public ::testing::Test
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

void access_strings()
{
    // Just do something with the string so it isn't optimized away
    std::string copied_string = my_string::the_string;
    printf("Copied string %s\n", copied_string.c_str());

    std::string copied_string2 = string_global;
    printf("Copied string %s\n", copied_string2.c_str());
}

TEST_F(StaticStringTest, StaticStringsSuccess)
{
    int leaks = static_cast<int>(VLDGetLeaksCount());
    ASSERT_EQ(0, leaks);

    access_strings();

    leaks = static_cast<int>(VLDGetLeaksCount());
    ASSERT_EQ(0, leaks);

}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    int res = RUN_ALL_TESTS();
    VLDMarkAllLeaksAsReported();
    return res;
}
