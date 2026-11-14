// chaos_new_handler.cpp - a new handler that allocates, leaks and retries,
// called from inside operator new and malloc allocations that fail.
//
// With _set_new_mode(1) malloc also calls the handler. The handler runs while
// the failing CRT allocation is in progress, before VLD has mapped anything
// for it. It allocates and leaks CRT and heap blocks, asks for a retry once,
// then gives up, so operator new throws and malloc returns NULL. VLD must
// count every leaked block and report it at its own address.

#include "chaos_ledger.h"
#include <malloc.h>
#include <new>
#include <new.h>

namespace
{
const int THREADS = 6;
const int ROUNDS = 300;
volatile LONG g_leaked = 0;
volatile LONG g_handled = 0;
volatile LONG g_thrown = 0;
__declspec(thread) int t_retries = 0;
__declspec(thread) bool t_worker = false;
__declspec(thread) bool t_inHandler = false;

// More than the address space, but small enough that the CRT tries the heap.
#ifdef _WIN64
volatile size_t g_hugeSize = (size_t)1 << 50;
#else
volatile size_t g_hugeSize = 0xFFF00000;
#endif

// Tags: 1 handler malloc, 2 handler new[], 3 handler heap block, 4 worker
// malloc, 5 worker new.
void Leak(void* block, LONG tag)
{
    InterlockedIncrement(&g_leaked);
    chaos::RecordLeak(block, tag);
}

int __cdecl NewHandler(size_t)
{
    // The handler's own allocations can fail and call it again.
    if (!t_worker || t_inHandler)
        return 0;
    t_inHandler = true;
    LONG call = InterlockedIncrement(&g_handled);
    void* block = malloc(40);
    if (block != NULL)
    {
        if (call % 5 == 0)
            Leak(block, 1);
        else
            free(block);
    }
    char* array = new (std::nothrow) char[24];
    if (array != NULL)
    {
        if (call % 7 == 0)
            Leak(array, 2);
        else
            delete[] array;
    }
    HANDLE heap = GetProcessHeap();
    void* raw = HeapAlloc(heap, 0, 48);
    if (raw != NULL)
    {
        if (call % 3 == 0)
            Leak(raw, 3);
        else
            HeapFree(heap, 0, raw);
    }
    t_inHandler = false;
    if (++t_retries >= 2)
        return 0;
    return 1;
}

DWORD WINAPI Worker(LPVOID p)
{
    chaos::Random random((unsigned)(INT_PTR)p * 37 + 13);
    t_worker = true;
    for (int round = 0; round < ROUNDS; round++)
    {
        // The handler asks for one retry per failing allocation; operator new
        // and malloc in new mode 1 each loop over it.
        t_retries = 0;
        switch (random.Below(4))
        {
        case 0:
            try
            {
                char* never = new char[g_hugeSize];
                chaos::Fail("a huge operator new succeeded");
                delete[] never;
            }
            catch (const std::bad_alloc&)
            {
                InterlockedIncrement(&g_thrown);
            }
            break;
        case 1:
            chaos::Check(malloc(g_hugeSize) == NULL, "a huge malloc succeeded");
            break;
        case 2:
        {
            void* block = malloc(1 + random.Below(300));
            if (block != NULL)
            {
                if (random.Below(10) == 0)
                    Leak(block, 4);
                else
                    free(block);
            }
            break;
        }
        default:
        {
            int* value = new int(round);
            if (random.Below(10) == 0)
                Leak(value, 5);
            else
                delete value;
            break;
        }
        }
    }
    return 0;
}
}

int main()
{
    chaos::StartWatchdog(25000);
    SIZE_T base = VLDGetLeaksCount();

    chaos::SetPhase("failing allocations through an allocating new handler");
    _PNH previous = _set_new_handler(NewHandler);
    int previousMode = _set_new_mode(1);
    chaos::RunThreads(THREADS, Worker);
    _set_new_mode(previousMode);
    _set_new_handler(previous);

    chaos::SetPhase("counting");
    chaos::Check(g_handled > 0, "the new handler was never called");
    chaos::Check(g_thrown > 0, "operator new never threw");
    SIZE_T counted = VLDGetLeaksCount() - base;
    if (counted != (SIZE_T)g_leaked)
    {
        char message[128];
        chaos::DescribeMissingLeaks();
        sprintf_s(message, "leaked %ld blocks (%ld handler calls) but VLD counts %zu", g_leaked, g_handled, counted);
        chaos::Fail(message);
    }

    chaos::SetPhase("reporting");
    chaos::Check(chaos::DescribeMissingLeaks() == 0, "the report misplaces leaked blocks");
    VLDMarkAllLeaksAsReported();
    chaos::Pass();
    return 0;
}
