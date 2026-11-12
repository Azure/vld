// chaos_cotaskmem.cpp - COM task memory from many threads at once, starting
// with a race on the first CoGetMalloc, with exact leak counts.
//
// VLD hooks CoGetMalloc and wraps the returned IMalloc, and hooks
// CoTaskMemAlloc and CoTaskMemRealloc. Threads that have not initialized COM
// race to get the task allocator, then allocate, reallocate (including to
// and from zero bytes), free through IMalloc and through CoTaskMemFree, and
// leak a known number of blocks.

#include "chaos_common.h"
#include <objbase.h>
#include "vld.h"

#pragma comment(lib, "ole32.lib")

namespace
{
const int THREADS = 8;
const int ROUNDS = 400;
volatile LONG g_go = 0;
volatile LONG g_leaked = 0;

DWORD WINAPI Worker(LPVOID p)
{
    chaos::Random random((unsigned)(INT_PTR)p * 37 + 13);
    while (!InterlockedCompareExchange(&g_go, 0, 0))
    {
        YieldProcessor();
    }
    IMalloc* malloc = NULL;
    chaos::Check(SUCCEEDED(CoGetMalloc(1, &malloc)) && malloc != NULL, "CoGetMalloc failed");
    for (int round = 0; round < ROUNDS; round++)
    {
        void* block = NULL;
        switch (random.Below(4))
        {
        case 0:
            block = CoTaskMemAlloc(1 + random.Below(300));
            break;
        case 1:
            block = malloc->Alloc(1 + random.Below(300));
            break;
        case 2:
            block = CoTaskMemRealloc(NULL, 1 + random.Below(300));
            break;
        default:
            block = malloc->Realloc(NULL, 1 + random.Below(300));
            break;
        }
        chaos::Check(block != NULL, "a COM task allocation failed");
        if (random.Below(2))
        {
            void* moved = random.Below(2) ? CoTaskMemRealloc(block, 1 + random.Below(2000)) : malloc->Realloc(block, 1 + random.Below(2000));
            chaos::Check(moved != NULL, "a COM task reallocation failed");
            block = moved;
        }
        if (random.Below(20) == 0)
        {
            InterlockedIncrement(&g_leaked);
            continue;
        }
        switch (random.Below(3))
        {
        case 0:
            CoTaskMemFree(block);
            break;
        case 1:
            malloc->Free(block);
            break;
        default:
            // Reallocating to zero bytes frees the block.
            chaos::Check(CoTaskMemRealloc(block, 0) == NULL, "CoTaskMemRealloc to zero bytes returned a block");
            break;
        }
    }
    malloc->Release();
    return 0;
}
}

int main()
{
    chaos::StartWatchdog(25000);
    SIZE_T base = VLDGetLeaksCount();

    chaos::SetPhase("racing first CoGetMalloc, then COM task memory churn");
    HANDLE threads[THREADS];
    for (int i = 0; i < THREADS; i++)
    {
        threads[i] = CreateThread(NULL, 0, Worker, (LPVOID)(INT_PTR)i, 0, NULL);
        chaos::Check(threads[i] != NULL, "CreateThread failed");
    }
    InterlockedExchange(&g_go, 1);
    WaitForMultipleObjects(THREADS, threads, TRUE, INFINITE);
    for (int i = 0; i < THREADS; i++)
    {
        CloseHandle(threads[i]);
    }

    chaos::SetPhase("counting");
    SIZE_T counted = VLDGetLeaksCount() - base;
    if (counted != (SIZE_T)g_leaked)
    {
        char message[96];
        sprintf_s(message, "leaked %ld COM task blocks but VLD counts %zu", g_leaked, counted);
        chaos::Fail(message);
    }
    VLDMarkAllLeaksAsReported();
    chaos::Pass();
    return 0;
}
