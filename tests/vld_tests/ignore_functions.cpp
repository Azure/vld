#include <vld.h>
#include "CppUnitTest.h"
#include <string>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

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


namespace vld_tests
{
    TEST_CLASS(ignore_functions)
    {
    public:

        TEST_METHOD_INITIALIZE(Setup)
        {
            VLDMarkAllLeaksAsReported();
        }

        TEST_METHOD(TestIgnoreFunctionsSuccess)
        {
            int leaks = static_cast<int>(VLDGetLeaksCount());
            Assert::AreEqual(0, leaks);

            // All of these strings should be ignored.
            static std::string const osVer = GetOSVersion();
            static std::string const someOtherString = SomeOtherString();
            static std::string const str3 = abcdefg();
            static std::string const str4 = testOtherString();

            leaks = static_cast<int>(VLDGetLeaksCount());
            Assert::AreEqual(0, leaks);
        }

        TEST_METHOD(TestIgnoreFunctionsReportsNonListedLeaks)
        {
            int leaks = static_cast<int>(VLDGetLeaksCount());
            Assert::AreEqual(0, leaks);

            // All of these strings should be ignored.
            static std::string const osVer = GetOSVersion();
            static std::string const someOtherString = SomeOtherString();
            static std::string const str3 = abcdefg();

            //This should be detected as leak
            static std::string const str4 = NotInTheList();

            leaks = static_cast<int>(VLDGetLeaksCount());
            Assert::AreEqual(1, leaks);
        }

        TEST_METHOD(TestIgnoreFunctionsReportsStaticStringLeaks)
        {
            int leaks = static_cast<int>(VLDGetLeaksCount());
            Assert::AreEqual(0, leaks);

            // All of these strings should be ignored.
            static std::string const someOtherString = SomeOtherString();
            static std::string const str3 = abcdefg();

            //This should be detected as leak
            static std::string const osVer = "LeakString";
            static std::string const str4 = NotInTheList();

            leaks = static_cast<int>(VLDGetLeaksCount());
            Assert::AreEqual(2, leaks);
        }


        TEST_METHOD_CLEANUP(TearDown)
        {
            // Check that callstack resolved without unresolved functions (required symbols for all dll's)
            Assert::AreEqual(0, VLDResolveCallstacks());
        }
    };
}
