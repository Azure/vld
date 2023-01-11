// ignore_functions_test.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "vld.h"
#include <string>


std::string GetOSVersion() {
    std::string osVersion = "Windows";
    return osVersion;
}

std::string SomeOtherString() {
    std::string osVersion = "Windows";
    return osVersion;
}

std::string abcdefg() {
    std::string osVersion = "Windows";
    return osVersion;
}

std::string testOtherString() {
    std::string osVersion = "Windows";
    return osVersion;
}

int main(int argc, char** argv)
{
    static std::string const osVer = GetOSVersion();
    static std::string const someOtherString = SomeOtherString();
    static std::string const str3 = abcdefg();
    static std::string const str4 = testOtherString();

    int leaks = static_cast<int>(VLDGetLeaksCount());
    if (0 != leaks)
    {
        (void)printf("!!! FAILED - Leaks detected: %i\n", leaks);
        VLDReportLeaks();
    }
    else
    {
        (void)printf("PASSED\n");
    }


    return leaks;
}
