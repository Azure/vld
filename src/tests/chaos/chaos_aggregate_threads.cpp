// chaos_aggregate_threads.cpp - duplicate aggregation must respect the thread
// filter and the leaks already marked as reported.
//
// Two threads leak blocks of the same size from the same call site, so with
// AggregateDuplicates on all of them are duplicates of each other. A report
// for one thread must count only that thread's blocks, and blocks marked as
// reported must not be counted again.

#include "chaos_common.h"
#include "vld.h"

namespace
{
HANDLE g_release = NULL;
void* g_blocks[8];

__declspec(noinline) void* LeakFrom()
{
    return malloc(72);
}

struct LeakJob
{
    int first;
    int count;
    HANDLE done;
};

DWORD WINAPI Leaker(LPVOID p)
{
    LeakJob* job = (LeakJob*)p;
    for (int i = 0; i < job->count; i++)
    {
        g_blocks[job->first + i] = LeakFrom();
    }
    SetEvent(job->done);
    WaitForSingleObject(g_release, INFINITE);
    return 0;
}

int __cdecl SwallowHook(int, wchar_t*, int*)
{
    return 1;
}
}

int main()
{
    chaos::StartWatchdog(20000);
    VLDSetReportHook(VLD_RPTHOOK_INSTALL, SwallowHook);
    UINT options = VLDGetOptions();
    VLDSetOptions(options | VLD_OPT_AGGREGATE_DUPLICATES, 256, 64);
    g_release = CreateEventW(NULL, TRUE, FALSE, NULL);

    chaos::SetPhase("two threads leaking duplicates");
    LeakJob jobA = { 0, 3, CreateEventW(NULL, TRUE, FALSE, NULL) };
    LeakJob jobB = { 3, 2, CreateEventW(NULL, TRUE, FALSE, NULL) };
    DWORD idA = 0;
    DWORD idB = 0;
    HANDLE threadA = CreateThread(NULL, 0, Leaker, &jobA, 0, &idA);
    HANDLE threadB = CreateThread(NULL, 0, Leaker, &jobB, 0, &idB);
    chaos::Check(threadA != NULL && threadB != NULL, "CreateThread failed");
    WaitForSingleObject(jobA.done, INFINITE);
    WaitForSingleObject(jobB.done, INFINITE);

    chaos::SetPhase("per-thread counts with aggregation");
    chaos::Check(VLDGetThreadLeaksCount(idA) == 3, "VLDGetThreadLeaksCount(A) is not 3");
    chaos::Check(VLDGetThreadLeaksCount(idB) == 2, "VLDGetThreadLeaksCount(B) is not 2");
    chaos::Check(VLDReportThreadLeaks(idA) == 3, "VLDReportThreadLeaks(A) counted another thread's duplicates");
    chaos::Check(VLDReportThreadLeaks(idB) == 2, "VLDReportThreadLeaks(B) counted another thread's duplicates");

    chaos::SetPhase("reported leaks are not aggregated again");
    VLDMarkThreadLeaksAsReported(idB);
    chaos::Check(VLDReportThreadLeaks(idA) == 3, "VLDReportThreadLeaks(A) changed after B was marked reported");
    VLDMarkThreadLeaksAsReported(idA);
    chaos::Check(VLDReportThreadLeaks(idA) == 0, "VLDReportThreadLeaks(A) reports leaks already marked reported");

    chaos::SetPhase("cleanup");
    SetEvent(g_release);
    WaitForSingleObject(threadA, INFINITE);
    WaitForSingleObject(threadB, INFINITE);
    for (int i = 0; i < 5; i++)
    {
        free(g_blocks[i]);
    }
    VLDSetOptions(options, 256, 64);
    VLDSetReportHook(VLD_RPTHOOK_REMOVE, SwallowHook);
    chaos::Pass();
    return 0;
}
