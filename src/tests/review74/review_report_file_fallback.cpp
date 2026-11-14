// review_report_file_fallback.cpp - PR #74 review comment on vld.cpp:3728.
//
// With the report going to a file only, a file that cannot be opened must not
// silence the report: the warning says it goes to the debugger, and it must.
// The parent runs the child as its debugger and collects what it outputs.

#include "review_common.h"
#include "vld.h"

namespace
{
int Child()
{
    VLDMarkAllLeaksAsReported();
    VLDSetReportOptions(VLD_OPT_REPORT_TO_FILE, L"Z:\\review74\\no such directory\\report.txt");
    void* leak = HeapAlloc(GetProcessHeap(), 0, 40);
    if (leak == NULL)
        return 2;
    memcpy(leak, "REVIEW74-MARKER", 16);
    VLDReportLeaks();
    VLDMarkAllLeaksAsReported();
    return 0;
}
}

int main()
{
    if (review::ChildMode() != NULL)
        return Child();

    chaos::StartWatchdog(60000);
    chaos::SetPhase("running the child under this process as its debugger");
    static wchar_t text[256 * 1024];
    DWORD exitCode = review::RunChildUnderDebugger(L"report", text, _countof(text), 50000);
    chaos::Check(exitCode == 0, "the child failed");
    chaos::Check(wcsstr(text, L"Couldn't open report file") != NULL,
        "the debugger did not get the warning that the report file could not be opened");
    chaos::Check(wcsstr(text, L"REVIEW74") != NULL, "the debugger did not get the leak report");
    chaos::Pass();
    return 0;
}
