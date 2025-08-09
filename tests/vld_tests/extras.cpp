#include <vld.h>
#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace {
    class some_class {
    public:
        std::unique_ptr<int> pPointer{ std::make_unique<int>() };
    };

    thread_local some_class s_some_class_thread_local{};
    static some_class s_some_class_static{};

    thread_local std::unique_ptr<int> pPointer_thread_local{ std::make_unique<int>() };
    static std::unique_ptr<int> pPointer{ std::make_unique<int>() };

    // todo: as pointed out here: https://github.com/Azure/vld/issues/4 
	// these two variables are not detected as leaks, while they should.
	static int* iPointer = new int(42);
	static void* vPointer = malloc(42);

	void test_thread_local()
	{
		Assert::IsNotNull(pPointer.get());
        Assert::IsNotNull(pPointer_thread_local.get());
        Assert::IsNotNull(s_some_class_static.pPointer.get());
        Assert::IsNotNull(s_some_class_thread_local.pPointer.get());
		Assert::IsNotNull(iPointer);
		Assert::IsNotNull(vPointer);
	}

    void test_thread_local_2()
    {
        // these do get recognized as leaks
        pPointer_thread_local = std::make_unique<int>(42);
		pPointer = std::make_unique<int>(42);

        Assert::AreEqual(*pPointer, 42);
        Assert::AreEqual(*pPointer_thread_local, 42);
    }
}



namespace vld_tests
{
    /// <summary>
	/// Tests bases on cases reported by users.
    /// </summary>
    TEST_CLASS(extras)
    {
    public:

        TEST_METHOD_INITIALIZE(Setup)
        {
            VLDMarkAllLeaksAsReported();
        }

        TEST_METHOD(TestThreadLocal)
        {
            int leaks = static_cast<int>(VLDGetLeaksCount());
            Assert::AreEqual(0, leaks);

			test_thread_local();

            leaks = static_cast<int>(VLDGetLeaksCount());
            Assert::AreEqual(0, leaks);

			test_thread_local_2();

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
