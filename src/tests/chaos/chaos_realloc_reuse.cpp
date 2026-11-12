// chaos_realloc_reuse.cpp - a realloc that moves a block must not forget a
// block another thread got at the old address.
//
// The heap frees the old block inside the real realloc, before VLD forgets
// the old address, so another thread can be handed that address first. Movers
// reallocate blocks that VLD does not track, or tracked ones while VLD is
// disabled for the thread, through every reallocation API to a larger size
// so they move; leakers keep every block they allocate. Each leaker's count
// must match exactly.

#include "chaos_common.h"
#include "vld.h"
#include <objbase.h>
#include <malloc.h>

#pragma comment(lib, "ole32.lib")

namespace
{
const DWORD RUN_MS = 3000;
const int MOVERS = 8;
const int LEAKERS = 8;
const int MAX_KEPT = 3000;
const size_t SIZE = 48;
volatile LONG g_stop = 0;

struct Leaker
{
    HANDLE thread;
    DWORD id;
    int kept;
    void* blocks[MAX_KEPT];
};
Leaker g_leakers[LEAKERS];

enum Api { CRT_REALLOC, CRT_RECALLOC, HEAP_REALLOC, COTASKMEM_REALLOC, API_COUNT };

void* Allocate(Api api)
{
    switch (api)
    {
    case CRT_REALLOC: return malloc(SIZE);
    case CRT_RECALLOC: return calloc(1, SIZE);
    case HEAP_REALLOC: return HeapAlloc(GetProcessHeap(), 0, SIZE);
    default: return CoTaskMemAlloc(SIZE);
    }
}

void* Reallocate(Api api, void* block, size_t size)
{
    switch (api)
    {
    case CRT_REALLOC: return realloc(block, size);
    case CRT_RECALLOC: return _recalloc(block, 1, size);
    case HEAP_REALLOC: return HeapReAlloc(GetProcessHeap(), 0, block, size);
    default: return CoTaskMemRealloc(block, size);
    }
}

void Free(Api api, void* block)
{
    switch (api)
    {
    case CRT_REALLOC:
    case CRT_RECALLOC: free(block); break;
    case HEAP_REALLOC: HeapFree(GetProcessHeap(), 0, block); break;
    default: CoTaskMemFree(block); break;
    }
}

DWORD WINAPI Mover(LPVOID p)
{
    chaos::Random random((unsigned)(INT_PTR)p * 31 + 7);
    while (!InterlockedCompareExchange(&g_stop, 0, 0))
    {
        Api api = (Api)random.Below(API_COUNT);
        bool untracked = random.Below(2) == 0;
        if (untracked)
            VLDDisable();
        void* block = Allocate(api);
        if (untracked)
            VLDRestore();
        else
            VLDDisable();
        void* moved = Reallocate(api, block, 4096 + random.Below(4096));
        if (!untracked)
            VLDRestore();
        Free(api, moved != NULL ? moved : block);
    }
    return 0;
}

DWORD WINAPI LeakerThread(LPVOID p)
{
    Leaker* leaker = (Leaker*)p;
    while (!InterlockedCompareExchange(&g_stop, 0, 0) && leaker->kept < MAX_KEPT)
    {
        leaker->blocks[leaker->kept++] = malloc(SIZE);
        if ((leaker->kept & 15) == 0)
            Sleep(0);
    }
    return 0;
}
}

int main()
{
    chaos::StartWatchdog(25000);
    VLDMarkAllLeaksAsReported();

    chaos::SetPhase("movers and leakers");
    HANDLE movers[MOVERS];
    for (int i = 0; i < MOVERS; i++)
    {
        movers[i] = CreateThread(NULL, 0, Mover, (LPVOID)(INT_PTR)i, 0, NULL);
        chaos::Check(movers[i] != NULL, "CreateThread failed");
    }
    for (int i = 0; i < LEAKERS; i++)
    {
        g_leakers[i].thread = CreateThread(NULL, 0, LeakerThread, &g_leakers[i], 0, &g_leakers[i].id);
        chaos::Check(g_leakers[i].thread != NULL, "CreateThread failed");
    }
    Sleep(RUN_MS);
    InterlockedExchange(&g_stop, 1);
    WaitForMultipleObjects(MOVERS, movers, TRUE, INFINITE);
    for (int i = 0; i < LEAKERS; i++)
    {
        WaitForSingleObject(g_leakers[i].thread, INFINITE);
    }

    chaos::SetPhase("counting each leaker's blocks");
    for (int i = 0; i < LEAKERS; i++)
    {
        UINT counted = VLDGetThreadLeaksCount(g_leakers[i].id);
        if (counted != (UINT)g_leakers[i].kept)
        {
            char message[128];
            sprintf_s(message, "leaker %d kept %d blocks but VLD counts %u", i, g_leakers[i].kept, counted);
            chaos::Fail(message);
        }
    }

    chaos::SetPhase("cleanup");
    for (int i = 0; i < LEAKERS; i++)
    {
        for (int j = 0; j < g_leakers[i].kept; j++)
        {
            free(g_leakers[i].blocks[j]);
        }
        CloseHandle(g_leakers[i].thread);
    }
    for (int i = 0; i < MOVERS; i++)
    {
        CloseHandle(movers[i]);
    }
    chaos::Pass();
    return 0;
}
