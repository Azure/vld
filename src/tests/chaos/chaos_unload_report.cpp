// chaos_unload_report.cpp - reporting and resolving leaks whose call stacks
// run through a DLL that other threads keep unloading and reloading.
//
// Loaders leak blocks from chaos_helper_dll and unload it, so the call stacks
// point into code that is gone, or into a newer copy of the DLL at the same
// or another address. Reporters report, count and resolve the whole time.
// Afterwards every leaked block must still be counted.

#include "chaos_ledger.h"

namespace
{
const DWORD RUN_MS = 3000;
const int LOADERS = 4;
const int REPORTERS = 3;
const int MAX_LEAKS = 4000;
volatile LONG g_stop = 0;
volatile LONG g_leaked = 0;

typedef void* (*HelperMalloc_t)(size_t);
volatile LONG g_loads = 0;
volatile LONG g_unpatchedLoads = 0;

// Counts the helper's imports, other than those of VLD itself, that VLD
// patched: zero means its allocations bypass VLD. Recorded with every leak to
// tell a missed block of an unpatched copy from one VLD skipped.
LONG CountPatchedImports(HMODULE module)
{
    HMODULE vld = NULL;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCWSTR)&VLDReportLeaks, &vld);
    char vldName[MAX_PATH] = { 0 };
    GetModuleFileNameA(vld, vldName, MAX_PATH);
    const char* vldBase = strrchr(vldName, '\\') != NULL ? strrchr(vldName, '\\') + 1 : vldName;
    BYTE* base = (BYTE*)module;
    IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)(base + ((IMAGE_DOS_HEADER*)base)->e_lfanew);
    IMAGE_DATA_DIRECTORY dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    LONG patched = 0;
    for (IMAGE_IMPORT_DESCRIPTOR* d = (IMAGE_IMPORT_DESCRIPTOR*)(base + dir.VirtualAddress); dir.VirtualAddress != 0 && d->Name != 0; d++)
    {
        if (_stricmp((const char*)(base + d->Name), vldBase) == 0)
            continue;
        for (IMAGE_THUNK_DATA* t = (IMAGE_THUNK_DATA*)(base + d->FirstThunk); t->u1.Function != 0; t++)
        {
            HMODULE target = NULL;
            if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                    (LPCWSTR)t->u1.Function, &target) && target == vld)
                patched++;
        }
    }
    return patched;
}

int __cdecl SwallowHook(int, wchar_t*, int*)
{
    return 1;
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
        // Tag: loader index, plus 100 if this copy of the helper is patched.
        bool patched = CountPatchedImports(helper) > 0;
        InterlockedIncrement(&g_loads);
        if (!patched)
            InterlockedIncrement(&g_unpatchedLoads);
        LONG tag = (LONG)(INT_PTR)p + (patched ? 100 : 0);
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
            chaos::RecordLeak(block, tag);
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
        VLDSetReportHook(VLD_RPTHOOK_REMOVE, SwallowHook);
        chaos::DescribeMissingLeaks();
        char loads[96];
        sprintf_s(loads, "%ld of %ld helper loads were not patched by VLD\n", g_unpatchedLoads, g_loads);
        chaos::Write(loads);
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
