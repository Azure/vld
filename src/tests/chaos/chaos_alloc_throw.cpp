// chaos_alloc_throw.cpp - allocations that fail by throwing through VLD's
// hooks must not disturb tracking of later allocations on the same thread.
//
// operator new throws std::bad_alloc and a HEAP_GENERATE_EXCEPTIONS heap
// raises STATUS_NO_MEMORY from inside VLD's allocation hooks. Each thread
// then leaks a known number of blocks through every path, which VLD must
// count exactly.

#include "chaos_common.h"
#include "vld.h"
#include <new>
#include <stdlib.h>

namespace
{
const int THREADS = 6;
const int ROUNDS = 300;
const size_t HUGE_SIZE = (size_t)-1 / 2;

struct Kept
{
    DWORD id;
    int count;
    HANDLE heap;
};
Kept g_kept[THREADS];

bool ThrowingNew()
{
    try
    {
        char* volatile p = new char[HUGE_SIZE];
        delete[] p;
        return false;
    }
    catch (const std::bad_alloc&)
    {
        return true;
    }
}

bool RaisingHeapAlloc(HANDLE heap)
{
    __try
    {
        HeapAlloc(heap, HEAP_GENERATE_EXCEPTIONS, HUGE_SIZE);
        return false;
    }
    __except (GetExceptionCode() == STATUS_NO_MEMORY ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
    {
        return true;
    }
}

bool RaisingHeapReAlloc(HANDLE heap, void* block)
{
    __try
    {
        HeapReAlloc(heap, HEAP_GENERATE_EXCEPTIONS, block, HUGE_SIZE);
        return false;
    }
    __except (GetExceptionCode() == STATUS_NO_MEMORY ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
    {
        return true;
    }
}

DWORD WINAPI Worker(LPVOID p)
{
    Kept* kept = &g_kept[(INT_PTR)p];
    kept->id = GetCurrentThreadId();
    kept->heap = HeapCreate(HEAP_GENERATE_EXCEPTIONS, 0, 0);
    chaos::Check(kept->heap != NULL, "HeapCreate failed");
    chaos::Random random((unsigned)(INT_PTR)p * 101 + 3);
    for (int round = 0; round < ROUNDS; round++)
    {
        switch (random.Below(4))
        {
        case 0:
            chaos::Check(ThrowingNew(), "new of a huge size did not throw");
            break;
        case 1:
            chaos::Check(RaisingHeapAlloc(kept->heap), "HeapAlloc of a huge size did not raise");
            break;
        case 2:
        {
            void* block = HeapAlloc(kept->heap, 0, 64);
            chaos::Check(block != NULL, "HeapAlloc failed");
            chaos::Check(RaisingHeapReAlloc(kept->heap, block), "HeapReAlloc to a huge size did not raise");
            HeapFree(kept->heap, 0, block);
            break;
        }
        default:
            chaos::Check(malloc(HUGE_SIZE) == NULL, "malloc of a huge size succeeded");
            break;
        }
        switch (random.Below(3))
        {
        case 0:
            chaos::Check(malloc(24) != NULL, "malloc failed");
            break;
        case 1:
            chaos::Check(new char[24] != NULL, "new failed");
            break;
        default:
            chaos::Check(HeapAlloc(kept->heap, 0, 24) != NULL, "HeapAlloc failed");
            break;
        }
        kept->count++;
    }
    return 0;
}
}

int main()
{
    chaos::StartWatchdog(20000);
    VLDMarkAllLeaksAsReported();

    chaos::SetPhase("failing allocations that throw through the hooks");
    chaos::RunThreads(THREADS, Worker);

    chaos::SetPhase("counting");
    for (int i = 0; i < THREADS; i++)
    {
        UINT counted = VLDGetThreadLeaksCount(g_kept[i].id);
        if (counted != (UINT)g_kept[i].count)
        {
            char message[128];
            sprintf_s(message, "thread %d leaked %d blocks but VLD counts %u", i, g_kept[i].count, counted);
            chaos::Fail(message);
        }
    }

    chaos::SetPhase("cleanup");
    VLDMarkAllLeaksAsReported();
    for (int i = 0; i < THREADS; i++)
    {
        HeapDestroy(g_kept[i].heap);
    }
    chaos::Pass();
    return 0;
}
