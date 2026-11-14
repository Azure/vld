// review_report_options_race.cpp - PR #74 review comment on vld.cpp:3674.
//
// VLDSetReportOptions is one operation: when two threads race it, the report
// file VLDGetReportFilename names must be the file the report goes to.

#include "review_common.h"
#include "vld.h"
#include <stdio.h>

namespace
{
const int ROUNDS = 1000;
// Only the last calls of a round decide which file is current, so each round
// is one call per thread, started together.
const int SETS_PER_THREAD = 1;

wchar_t g_paths[2][MAX_PATH];

DWORD WINAPI Setter(LPVOID p)
{
    const wchar_t* path = g_paths[(INT_PTR)p];
    for (int i = 0; i < SETS_PER_THREAD; i++)
    {
        VLDSetReportOptions(VLD_OPT_REPORT_TO_FILE, path);
    }
    return 0;
}

long FileSize(const wchar_t* path)
{
    WIN32_FILE_ATTRIBUTE_DATA data;
    if (!GetFileAttributesExW(path, GetFileExInfoStandard, &data))
        return -1;
    return (long)data.nFileSizeLow;
}
}

int main()
{
    chaos::StartWatchdog(120000);
    wchar_t dir[MAX_PATH];
    GetTempPathW(_countof(dir), dir);
    swprintf_s(g_paths[0], L"%sreview74_race_%lu_a.txt", dir, GetCurrentProcessId());
    swprintf_s(g_paths[1], L"%sreview74_race_%lu_b.txt", dir, GetCurrentProcessId());
    VLDMarkAllLeaksAsReported();

    for (int round = 0; round < ROUNDS; round++)
    {
        chaos::SetPhase("racing VLDSetReportOptions");
        chaos::RunThreads(2, Setter);

        chaos::SetPhase("reporting to the file VLD names");
        wchar_t named[MAX_PATH] = L"";
        VLDGetReportFilename(named);
        int index = _wcsicmp(named, g_paths[0]) == 0 ? 0 : _wcsicmp(named, g_paths[1]) == 0 ? 1 : -1;
        chaos::Check(index >= 0, "VLDGetReportFilename names neither file");
        void* leak = HeapAlloc(GetProcessHeap(), 0, 16);
        chaos::Check(leak != NULL, "HeapAlloc failed");
        VLDReportLeaks();
        VLDMarkAllLeaksAsReported();
        HeapFree(GetProcessHeap(), 0, leak);
        // Closes the report file, flushing it.
        VLDSetReportOptions(VLD_OPT_REPORT_TO_STDOUT, NULL);
        char message[512];
        sprintf_s(message, "round %d: VLDGetReportFilename names file %c (%ld bytes) but the report went to file %c (%ld bytes)",
            round, 'a' + index, FileSize(g_paths[index]), 'b' - index, FileSize(g_paths[1 - index]));
        chaos::Check(FileSize(g_paths[index]) > 0 && FileSize(g_paths[1 - index]) == 0, message);
    }

    DeleteFileW(g_paths[0]);
    DeleteFileW(g_paths[1]);
    chaos::Pass();
    return 0;
}
