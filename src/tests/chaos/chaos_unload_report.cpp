// chaos_unload_report.cpp - reporting and resolving leaks whose call stacks
// run through a DLL that other threads keep unloading and reloading.
//
// Loaders leak blocks from chaos_helper_dll and unload it, so the call stacks
// point into code that is gone, or into a newer copy of the DLL at the same
// or another address. Reporters report, count and resolve the whole time.
// Afterwards every leaked block must still be counted.

#include "chaos_common.h"
#include "vld.h"

namespace
{
const DWORD RUN_MS = 3000;
const int LOADERS = 4;
const int REPORTERS = 3;
const int MAX_LEAKS = 4000;
volatile LONG g_stop = 0;
volatile LONG g_leaked = 0;
void* volatile g_blocks[MAX_LEAKS];
volatile LONG g_blockThreads[MAX_LEAKS];

typedef void* (*HelperMalloc_t)(size_t);

int __cdecl SwallowHook(int, wchar_t*, int*)
{
    return 1;
}

// Addresses of the blocks a diagnostic report lists.
void* g_reported[MAX_LEAKS * 2];
volatile LONG g_reportedCount = 0;

int __cdecl CollectHook(int, wchar_t* message, int*)
{
    const wchar_t* at = wcsstr(message, L"---------- Block ");
    if (at != NULL)
    {
        at = wcsstr(at, L" at 0x");
        if (at != NULL)
        {
            ULONG_PTR address = (ULONG_PTR)_wcstoui64(at + 6, NULL, 16);
            LONG slot = InterlockedIncrement(&g_reportedCount) - 1;
            if (slot < (LONG)_countof(g_reported))
                g_reported[slot] = (void*)address;
        }
    }
    return 1;
}

// Lists the leaked blocks that VLD does not report, to find out why it misses them.
void DescribeMissingBlocks()
{
    VLDSetReportHook(VLD_RPTHOOK_REMOVE, SwallowHook);
    VLDSetReportHook(VLD_RPTHOOK_INSTALL, CollectHook);
    VLDReportLeaks();
    VLDSetReportHook(VLD_RPTHOOK_REMOVE, CollectHook);
    LONG reported = min(g_reportedCount, (LONG)_countof(g_reported));
    char line[256];
    sprintf_s(line, "diagnostic report lists %ld blocks\n", reported);
    chaos::Write(line);
    int missing = 0;
    for (LONG i = 0; i < g_leaked && i < MAX_LEAKS; i++)
    {
        bool found = false;
        for (LONG j = 0; j < reported && !found; j++)
            found = (ULONG_PTR)g_blocks[i] - (ULONG_PTR)g_reported[j] <= 64;
        if (found)
            continue;
        if (++missing > 40)
            continue;
        const unsigned char* bytes = (const unsigned char*)g_blocks[i];
        int length = sprintf_s(line, "missing #%ld thread %ld at %p:", i, g_blockThreads[i], bytes);
        __try
        {
            for (int b = 0; b < 16; b++)
                length += sprintf_s(line + length, _countof(line) - length, " %02X", bytes[b]);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            length += sprintf_s(line + length, _countof(line) - length, " (not readable)");
        }
        sprintf_s(line + length, _countof(line) - length, "\n");
        chaos::Write(line);
    }
    sprintf_s(line, "%d blocks missing from the diagnostic report\n", missing);
    chaos::Write(line);
}

DWORD WINAPI Loader(LPVOID p)
{
    chaos::Random random((unsigned)(INT_PTR)p * 53 + 11);
    while (!InterlockedCompareExchange(&g_stop, 0, 0))
    {
        HMODULE helper = LoadLibraryW(L"chaos_helper_dll.dll");
        chaos::Check(helper != NULL, "LoadLibrary(chaos_helper_dll) failed");
        HelperMalloc_t helperMalloc = (HelperMalloc_t)GetProcAddress(helper, "HelperMalloc");
        chaos::Check(helperMalloc != NULL, "HelperMalloc not found");
        int count = 1 + random.Below(4);
        for (int i = 0; i < count; i++)
        {
            LONG index = InterlockedIncrement(&g_leaked);
            if (index > MAX_LEAKS)
            {
                InterlockedDecrement(&g_leaked);
                break;
            }
            void* block = helperMalloc(16 + random.Below(200));
            chaos::Check(block != NULL, "HelperMalloc failed");
            g_blocks[index - 1] = block;
            g_blockThreads[index - 1] = (LONG)(INT_PTR)p;
        }
        FreeLibrary(helper);
    }
    return 0;
}

DWORD WINAPI Reporter(LPVOID p)
{
    chaos::Random random((unsigned)(INT_PTR)p * 29 + 5);
    while (!InterlockedCompareExchange(&g_stop, 0, 0))
    {
        switch (random.Below(4))
        {
        case 0:
            VLDReportLeaks();
            break;
        case 1:
            VLDGetLeaksCount();
            break;
        case 2:
            VLDResolveCallstacks();
            break;
        default:
            VLDRefreshModules();
            break;
        }
    }
    return 0;
}
}

int main()
{
    chaos::StartWatchdog(25000);
    VLDSetReportHook(VLD_RPTHOOK_INSTALL, SwallowHook);
    VLDMarkAllLeaksAsReported();

    chaos::SetPhase("unloading while reporting");
    HANDLE threads[LOADERS + REPORTERS];
    for (int i = 0; i < LOADERS + REPORTERS; i++)
    {
        threads[i] = CreateThread(NULL, 0, i < LOADERS ? Loader : Reporter, (LPVOID)(INT_PTR)i, 0, NULL);
        chaos::Check(threads[i] != NULL, "CreateThread failed");
    }
    Sleep(RUN_MS);
    InterlockedExchange(&g_stop, 1);
    WaitForMultipleObjects(LOADERS + REPORTERS, threads, TRUE, INFINITE);

    chaos::SetPhase("counting leaks from the unloaded DLL");
    UINT counted = VLDGetLeaksCount();
    if (counted != (UINT)g_leaked)
    {
        DescribeMissingBlocks();
        char message[96];
        sprintf_s(message, "leaked %ld blocks but VLD counts %u", g_leaked, counted);
        chaos::Fail(message);
    }
    chaos::Check(VLDReportLeaks() == counted, "VLDReportLeaks disagrees with VLDGetLeaksCount");

    chaos::SetPhase("cleanup");
    VLDMarkAllLeaksAsReported();
    for (int i = 0; i < LOADERS + REPORTERS; i++)
    {
        CloseHandle(threads[i]);
    }
    VLDSetReportHook(VLD_RPTHOOK_REMOVE, SwallowHook);
    chaos::Pass();
    return 0;
}
