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
#include <cstdio>
#include <cstdlib>
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

    printf("[DIAG] Build config: "
#ifdef _DEBUG
        "Debug"
#elif defined(NDEBUG)
        "NDEBUG (optimized)"
#else
        "Unknown"
#endif
        "\n");
    printf("[DIAG] VLDGetLeaksCount after getaddrinfo+freeaddrinfo: %d\n", leaks);
    printf("[DIAG] VLDGetOptions: 0x%08X\n", VLDGetOptions());

    // Dump any leaks VLD sees so we can inspect them in CI logs
    if (leaks > 0)
    {
        printf("[DIAG] VLD reports %d leak(s). Reporting them:\n", leaks);
        VLDReportLeaks();
    }
    else
    {
        printf("[DIAG] VLD reports 0 leaks. Trying a known malloc leak to verify VLD is active...\n");
        volatile void* test_leak = malloc(42);
        (void)test_leak;
        int after_malloc = static_cast<int>(VLDGetLeaksCount());
        printf("[DIAG] After deliberate malloc(42) without free: VLDGetLeaksCount = %d\n", after_malloc);
        if (after_malloc > 0)
        {
            printf("[DIAG] VLD IS active (tracks CRT malloc), but did NOT see getaddrinfo allocs.\n");
            VLDReportLeaks();
            free((void*)test_leak);
        }
        else
        {
            printf("[DIAG] VLD did NOT track even malloc - VLD may be inactive in this config.\n");
        }
        VLDMarkAllLeaksAsReported();
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
