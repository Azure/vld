// chaos_crt_alloc_hook.cpp - a debug CRT allocation hook that allocates and
// vetoes allocations, from several threads.
//
// The hook runs inside the debug CRT's malloc, before VLD sees the heap
// allocation. It allocates itself (guarded against recursion), leaks some of
// those blocks, and refuses every seventh allocation so that malloc returns
// NULL. VLD must count exactly the blocks that were allocated and leaked. In
// a release CRT the hook is not called, and only the plain leaks count.

#include "chaos_common.h"
#include <crtdbg.h>
#include <stdlib.h>
#include "vld.h"

namespace
{
const int THREADS = 6;
const int ROUNDS = 600;
volatile LONG g_leaked = 0;
volatile LONG g_vetoed = 0;
volatile LONG g_calls = 0;
__declspec(thread) int t_inHook = 0;
__declspec(thread) void* t_moving = NULL;
__declspec(thread) void* t_tracked = NULL;

int __cdecl AllocHook(int type, void*, size_t, int blockUse, long, const unsigned char*, int)
{
    if (type != _HOOK_ALLOC || blockUse != _NORMAL_BLOCK || t_inHook)
        return TRUE;
    t_inHook = 1;
    LONG call = InterlockedIncrement(&g_calls);
    void* inner = malloc(24);
    if (inner != NULL)
    {
        if (call % 11 == 0)
            InterlockedIncrement(&g_leaked);
        else
            free(inner);
    }
    // A block that the hook keeps moving; each thread leaks it at the end.
    void* moved = realloc(t_moving, 16 + (call % 5) * 200);
    if (moved != NULL)
        t_moving = moved;

    // Raw heap blocks from inside the hook, while the CRT allocation that
    // called it is in progress.
    HANDLE heap = GetProcessHeap();
    switch (call % 3)
    {
    case 0:
    {
        // Allocated, then moved.
        void* raw = HeapAlloc(heap, 0, 32);
        void* grown = raw != NULL ? HeapReAlloc(heap, 0, raw, 4096) : NULL;
        if (grown != NULL)
        {
            if (call % 4 == 0)
                InterlockedIncrement(&g_leaked);
            else
                HeapFree(heap, 0, grown);
        }
        else if (raw != NULL)
        {
            HeapFree(heap, 0, raw);
        }
        break;
    }
    case 1:
        // A tracked block shrunk in place, then freed.
        if (t_tracked != NULL)
        {
            HeapReAlloc(heap, HEAP_REALLOC_IN_PLACE_ONLY, t_tracked, 64);
            HeapFree(heap, 0, t_tracked);
            t_tracked = NULL;
        }
        break;
    default:
        // Zero-filled, which looks like the header of a freed CRT block.
        if (call % 4 == 2)
        {
            if (HeapAlloc(heap, HEAP_ZERO_MEMORY, 64) != NULL)
                InterlockedIncrement(&g_leaked);
        }
        break;
    }
    t_inHook = 0;
    if (call % 7 == 0)
    {
        InterlockedIncrement(&g_vetoed);
        return FALSE;
    }
    return TRUE;
}

DWORD WINAPI Worker(LPVOID p)
{
    chaos::Random random((unsigned)(INT_PTR)p * 71 + 3);
    for (int round = 0; round < ROUNDS; round++)
    {
        if (t_tracked == NULL)
            t_tracked = HeapAlloc(GetProcessHeap(), 0, 128);
        void* block = malloc(1 + random.Below(400));
        if (block == NULL)
            continue;
        if (random.Below(2))
        {
            void* moved = realloc(block, 1 + random.Below(800));
            if (moved != NULL)
                block = moved;
        }
        if (random.Below(25) == 0)
            InterlockedIncrement(&g_leaked);
        else
            free(block);
    }
    if (t_moving != NULL)
        InterlockedIncrement(&g_leaked);
    if (t_tracked != NULL)
        HeapFree(GetProcessHeap(), 0, t_tracked);
    return 0;
}
}

int main()
{
    chaos::StartWatchdog(25000);
    SIZE_T base = VLDGetLeaksCount();

    chaos::SetPhase("allocating through a vetoing, allocating CRT alloc hook");
    _CRT_ALLOC_HOOK previous = _CrtSetAllocHook(AllocHook);
    (void)&AllocHook;
    chaos::RunThreads(THREADS, Worker);
    _CrtSetAllocHook(previous);

    chaos::SetPhase("counting");
#ifdef _DEBUG
    chaos::Check(g_vetoed > 0, "the CRT alloc hook never vetoed an allocation");
#endif
    SIZE_T counted = VLDGetLeaksCount() - base;
    if (counted != (SIZE_T)g_leaked)
    {
        char message[128];
        sprintf_s(message, "leaked %ld blocks (%ld allocations vetoed) but VLD counts %zu", g_leaked, g_vetoed, counted);
        chaos::Fail(message);
    }
    VLDMarkAllLeaksAsReported();
    chaos::Pass();
    return 0;
}
