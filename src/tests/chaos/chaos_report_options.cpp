// chaos_report_options.cpp - changing the report destination while VLD reports.
//
// VLDSetReportOptions closes the report file when reporting moves away from
// it, and reopens it when a file is chosen again. Reports made afterwards,
// from any thread or from a report hook, must not touch a closed file, and
// each file must hold only what was reported while it was the destination.
// Also passes NULL to the APIs that take strings.

#include "chaos_common.h"
#include <stdio.h>
#include "vld.h"

namespace
{
const DWORD RUN_MS = 2000;
volatile LONG g_stop = 0;
wchar_t g_fileA[MAX_PATH];
wchar_t g_fileB[MAX_PATH];
volatile LONG g_hookSwitches = 0;

LONGLONG FileSize(const wchar_t* path)
{
    WIN32_FILE_ATTRIBUTE_DATA data;
    if (!GetFileAttributesExW(path, GetFileExInfoStandard, &data))
    {
        return -1;
    }
    return ((LONGLONG)data.nFileSizeHigh << 32) | data.nFileSizeLow;
}

DWORD WINAPI Switcher(LPVOID p)
{
    chaos::Random random((unsigned)(INT_PTR)p * 7 + 1);
    while (!InterlockedCompareExchange(&g_stop, 0, 0))
    {
        switch (random.Below(3))
        {
        case 0:
            VLDSetReportOptions(VLD_OPT_REPORT_TO_FILE, g_fileA);
            break;
        case 1:
            VLDSetReportOptions(VLD_OPT_REPORT_TO_FILE, g_fileB);
            break;
        default:
            VLDSetReportOptions(VLD_OPT_REPORT_TO_DEBUGGER, NULL);
            break;
        }
    }
    return 0;
}

DWORD WINAPI Reporter(LPVOID)
{
    while (!InterlockedCompareExchange(&g_stop, 0, 0))
    {
        VLDReportLeaks();
    }
    return 0;
}

DWORD WINAPI Worker(LPVOID p)
{
    INT_PTR index = (INT_PTR)p;
    if (index < 2)
    {
        return Switcher(p);
    }
    if (index < 5)
    {
        return Reporter(p);
    }
    Sleep(RUN_MS);
    InterlockedExchange(&g_stop, 1);
    return 0;
}

int __cdecl SwitchingHook(int, wchar_t*, int*)
{
    if (InterlockedIncrement(&g_hookSwitches) % 3 == 0)
    {
        VLDSetReportOptions(VLD_OPT_REPORT_TO_FILE, g_fileB);
    }
    else
    {
        VLDSetReportOptions(VLD_OPT_REPORT_TO_DEBUGGER, NULL);
    }
    return 0;
}
}

int main()
{
    chaos::StartWatchdog(25000);
    wchar_t dir[MAX_PATH];
    GetTempPathW(MAX_PATH, dir);
    _snwprintf_s(g_fileA, _TRUNCATE, L"%schaos_report_a_%lu.txt", dir, GetCurrentProcessId());
    _snwprintf_s(g_fileB, _TRUNCATE, L"%schaos_report_b_%lu.txt", dir, GetCurrentProcessId());
    UINT options = VLDGetOptions();
    void* leak = malloc(55);

    chaos::SetPhase("report to a file, then away from it");
    VLDSetReportOptions(VLD_OPT_REPORT_TO_FILE, g_fileA);
    VLDReportLeaks();
    VLDSetReportOptions(VLD_OPT_REPORT_TO_DEBUGGER, NULL);
    LONGLONG sizeA = FileSize(g_fileA);
    chaos::Check(sizeA > 0, "the report file is empty");
    VLDReportLeaks();
    VLDReportLeaks();
    chaos::Check(FileSize(g_fileA) == sizeA, "a report went to the file after reporting moved away from it");
    {
        // A file opened now can reuse the closed report file's FILE object.
        wchar_t mine[MAX_PATH];
        _snwprintf_s(mine, _TRUNCATE, L"%schaos_report_mine_%lu.txt", dir, GetCurrentProcessId());
        FILE* file = NULL;
        chaos::Check(_wfopen_s(&file, mine, L"w") == 0 && file != NULL, "could not open a file");
        VLDReportLeaks();
        fclose(file);
        LONGLONG mineSize = FileSize(mine);
        DeleteFileW(mine);
        chaos::Check(mineSize == 0, "a report went into a file the application opened");
    }

    chaos::SetPhase("NULL strings");
    VLDSetReportOptions(VLD_OPT_REPORT_TO_FILE, NULL);
    VLDReportLeaks();
    VLDSetModulesList(NULL, FALSE);
    wchar_t modules[64];
    VLDGetModulesList(modules, _countof(modules));
    chaos::Check(modules[0] == L'\0', "VLDSetModulesList(NULL) left a module list");

    chaos::SetPhase("a report hook that changes the report destination");
    VLDSetReportHook(VLD_RPTHOOK_INSTALL, SwitchingHook);
    VLDReportLeaks();
    VLDReportLeaks();
    VLDSetReportHook(VLD_RPTHOOK_REMOVE, SwitchingHook);

    chaos::SetPhase("switching the destination while threads report");
    chaos::RunThreads(6, Worker);

    chaos::SetPhase("cleanup");
    VLDSetReportOptions(VLD_OPT_REPORT_TO_STDOUT, NULL);
    VLDSetOptions(options, 256, 64);
    free(leak);
    DeleteFileW(g_fileA);
    DeleteFileW(g_fileB);
    chaos::Pass();
    return 0;
}