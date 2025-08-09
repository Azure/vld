#include "CppUnitTest.h"
#include <tchar.h>
#include <Windows.h>
#include <string>
#include <iostream>
#include <filesystem>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
namespace fs = std::filesystem;

namespace vld_tests
{
    TEST_CLASS(vld_main)
    {
    public:
        TEST_METHOD(TestVldMain)
        {
            PROCESS_INFORMATION processInformation = { 0 };
            STARTUPINFO startupInfo = { 0 };
            startupInfo.cb = sizeof(startupInfo);

            auto exe = fs::current_path().append("vld_main.exe");

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
            Assert::AreEqual(2, (int)exitCode);
        }
    };
}
