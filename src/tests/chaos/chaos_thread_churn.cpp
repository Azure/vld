// chaos_thread_churn.cpp - many short-lived threads whose DLL_THREAD_ATTACH and
// DLL_THREAD_DETACH allocate and free, while other threads load and unload
// DLLs and count leaks.
//
// Thread attach/detach run under the loader lock; leak queries and module
// (un)load notifications take VLD's locks in other orders. Nothing may hang,
// and every block allocated in thread attach is freed in thread detach.

#include "chaos_common.h"
#include "vld.h"

namespace
{
typedef void (*SetThreadChurn_t)(LONG);
typedef LONG (*ThreadChurnLiveBlocks_t)();
const DWORD RUN_MS = 4000;
volatile LONG g_stop = 0;

DWORD WINAPI ShortThread(LPVOID p)
{
    void* block = malloc(16 + ((INT_PTR)p % 64));
    free(block);
    return 0;
}

DWORD WINAPI Spawner(LPVOID p)
{
    chaos::Random random((unsigned)(INT_PTR)p + 11);
    while (!InterlockedCompareExchange(&g_stop, 0, 0))
    {
        HANDLE threads[8];
        int count = 0;
        for (int i = 0; i < 8; i++)
        {
            threads[count] = CreateThread(NULL, 0, ShortThread, (LPVOID)(INT_PTR)random.Next(), 0, NULL);
            if (threads[count] != NULL)
            {
                count++;
            }
        }
        WaitForMultipleObjects(count, threads, TRUE, INFINITE);
        for (int i = 0; i < count; i++)
        {
            CloseHandle(threads[i]);
        }
    }
    return 0;
}

DWORD WINAPI Loader(LPVOID)
{
    const wchar_t* dlls[] = { L"version.dll", L"winmm.dll", L"dbgeng.dll", L"wtsapi32.dll" };
    int i = 0;
    while (!InterlockedCompareExchange(&g_stop, 0, 0))
    {
        HMODULE module = LoadLibraryW(dlls[i++ % _countof(dlls)]);
        if (module != NULL)
        {
            FreeLibrary(module);
        }
    }
    return 0;
}

DWORD WINAPI Counter(LPVOID)
{
    while (!InterlockedCompareExchange(&g_stop, 0, 0))
    {
        VLDGetLeaksCount();
        Sleep(1);
    }
    return 0;
}

DWORD WINAPI Worker(LPVOID p)
{
    INT_PTR index = (INT_PTR)p;
    if (index < 4)
    {
        return Spawner(p);
    }
    if (index < 6)
    {
        return Loader(p);
    }
    if (index < 7)
    {
        return Counter(p);
    }
    Sleep(RUN_MS);
    InterlockedExchange(&g_stop, 1);
    return 0;
}
}

int main()
{
    chaos::StartWatchdog(25000);
    HMODULE helper = LoadLibraryW(L"chaos_helper_dll.dll");
    chaos::Check(helper != NULL, "LoadLibrary(chaos_helper_dll.dll) failed");
    SetThreadChurn_t setThreadChurn = (SetThreadChurn_t)GetProcAddress(helper, "SetThreadChurn");
    chaos::Check(setThreadChurn != NULL, "GetProcAddress(SetThreadChurn) failed");
    ThreadChurnLiveBlocks_t liveBlocks = (ThreadChurnLiveBlocks_t)GetProcAddress(helper, "ThreadChurnLiveBlocks");
    chaos::Check(liveBlocks != NULL, "GetProcAddress(ThreadChurnLiveBlocks) failed");
    SIZE_T base = VLDGetLeaksCount();

    chaos::SetPhase("thread churn with allocating thread attach/detach, DLL loads and leak counts");
    setThreadChurn(1);
    chaos::RunThreads(8, Worker);
    setThreadChurn(0);

    // Threads still alive that attached during the churn (thread pool and
    // loader workers) legitimately hold their attach block.
    chaos::SetPhase("final count");
    for (int attempt = 0; ; attempt++)
    {
        LONG live = liveBlocks();
        SIZE_T leaks = VLDGetLeaksCount();
        if (leaks - base == (SIZE_T)live && liveBlocks() == live)
        {
            break;
        }
        if (attempt == 50)
        {
            char message[128];
            _snprintf_s(message, _TRUNCATE, "%zu leaks left behind by thread churn, %ld blocks held by live threads",
                leaks - base, live);
            chaos::Fail(message);
        }
        Sleep(20);
    }
    // Their attach blocks stay allocated past the exit report.
    VLDMarkAllLeaksAsReported();
    chaos::Pass();
    return 0;
}
