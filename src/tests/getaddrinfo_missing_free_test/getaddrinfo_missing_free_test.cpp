// getaddrinfo_missing_free_test.cpp - Verifies VLD detects user-code leaks
// alongside getaddrinfo usage, while suppressing system DLL false positives.
//
// With improved module detection, VLD only tracks allocations from modules
// that import VLD (user code). System DLL internal allocations during
// getaddrinfo are automatically excluded. This test verifies that:
//   1. getaddrinfo+freeaddrinfo produces 0 leaks (no false positives)
//   2. User-code malloc leaks ARE still detected after getaddrinfo
//
// This test runs in its own process so DLL loading state is clean.

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

// Verifies getaddrinfo+freeaddrinfo produces 0 leaks without IgnoreModulesList.
TEST_F(TestGetAddrInfoMissingFree, GetAddrInfoWithFreeProducesNoLeaks)
{
    // Warm up: trigger system DLL loading.
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

    // After warm-up, getaddrinfo+freeaddrinfo should produce 0 leaks.
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
    }

    int leaks = static_cast<int>(VLDGetLeaksCount());
    EXPECT_EQ(0, leaks)
        << "Expected 0 leaks after getaddrinfo+freeaddrinfo. "
        << "System DLL allocations should be automatically excluded. "
        << "VLD reported " << leaks << " leak(s).";
    VLDMarkAllLeaksAsReported();
}

// Verifies user-code malloc leaks ARE detected after getaddrinfo has been
// called. This is the critical safety check: automatic system DLL exclusion
// must NOT suppress real application leaks from user code.
TEST_F(TestGetAddrInfoMissingFree, UserCodeLeaksDetectedAfterGetAddrInfo)
{
    // Trigger getaddrinfo to load system DLLs.
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

    int leaks_before = static_cast<int>(VLDGetLeaksCount());
    ASSERT_EQ(0, leaks_before);

    // Simulate a real application bug: allocate memory and forget to free it.
    // This allocation comes from the test exe (which imports VLD), so VLD
    // must report it regardless of system DLL exclusion.
    volatile void* leaked = malloc(128);
    ASSERT_NE(static_cast<volatile void*>(NULL), leaked);

    int leaks = static_cast<int>(VLDGetLeaksCount());
    EXPECT_EQ(1, leaks)
        << "Expected exactly 1 leak from malloc without free, but VLD "
        << "reported " << leaks << ". System DLL exclusion must NOT "
        << "suppress real application leaks from user code.";

    // Clean up so VLD exits cleanly.
    VLDMarkAllLeaksAsReported();
    free((void*)leaked);
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    int res = RUN_ALL_TESTS();
    VLDMarkAllLeaksAsReported();
    return res;
}
