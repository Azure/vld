// getaddrinfo_leaks_test.cpp - Verifies that getaddrinfo+freeaddrinfo does NOT
// produce false-positive leaks from system DLLs.
//
// System DLLs loaded during getaddrinfo (rasadhlp.dll, dnsapi.dll, mswsock.dll,
// etc.) perform one-time initialization allocations that persist for the process
// lifetime. With the improved module detection in ShouldReportLeaksForModule(),
// these allocations are automatically excluded because the system DLLs do not
// import VLD and are not user code.
//
// For modules not yet processed by VLD (e.g., during DllMain of a newly loaded
// DLL), ShouldReportLeaksForModule uses FindImport to directly check whether
// the module imports VLD, correctly distinguishing system DLLs from user code.
//
// The vld.ini for this test intentionally has an empty IgnoreModulesList.

#include <gtest/gtest.h>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include "vld.h"

#pragma comment(lib, "ws2_32.lib")

class TestGetAddrInfoLeaks : public ::testing::Test
{
    virtual void SetUp()
    {
        WSADATA wsaData;
        int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        ASSERT_EQ(0, result) << "WSAStartup failed with error: " << result;
        VLDMarkAllLeaksAsReported();
    }

    virtual void TearDown()
    {
        WSACleanup();
        EXPECT_EQ(0, VLDResolveCallstacks());
    }
};

// Test: Calling getaddrinfo + freeaddrinfo should NOT produce false-positive
// leaks from system DLL initialization, even without IgnoreModulesList.
//
// During the first getaddrinfo call, namespace provider DLLs (rasadhlp.dll,
// dnsapi.dll, mswsock.dll, fwpuclnt.dll, etc.) are dynamically loaded.
// Their DLL initialization allocates memory that persists for the process
// lifetime. With improved module detection, VLD recognizes that these
// allocations originate from modules that do not import VLD (system DLLs)
// and automatically excludes them from leak reporting.
TEST_F(TestGetAddrInfoLeaks, GetAddrInfoWithFreeHasNoSystemDllFalsePositives)
{
    int leaks_before = static_cast<int>(VLDGetLeaksCount());
    ASSERT_EQ(0, leaks_before);

    struct addrinfo hints;
    struct addrinfo* result = NULL;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    // Resolve localhost. This triggers system DLL loading and internal
    // allocations. With improved module detection, these should NOT be
    // reported as leaks.
    int rc = getaddrinfo("localhost", "80", &hints, &result);
    ASSERT_EQ(0, rc) << "getaddrinfo failed with error: " << rc;
    ASSERT_NE(static_cast<struct addrinfo*>(NULL), result);

    // Properly free the result - no application-level leak.
    freeaddrinfo(result);

    int leaks = static_cast<int>(VLDGetLeaksCount());
    if (0 != leaks)
        VLDReportLeaks();
    EXPECT_EQ(0, leaks)
        << "Expected 0 leaks after getaddrinfo+freeaddrinfo. System DLL "
        << "internal allocations should be automatically excluded because "
        << "they originate from modules that do not import VLD. "
        << "VLD reported " << leaks << " leak(s).";

    // Mark all leaks as reported so VLD exits cleanly.
    VLDMarkAllLeaksAsReported();
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    int res = RUN_ALL_TESTS();
    VLDMarkAllLeaksAsReported();
    return res;
}
