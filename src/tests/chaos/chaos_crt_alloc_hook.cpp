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
