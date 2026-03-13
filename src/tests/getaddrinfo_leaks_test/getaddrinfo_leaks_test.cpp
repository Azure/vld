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
    EXPECT_GT(leaks, 0)
        << "Expected system DLL leaks from getaddrinfo (false positives from "
        << "namespace provider DLL initialization), but VLD reported 0 leaks. "
        << "This test validates that leaks exist so that "
        << "ignore_modules_getaddrinfo_test can prove IgnoreModulesList "
        << "suppresses them.";

    // Mark all leaks as reported so VLD exits cleanly.
    VLDMarkAllLeaksAsReported();
}

// Test: Calling getaddrinfo WITHOUT freeaddrinfo produces MORE leaks than
// calling with freeaddrinfo. This proves VLD detects the real leak from
// the missing freeaddrinfo call, not just system DLL false positives.
//
// After the first test, system DLL one-time initialization is complete.
// So any additional leaks in a second getaddrinfo call come from per-call
// allocations that freeaddrinfo would normally clean up.
TEST_F(TestGetAddrInfoLeaks, MissingFreeAddrInfoProducesMoreLeaks)
{
    // Warm up: trigger system DLL loading and mark those leaks.
    {
        struct addrinfo hints;
        struct addrinfo* warmup = NULL;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;
        int rc = getaddrinfo("localhost", "80", &hints, &warmup);
        if (rc == 0 && warmup != NULL)
            freeaddrinfo(warmup);
    }
    VLDMarkAllLeaksAsReported();

    // Baseline: call getaddrinfo WITH freeaddrinfo.
    int leaks_with_free;
    {
        struct addrinfo hints;
        struct addrinfo* result = NULL;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;
        int rc = getaddrinfo("localhost", "80", &hints, &result);
        ASSERT_EQ(0, rc);
        ASSERT_NE(static_cast<struct addrinfo*>(NULL), result);
        freeaddrinfo(result);
        leaks_with_free = static_cast<int>(VLDGetLeaksCount());
    }
    VLDMarkAllLeaksAsReported();

    // Test: call getaddrinfo WITHOUT freeaddrinfo.
    int leaks_without_free;
    {
        struct addrinfo hints;
        struct addrinfo* leaked_result = NULL;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;
        int rc = getaddrinfo("localhost", "80", &hints, &leaked_result);
        ASSERT_EQ(0, rc);
        ASSERT_NE(static_cast<struct addrinfo*>(NULL), leaked_result);
        // Intentionally NOT calling freeaddrinfo(leaked_result).
        leaks_without_free = static_cast<int>(VLDGetLeaksCount());
    }

    EXPECT_GT(leaks_without_free, leaks_with_free)
        << "Expected more leaks when freeaddrinfo is NOT called. "
        << "Leaks with freeaddrinfo: " << leaks_with_free
        << ", leaks without freeaddrinfo: " << leaks_without_free
        << ". VLD should detect the missing freeaddrinfo as an "
        << "additional tracked allocation.";

    VLDMarkAllLeaksAsReported();
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    int res = RUN_ALL_TESTS();
    VLDMarkAllLeaksAsReported();
    return res;
}
