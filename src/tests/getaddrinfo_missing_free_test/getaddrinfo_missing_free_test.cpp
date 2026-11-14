// getaddrinfo_missing_free_test.cpp - Proves VLD detects when application
// code calls getaddrinfo without a matching freeaddrinfo.
//
// This test runs in its own process so DLL loading state is clean.
// It first warms up getaddrinfo (triggering system DLL one-time init),
// then checks that VLD tracks the block getaddrinfo returns and counts it
// as a leak of this thread when freeaddrinfo is not called.
//
// The vld.ini for this test has an empty IgnoreModulesList, so system DLL
// allocations are tracked too. Leak counts are not compared: the DNS client
// allocates on this thread and on threads of its own at any time.

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
//   2. Baseline: VLD tracks the block getaddrinfo returns.
//   3. Test: call getaddrinfo WITHOUT freeaddrinfo. VLD still tracks the
//      block, as a leak of this thread.
typedef const wchar_t* (*AllocationCallstack_t)(void* alloc, BOOL showInternalFrames);

static AllocationCallstack_t GetAllocationCallstack()
{
    HMODULE vld = NULL;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCWSTR)&VLDMarkAllLeaksAsReported, &vld);
    return vld != NULL ? (AllocationCallstack_t)GetProcAddress(vld, "VldInternalGetAllocationCallstack") : NULL;
}

static struct addrinfo* ResolveLocalhost()
{
    struct addrinfo hints;
    struct addrinfo* result = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    int rc = getaddrinfo("localhost", "80", &hints, &result);
    EXPECT_EQ(0, rc);
    return rc == 0 ? result : NULL;
}

TEST_F(TestGetAddrInfoMissingFree, MissingFreeAddrInfoIsTrackedAsLeak)
{
    AllocationCallstack_t callstack = GetAllocationCallstack();
    ASSERT_NE(static_cast<AllocationCallstack_t>(NULL), callstack);

    // Step 1: warm up - trigger system DLL loading and mark those leaks.
    struct addrinfo* warmup = ResolveLocalhost();
    if (warmup != NULL)
        freeaddrinfo(warmup);
    VLDMarkAllLeaksAsReported();

    // Step 2: baseline - VLD tracks the block getaddrinfo returns.
    for (int round = 0; round < 3; round++)
    {
        struct addrinfo* result = ResolveLocalhost();
        ASSERT_NE(static_cast<struct addrinfo*>(NULL), result);
        EXPECT_NE(static_cast<const wchar_t*>(NULL), callstack(result, FALSE))
            << "VLD does not track the block getaddrinfo returned.";
        freeaddrinfo(result);
        VLDMarkAllLeaksAsReported();
    }

    // Step 3: test - getaddrinfo WITHOUT freeaddrinfo.
    struct addrinfo* leaked_result = ResolveLocalhost();
    ASSERT_NE(static_cast<struct addrinfo*>(NULL), leaked_result);
    // Intentionally NOT calling freeaddrinfo(leaked_result).
    EXPECT_NE(static_cast<const wchar_t*>(NULL), callstack(leaked_result, FALSE))
        << "VLD should detect the missing freeaddrinfo: it does not track the block getaddrinfo returned.";
    EXPECT_GT(VLDGetThreadLeaksCount(GetCurrentThreadId()), 0u)
        << "VLD should count the block getaddrinfo returned as a leak of this thread.";

    VLDMarkAllLeaksAsReported();
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    int res = RUN_ALL_TESTS();
    VLDMarkAllLeaksAsReported();
    return res;
}
