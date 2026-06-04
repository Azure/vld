// dtor_suppression_test.cpp
//
// Regression test for VLD's destructor-time leak suppression.
//
// Background: when SkipCrtStartupLeaks=yes (default) and IgnoreFunctionsList
// is configured, VLD must suppress matching allocations from the automatic
// leak report produced by ~VisualLeakDetector. The suppression uses dbghelp
// to resolve function names from the allocation's callstack and match them
// against the configured list.
//
// On ARM64, the bundled dbghelp spins under the loader lock at process
// teardown, so VLD installs Arm64TeardownReportScope which disables dbghelp
// symbol resolution for the duration of the destructor. To preserve correct
// suppression behavior under that scope, VLD must pre-populate the suppression
// cache (CallStack::m_status and blockinfo_t::reported) BEFORE the scope is
// activated. This test verifies the pre-population works even when user code
// never calls VLDGetLeaksCount or any other VLD API.
//
// The test allocates from a function listed in IgnoreFunctionsList, then exits.
// It never calls any VLD API. The expected outcome is that VLD's destructor
// reports "No memory leaks detected" - the allocation is correctly suppressed.

#include <windows.h>
#include <stdio.h>
// Including vld.h imports g_vld; that import causes the loader to pull in
// vld_*.dll which then attaches and instruments allocations. Without this
// include the test exe would never load VLD at all.
#include "vld.h"

// Listed in IgnoreFunctionsList in vld.ini. Disable optimization so the function
// frame survives and dbghelp can resolve its name.
#pragma optimize("", off)
__declspec(noinline) void* LeakInIgnoredHelper()
{
    return new char[32];
}
#pragma optimize("", on)

int main()
{
    void* p = LeakInIgnoredHelper();
    (void)p;
    printf("dtor_suppression_test: leak from LeakInIgnoredHelper allocated; relying on VLD destructor to suppress it\n");
    return 0;
}
