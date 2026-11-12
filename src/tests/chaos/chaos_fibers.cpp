// chaos_fibers.cpp - allocations made on fibers.
//
// Each thread runs several fibers that allocate, reallocate and free, and
// switch to each other between operations, so call stacks are captured on
// fiber stacks and blocks are freed by a different fiber than allocated
// them. VLD must count each thread's kept blocks exactly.

#include "chaos_common.h"
#include "vld.h"
#include <stdlib.h>

namespace
{
const int THREADS = 4;
const int FIBERS = 6;
const int ROUNDS = 300;
const int SLOTS = 16;

struct ThreadState
{
    DWORD id;
    LPVOID main;
    LPVOID fibers[FIBERS];
    int done;
    void* slots[SLOTS];
    int kept;
};
ThreadState g_threads[THREADS];

struct FiberArg
{
    ThreadState* state;
    int index;
};
FiberArg g_args[THREADS][FIBERS];

void Next(ThreadState* state, int index)
{
    SwitchToFiber(state->fibers[(index + 1) % FIBERS]);
}

VOID CALLBACK FiberProc(LPVOID p)
{
    FiberArg* arg = (FiberArg*)p;
    ThreadState* state = arg->state;
    chaos::Random random(arg->index * 389 + (unsigned)state->id);
    for (int round = 0; round < ROUNDS; round++)
    {
        int slot = random.Below(SLOTS);
        switch (random.Below(3))
        {
        case 0:
            free(state->slots[slot]);
            state->slots[slot] = malloc(8 + random.Below(500));
            break;
        case 1:
            state->slots[slot] = realloc(state->slots[slot], 8 + random.Below(5000));
            break;
        default:
            chaos::Check(malloc(32) != NULL, "malloc failed");
            state->kept++;
            break;
        }
        Next(state, arg->index);
    }
    state->done++;
    // A finished fiber keeps passing control on until every fiber is done.
    for (;;)
    {
        if (state->done == FIBERS)
            SwitchToFiber(state->main);
        Next(state, arg->index);
    }
}

DWORD WINAPI Worker(LPVOID p)
{
    ThreadState* state = &g_threads[(INT_PTR)p];
    state->id = GetCurrentThreadId();
    state->main = ConvertThreadToFiber(NULL);
    chaos::Check(state->main != NULL, "ConvertThreadToFiber failed");
    for (int i = 0; i < FIBERS; i++)
    {
        g_args[(INT_PTR)p][i].state = state;
        g_args[(INT_PTR)p][i].index = i;
        state->fibers[i] = CreateFiber(64 * 1024, FiberProc, &g_args[(INT_PTR)p][i]);
        chaos::Check(state->fibers[i] != NULL, "CreateFiber failed");
    }
    SwitchToFiber(state->fibers[0]);
    for (int i = 0; i < FIBERS; i++)
    {
        DeleteFiber(state->fibers[i]);
    }
    for (int i = 0; i < SLOTS; i++)
    {
        free(state->slots[i]);
        state->slots[i] = NULL;
    }
    ConvertFiberToThread();
    return 0;
}
}

int main()
{
    chaos::StartWatchdog(20000);
    VLDMarkAllLeaksAsReported();

    chaos::SetPhase("fibers allocating and switching");
    chaos::RunThreads(THREADS, Worker);

    chaos::SetPhase("counting");
    for (int i = 0; i < THREADS; i++)
    {
        UINT counted = VLDGetThreadLeaksCount(g_threads[i].id);
        if (counted != (UINT)g_threads[i].kept)
        {
            char message[128];
            sprintf_s(message, "thread %d kept %d blocks but VLD counts %u", i, g_threads[i].kept, counted);
            chaos::Fail(message);
        }
    }

    VLDMarkAllLeaksAsReported();
    chaos::Pass();
    return 0;
}
