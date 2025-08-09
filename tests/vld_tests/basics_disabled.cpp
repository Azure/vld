#include <vld.h>
#include "CppUnitTest.h"
#include "Allocs.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace vld_tests
{
    TEST_CLASS(basics_disabled)
    {
        bool bFreeMemory = false; // todo: this used to be a parameter
    public:

        TEST_METHOD_INITIALIZE(Setup)
        {
            VLDDisable();
        }

        TEST_METHOD(TestMalloc)
        {
            int prev = static_cast<int>(VLDGetLeaksCount());
            LeakMemoryMalloc(repeats, bFreeMemory);
            int total = static_cast<int>(VLDGetLeaksCount());
            int leaks = total - prev;
            Assert::AreEqual(0, leaks);
        }

        TEST_METHOD(TestNew)
        {
            int prev = static_cast<int>(VLDGetLeaksCount());
            LeakMemoryNew(repeats, bFreeMemory);
            int total = static_cast<int>(VLDGetLeaksCount());
            int leaks = total - prev;
            Assert::AreEqual(0, leaks);
        }

        TEST_METHOD(TestNewArray)
        {
            int prev = static_cast<int>(VLDGetLeaksCount());
            LeakMemoryNewArray(repeats, bFreeMemory);
            int total = static_cast<int>(VLDGetLeaksCount());
            int leaks = total - prev;
            Assert::AreEqual(0, leaks);
        }

        TEST_METHOD(TestCalloc)
        {
            int prev = static_cast<int>(VLDGetLeaksCount());
            LeakMemoryCalloc(repeats, bFreeMemory);
            int total = static_cast<int>(VLDGetLeaksCount());
            int leaks = total - prev;
            Assert::AreEqual(0, leaks);
        }

        TEST_METHOD(TestRealloc)
        {
            int prev = static_cast<int>(VLDGetLeaksCount());
            LeakMemoryRealloc(repeats, bFreeMemory);
            int total = static_cast<int>(VLDGetLeaksCount());
            int leaks = total - prev;
            Assert::AreEqual(0, leaks);
        }

        TEST_METHOD(TestCoTaskMem)
        {
            int prev = static_cast<int>(VLDGetLeaksCount());
            LeakMemoryCoTaskMem(repeats, bFreeMemory);
            int total = static_cast<int>(VLDGetLeaksCount());
            int leaks = total - prev;
            Assert::AreEqual(0, leaks);
        }

        TEST_METHOD(TestAlignedMalloc)
        {
            int prev = static_cast<int>(VLDGetLeaksCount());
            LeakMemoryAlignedMalloc(repeats, bFreeMemory);
            int total = static_cast<int>(VLDGetLeaksCount());
            int leaks = total - prev;
            Assert::AreEqual(0, leaks);
        }

        TEST_METHOD(TestAlignedRealloc)
        {
            int prev = static_cast<int>(VLDGetLeaksCount());
            LeakMemoryAlignedRealloc(repeats, bFreeMemory);
            int total = static_cast<int>(VLDGetLeaksCount());
            int leaks = total - prev;
            Assert::AreEqual(0, leaks);
        }

        TEST_METHOD(TestStrdup)
        {
            int prev = static_cast<int>(VLDGetLeaksCount());
            LeakMemoryStrdup(repeats, bFreeMemory);
            int total = static_cast<int>(VLDGetLeaksCount());
            int leaks = total - prev;
            Assert::AreEqual(0, leaks);
        }

        TEST_METHOD(TestHeapAlloc)
        {
            int prev = static_cast<int>(VLDGetLeaksCount());
            LeakMemoryHeapAlloc(repeats, bFreeMemory);
            int total = static_cast<int>(VLDGetLeaksCount());
            int leaks = total - prev;
            Assert::AreEqual(0, leaks);
        }

        TEST_METHOD(TestIMalloc)
        {
            int prev = static_cast<int>(VLDGetLeaksCount());
            LeakMemoryIMalloc(repeats, bFreeMemory);
            int total = static_cast<int>(VLDGetLeaksCount());
            int leaks = total - prev;
            Assert::AreEqual(0, leaks);
        }

        TEST_METHOD(TestGetProcMalloc)
        {
            int prev = static_cast<int>(VLDGetLeaksCount());
            LeakMemoryGetProcMalloc(repeats, bFreeMemory);
            int total = static_cast<int>(VLDGetLeaksCount());
            int leaks = total - prev;
            Assert::AreEqual(0, leaks);
        }

        TEST_METHOD_CLEANUP(TearDown)
        {
            // Check that callstack resolved without unresolved functions (required symbols for all dll's)
            VLDEnable();
        }
    };
}
