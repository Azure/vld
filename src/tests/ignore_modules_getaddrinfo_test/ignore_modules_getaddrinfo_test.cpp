// ignore_modules_getaddrinfo_test.cpp - Integration test for IgnoreModulesList
// with system DLL suppression during getaddrinfo.
//
// When getaddrinfo is first called, the Winsock layer dynamically loads
// several namespace provider DLLs (rasadhlp.dll, dnsapi.dll, mswsock.dll,
// fwpuclnt.dll, etc.) whose DLL initialization allocates memory that persists
// for the process lifetime and is cleaned up by the OS at process exit.
// These allocations are false positives for VLD.
//
// The specific DLLs that leak depend on the OS version and installed network
// providers. Their internal functions are unexported and cannot be targeted
// by IgnoreFunctionsList, so the entire modules must be excluded via
// IgnoreModulesList.
//
// This test verifies that adding the common leaking system DLLs to
// IgnoreModulesList suppresses these false positives while
// getaddrinfo+freeaddrinfo usage remains correct.

#include <gtest/gtest.h>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include "vld.h"

#pragma comment(lib, "ws2_32.lib")

class TestIgnoreModulesGetAddrInfo : public ::testing::Test
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

// Test: Calling getaddrinfo + freeaddrinfo for localhost should not report
// any leaks when the common leaking system DLLs are in IgnoreModulesList.
//
// Without IgnoreModulesList, this test would report leaks from system DLL
// initialization (e.g., 256-byte blocks from rasadhlp.dll, 400-byte blocks
// from DNSAPI.dll locale comparison, socket handles from mswsock.dll, and
// network policy allocations from fwpuclnt.dll).
TEST_F(TestIgnoreModulesGetAddrInfo, GetAddrInfoWithFreeHasNoLeaks)
{
    int leaks_before = static_cast<int>(VLDGetLeaksCount());
    ASSERT_EQ(0, leaks_before);

    struct addrinfo hints;
    struct addrinfo* result = NULL;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    // Resolve localhost. This triggers loading of namespace provider DLLs.
    int rc = getaddrinfo("localhost", "80", &hints, &result);
    ASSERT_EQ(0, rc) << "getaddrinfo failed with error: " << rc;
    ASSERT_NE(static_cast<struct addrinfo*>(NULL), result);

    // Properly free the result - no application-level leak.
    freeaddrinfo(result);

    int leaks = static_cast<int>(VLDGetLeaksCount());
    if (0 != leaks)
        VLDReportLeaks();
    ASSERT_EQ(0, leaks)
        << "Expected 0 leaks after getaddrinfo+freeaddrinfo with system DLLs "
        << "in IgnoreModulesList, but VLD reported "
        << leaks << " leak(s). If a new system DLL is leaking, add it to "
        << "IgnoreModulesList in vld.ini for this test.";
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    int res = RUN_ALL_TESTS();
    VLDMarkAllLeaksAsReported();
    return res;
}
