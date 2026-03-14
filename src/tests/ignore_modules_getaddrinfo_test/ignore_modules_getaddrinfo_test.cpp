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
// getaddrinfo+freeaddrinfo usage remains correct, and that REAL leaks
// (forgetting to call freeaddrinfo) are still reported.

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
// Without IgnoreModulesList, getaddrinfo triggers one-time initialization
// of namespace providers inside WS2_32.dll (via WSAEnumNameSpaceProvidersW
// and freeaddrinfo internal paths). This causes 40+ leak blocks from:
//   - RPCRT4.dll (majority): RPC binding setup, buffer allocation, context
//     handles for namespace provider communication
//   - KERNELBASE.dll: LoadLibraryExW loading namespace provider DLLs
//   - mswsock.dll: Winsock namespace provider internal state
//   - DNSAPI.dll: DNS resolver initialization
//   - Unresolved namespace provider DLLs (e.g., NLAapi.dll)
// These are OS-internal one-time allocations freed at process exit, not
// application leaks. Sizes vary (8 to 4172 bytes). The exact count and
// DLLs involved depend on the OS version and installed providers.
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

// Test: A real CRT leak (malloc without free) from user code must still be
// reported even when IgnoreModulesList is configured and getaddrinfo has
// been called. This is the critical safety check: IgnoreModulesList must
// NOT suppress real application leaks.
//
// Note: VLD cannot detect a missing freeaddrinfo call because WS2_32.dll
// allocates the addrinfo result via native HeapAlloc (not CRT malloc),
// which VLD does not track. However, CRT allocations from user code (the
// vast majority of application memory) ARE tracked, and IgnoreModulesList
// must not interfere with those.
TEST_F(TestIgnoreModulesGetAddrInfo, UserCodeLeaksAreStillReportedWithIgnoreModules)
{
    // Trigger system DLL loading via getaddrinfo first (warm up).
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

    // Clear any leaks from warmup.
    VLDMarkAllLeaksAsReported();

    int leaks_before = static_cast<int>(VLDGetLeaksCount());
    ASSERT_EQ(0, leaks_before);

    // Simulate a real application bug: allocate memory and forget to free it.
    // This allocation comes from the test exe (not from any ignored module),
    // so VLD must report it.
    volatile void* leaked = malloc(128);
    ASSERT_NE(static_cast<volatile void*>(NULL), leaked);

    int leaks = static_cast<int>(VLDGetLeaksCount());
    EXPECT_EQ(1, leaks)
        << "Expected exactly 1 leak from malloc without free, but VLD "
        << "reported " << leaks << ". IgnoreModulesList must NOT suppress "
        << "real application leaks from user code.";

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
