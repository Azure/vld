// chaos_heap_flags.cpp - unusual heap flags and heap inspection APIs under
// allocation traffic, with exact leak counts.
//
// Covers HEAP_NO_SERIALIZE heaps, HEAP_GENERATE_EXCEPTIONS with a failing
// allocation, HEAP_REALLOC_IN_PLACE_ONLY that fails or succeeds, zero-size
// blocks and reallocations to zero, and HeapLock/HeapWalk/HeapCompact/
// HeapValidate running while other threads allocate from the same heap.

#include "chaos_common.h"
#include "vld.h"

namespace
{
const DWORD RUN_MS = 2500;
volatile LONG g_stop = 0;

DWORD WINAPI Allocator(LPVOID p)
{
    chaos::Random random((unsigned)(INT_PTR)p * 31 + 7);
    HANDLE heap = GetProcessHeap();
    void* slots[16] = { 0 };
    while (!InterlockedCompareExchange(&g_stop, 0, 0))
    {
        unsigned i = random.Below(16);
        if (slots[i] == NULL)
        {
            slots[i] = HeapAlloc(heap, (random.Below(2) ? HEAP_ZERO_MEMORY : 0), random.Below(512));
        }
        else if (random.Below(3) == 0)
        {
            void* moved = HeapReAlloc(heap, random.Below(2) ? HEAP_REALLOC_IN_PLACE_ONLY : 0, slots[i], random.Below(2048));
            if (moved != NULL)
            {
                slots[i] = moved;
            }
        }
        else
        {
            HeapFree(heap, 0, slots[i]);
            slots[i] = NULL;
        }
    }
    for (int i = 0; i < 16; i++)
    {
        if (slots[i] != NULL)
        {
            HeapFree(heap, 0, slots[i]);
        }
    }
    return 0;
}

DWORD WINAPI Inspector(LPVOID p)
{
    HANDLE heap = GetProcessHeap();
    bool walk = ((INT_PTR)p % 2) == 0;
    while (!InterlockedCompareExchange(&g_stop, 0, 0))
    {
        if (walk)
        {
            if (HeapLock(heap))
            {
                PROCESS_HEAP_ENTRY entry = { 0 };
                int entries = 0;
                while (HeapWalk(heap, &entry) && entries < 100000)
                {
                    entries++;
                }
                HeapUnlock(heap);
            }
        }
        else
        {
            HeapCompact(heap, 0);
            HeapValidate(heap, 0, NULL);
        }
        Sleep(0);
    }
    return 0;
}

DWORD WINAPI Worker(LPVOID p)
{
    INT_PTR index = (INT_PTR)p;
    if (index < 4)
    {
        return Allocator(p);
    }
    if (index < 6)
    {
        return Inspector(p);
    }
    Sleep(RUN_MS);
    InterlockedExchange(&g_stop, 1);
    return 0;
}

bool AllocWithExceptions(HANDLE heap, SIZE_T size)
{
    __try
    {
        void* p = HeapAlloc(heap, HEAP_GENERATE_EXCEPTIONS, size);
        if (p != NULL)
        {
            HeapFree(heap, 0, p);
        }
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}
}

int main()
{
    chaos::StartWatchdog(25000);
    SIZE_T base = VLDGetLeaksCount();

    chaos::SetPhase("HEAP_NO_SERIALIZE heap");
    HANDLE noSerialize = HeapCreate(HEAP_NO_SERIALIZE, 0, 0);
    chaos::Check(noSerialize != NULL, "HeapCreate(HEAP_NO_SERIALIZE) failed");
    void* blocks[10];
    for (int i = 0; i < 10; i++)
    {
        blocks[i] = HeapAlloc(noSerialize, HEAP_NO_SERIALIZE, 32 + i * 8);
    }
    chaos::Check(VLDGetLeaksCount() == base + 10, "HEAP_NO_SERIALIZE allocations not counted");
    void* grown = HeapReAlloc(noSerialize, HEAP_REALLOC_IN_PLACE_ONLY, blocks[0], 1024 * 1024);
    chaos::Check(grown == NULL, "an in-place reallocation to 1 MB unexpectedly succeeded");
    chaos::Check(VLDGetLeaksCount() == base + 10, "a failed in-place reallocation changed the count");
    void* shrunk = HeapReAlloc(noSerialize, HEAP_REALLOC_IN_PLACE_ONLY, blocks[1], 8);
    chaos::Check(shrunk == NULL || shrunk == blocks[1], "an in-place shrink moved the block");
    void* zero = HeapReAlloc(noSerialize, 0, blocks[2], 0);
    chaos::Check(zero != NULL, "HeapReAlloc to zero bytes failed");
    blocks[2] = zero;
    chaos::Check(VLDGetLeaksCount() == base + 10, "reallocations changed the count");
    for (int i = 0; i < 10; i++)
    {
        HeapFree(noSerialize, 0, blocks[i]);
    }
    chaos::Check(VLDGetLeaksCount() == base, "HEAP_NO_SERIALIZE blocks still counted after HeapFree");
    HeapDestroy(noSerialize);

    chaos::SetPhase("HEAP_GENERATE_EXCEPTIONS with a failing allocation");
    HANDLE fixedHeap = HeapCreate(HEAP_GENERATE_EXCEPTIONS, 0, 64 * 1024);
    chaos::Check(fixedHeap != NULL, "HeapCreate(HEAP_GENERATE_EXCEPTIONS) failed");
    chaos::Check(!AllocWithExceptions(fixedHeap, 1024 * 1024), "a 1 MB allocation from a 64 KB fixed heap did not raise");
    chaos::Check(AllocWithExceptions(fixedHeap, 100), "a small allocation raised");
    chaos::Check(VLDGetLeaksCount() == base, "a failed allocation was counted");
    HeapDestroy(fixedHeap);

    chaos::SetPhase("zero-size blocks");
    void* empty = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 0);
    chaos::Check(empty != NULL, "HeapAlloc of zero bytes failed");
    chaos::Check(VLDGetLeaksCount() == base + 1, "a zero-size block is not counted");
    HeapFree(GetProcessHeap(), 0, empty);
    chaos::Check(VLDGetLeaksCount() == base, "a freed zero-size block is still counted");

    chaos::SetPhase("heap inspection under allocation traffic");
    chaos::RunThreads(7, Worker);
    chaos::Check(VLDGetLeaksCount() == base, "allocation traffic with heap inspection left leaks");
    chaos::Pass();
    return 0;
}
