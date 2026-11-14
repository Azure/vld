// chaos_long_frame.cpp - a call stack frame longer than VLD's report line.
//
// The leak report line for a frame is "<file> (<line>): <module>!<function>()".
// With a long source path, module name and function name it exceeds
// MAXREPORTLENGTH. VLD must truncate it without writing past its line buffer
// and keep every following frame on its own line.

#include "chaos_common.h"
#include <wchar.h>
#include "vld.h"

namespace
{
typedef void* (*LongNameAlloc_t)(size_t);

wchar_t g_report[64 * 1024];
size_t g_reportLength = 0;

int __cdecl CaptureReport(int, wchar_t* message, int*)
{
    size_t length = wcslen(message);
    if (g_reportLength + length < _countof(g_report))
    {
        wmemcpy(g_report + g_reportLength, message, length);
        g_reportLength += length;
        g_report[g_reportLength] = L'\0';
    }
    return 1;
}
}

int main()
{
    chaos::StartWatchdog(20000);
    HMODULE dll = LoadLibraryW(L"chaos_longname_012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789end.dll");
    chaos::Check(dll != NULL, "LoadLibrary of the long-named helper failed");
    LongNameAlloc_t alloc = (LongNameAlloc_t)GetProcAddress(dll, "LongName_abcdefghijabcdefghijabcdefghijabcdefghijabcdefghijabcdefghijabcdefghijabcdefghijabcdefghijabcdefghijabcdefghijabcdefghijabcdefghijabcdefghijabcdefghijabcdefghijabcdefghijabcdefghijabcdefghijabcdefghijabcdefghijabcdefghijabcdefghijabcdefghijxyz");
    chaos::Check(alloc != NULL, "GetProcAddress failed");

    void* leak = alloc(24);
    chaos::SetPhase("leak report with an over-long frame line");
    VLDSetReportHook(VLD_RPTHOOK_INSTALL, CaptureReport);
    VLDReportLeaks();
    VLDSetReportHook(VLD_RPTHOOK_REMOVE, CaptureReport);
    free(leak);

    const wchar_t* longFrame = wcsstr(g_report, L"LongName_");
    chaos::Check(longFrame != NULL, "the long frame is missing from the report");
    const wchar_t* mainFrame = wcsstr(longFrame, L"chaos_long_frame.exe!main()");
    chaos::Check(mainFrame != NULL, "the main frame is missing from the report");
    const wchar_t* lineEnd = wcschr(longFrame, L'\n');
    chaos::Check(lineEnd != NULL && lineEnd < mainFrame, "the frame after the over-long line is not on its own line");
    chaos::Pass();
    return 0;
}