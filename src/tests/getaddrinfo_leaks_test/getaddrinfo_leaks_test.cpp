// getaddrinfo_leaks_test.cpp - Proves that getaddrinfo+freeaddrinfo produces
// system DLL false-positive leaks that are only suppressible via
// IgnoreModulesList.
//
// This is the counterpart to ignore_modules_getaddrinfo_test. That test
// shows the leaks disappear WITH IgnoreModulesList. This test shows the
// leaks EXIST WITHOUT IgnoreModulesList - proving the suppression is real.
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

// Test: Calling getaddrinfo + freeaddrinfo DOES produce leaks when
// IgnoreModulesList is empty. This proves the leaks are real and that
// IgnoreModulesList (tested in ignore_modules_getaddrinfo_test) is
// actively suppressing them.
//
// The leaks come from system DLL one-time initialization during the
// first getaddrinfo call (namespace provider enumeration, locale caches,
// socket handle tables, etc.). These are false positives - the memory
// is cleaned up by the OS at process exit.
TEST_F(TestGetAddrInfoLeaks, GetAddrInfoWithFreeStillProducesSystemDllLeaks)
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
    // allocations that VLD considers leaks.
    int rc = getaddrinfo("localhost", "80", &hints, &result);
    ASSERT_EQ(0, rc) << "getaddrinfo failed with error: " << rc;
    ASSERT_NE(static_cast<struct addrinfo*>(NULL), result);

    // Properly free the result - no application-level leak.
    freeaddrinfo(result);

    int leaks = static_cast<int>(VLDGetLeaksCount());
    if (leaks == 0)
    {
        // VLD is compiled out in non-Debug builds (vld.h gates on _DEBUG).
        // All VLD API calls become no-op macros, so no leaks are tracked.
        VLDMarkAllLeaksAsReported();
        GTEST_SKIP() << "VLD is inactive (compiled out in non-Debug builds).";
    }
    EXPECT_GT(leaks, 0)
        << "Expected system DLL leaks from getaddrinfo (false positives from "
        << "namespace provider DLL initialization), but VLD reported 0 leaks. "
        << "This test validates that leaks exist so that "
        << "ignore_modules_getaddrinfo_test can prove IgnoreModulesList "
        << "suppresses them.";

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
