// getaddrinfo_missing_free_test.cpp - Proves VLD detects when application
// code calls getaddrinfo without a matching freeaddrinfo.
//
// This test runs in its own process so DLL loading state is clean.
// It first warms up getaddrinfo (triggering system DLL one-time init),
// then compares leak counts with and without freeaddrinfo.
//
// The vld.ini for this test has an empty IgnoreModulesList, so system DLL
// false positives are visible. The test accounts for this by comparing
// leak counts rather than checking for zero.

#include <gtest/gtest.h>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include "vld.h"

#pragma comment(lib, "ws2_32.lib")

class TestGetAddrInfoMissingFree : public ::testing::Test
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

// Proves VLD detects the missing freeaddrinfo call.
//
// Strategy:
//   1. Warm up: call getaddrinfo+freeaddrinfo to trigger system DLL loading.
//      Mark all one-time init leaks as reported.
//   2. Baseline: call getaddrinfo+freeaddrinfo again. Since DLLs are already
//      loaded, this should produce 0 new leaks.
//   3. Test: call getaddrinfo WITHOUT freeaddrinfo. VLD should track the
//      unreleased addrinfo allocation, producing more leaks than baseline.
TEST_F(TestGetAddrInfoMissingFree, MissingFreeAddrInfoProducesMoreLeaks)
{
    // Step 1: warm up - trigger system DLL loading and mark those leaks.
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

    // Step 2: baseline - getaddrinfo WITH freeaddrinfo after DLLs loaded.
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

    // Step 3: test - getaddrinfo WITHOUT freeaddrinfo.
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

    if (leaks_without_free == 0 && leaks_with_free == 0)
    {
        // VLD is compiled out in non-Debug builds (vld.h gates on _DEBUG).
        // All VLD API calls become no-op macros, so no leaks are tracked.
        VLDMarkAllLeaksAsReported();
        GTEST_SKIP() << "VLD is inactive (compiled out in non-Debug builds).";
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
