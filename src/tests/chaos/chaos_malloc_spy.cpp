// chaos_malloc_spy.cpp - an IMallocSpy that moves every COM task block behind
// a header of its own, and allocates and leaks from inside the allocations it
// watches.
//
// VLD hooks CoTaskMemAlloc and CoTaskMemRealloc. The spy runs inside them:
// PostAlloc and PostRealloc allocate CRT and heap blocks while the COM
// allocation is still in progress, and the pointer the application gets is
// 16 bytes past the heap block. VLD must count every leaked block and report
// it next to the pointer the application holds.

#include "chaos_ledger.h"
#include <objbase.h>
#include <malloc.h>

#pragma comment(lib, "ole32.lib")

namespace
{
const int THREADS = 6;
const int ROUNDS = 300;
const SIZE_T HEADER = 16;
volatile LONG g_leaked = 0;
volatile LONG g_spied = 0;
__declspec(thread) bool t_worker = false;
__declspec(thread) bool t_inSpy = false;
__declspec(thread) SIZE_T t_reallocSize = 0;
__declspec(thread) bool t_reallocAddsHeader = false;

// Tags: 1 spy malloc, 2 spy heap block, 3 spy new, 4 worker COM block.
void Leak(void* block, LONG tag)
{
    InterlockedIncrement(&g_leaked);
    chaos::RecordLeak(block, tag);
}

void AllocateInsideSpy()
{
    if (!t_worker || t_inSpy)
        return;
    t_inSpy = true;
    LONG call = InterlockedIncrement(&g_spied);
    void* block = malloc(32);
    if (block != NULL)
    {
        if (call % 6 == 0)
            Leak(block, 1);
        else
            free(block);
    }
    HANDLE heap = GetProcessHeap();
    void* raw = HeapAlloc(heap, 0, 48);
    if (raw != NULL)
    {
        if (call % 4 == 0)
            Leak(raw, 2);
        else
            HeapFree(heap, 0, raw);
    }
    if (call % 9 == 0)
        Leak(new int((int)call), 3);
    t_inSpy = false;
}

void* Behind(void* request, BOOL spied)
{
    return request != NULL && spied ? (BYTE*)request - HEADER : request;
}

class Spy : public IMallocSpy
{
public:
    STDMETHODIMP QueryInterface(REFIID riid, void** object)
    {
        if (riid == IID_IUnknown || riid == IID_IMallocSpy)
        {
            *object = static_cast<IMallocSpy*>(this);
            return S_OK;
        }
        *object = NULL;
        return E_NOINTERFACE;
    }
    // The spy is a static object; COM may hold it past main.
    STDMETHODIMP_(ULONG) AddRef() { return 2; }
    STDMETHODIMP_(ULONG) Release() { return 1; }

    STDMETHODIMP_(SIZE_T) PreAlloc(SIZE_T request) { return request + HEADER; }
    STDMETHODIMP_(void*) PostAlloc(void* actual)
    {
        if (actual == NULL)
            return NULL;
        memset(actual, 0x5A, HEADER);
        AllocateInsideSpy();
        return (BYTE*)actual + HEADER;
    }
    STDMETHODIMP_(void*) PreFree(void* request, BOOL spied) { return Behind(request, spied); }
    STDMETHODIMP_(void) PostFree(BOOL) {}
    STDMETHODIMP_(SIZE_T) PreRealloc(void* request, SIZE_T size, void** newRequest, BOOL spied)
    {
        *newRequest = Behind(request, spied);
        // COM treats the result as spied even if the block was not, so a block
        // without a header gets one and its data moves behind it.
        t_reallocSize = size;
        t_reallocAddsHeader = request != NULL && !spied;
        return size == 0 ? 0 : size + HEADER;
    }
    STDMETHODIMP_(void*) PostRealloc(void* actual, BOOL)
    {
        if (actual == NULL)
            return NULL;
        if (t_reallocAddsHeader)
            memmove((BYTE*)actual + HEADER, actual, t_reallocSize);
        AllocateInsideSpy();
        return (BYTE*)actual + HEADER;
    }
    STDMETHODIMP_(void*) PreGetSize(void* request, BOOL spied) { return Behind(request, spied); }
    STDMETHODIMP_(SIZE_T) PostGetSize(SIZE_T actual, BOOL spied) { return spied && actual >= HEADER ? actual - HEADER : actual; }
    STDMETHODIMP_(void*) PreDidAlloc(void* request, BOOL spied) { return Behind(request, spied); }
    STDMETHODIMP_(int) PostDidAlloc(void*, BOOL, int actual) { return actual; }
    STDMETHODIMP_(void) PreHeapMinimize() {}
    STDMETHODIMP_(void) PostHeapMinimize() {}
};

Spy g_spy;

DWORD WINAPI Worker(LPVOID p)
{
    chaos::Random random((unsigned)(INT_PTR)p * 41 + 7);
    t_worker = true;
    for (int round = 0; round < ROUNDS; round++)
    {
        void* block = random.Below(2) ? CoTaskMemAlloc(1 + random.Below(300)) : CoTaskMemRealloc(NULL, 1 + random.Below(300));
        chaos::Check(block != NULL, "a COM task allocation failed");
        if (random.Below(2))
        {
            void* moved = CoTaskMemRealloc(block, 1 + random.Below(2000));
            chaos::Check(moved != NULL, "a COM task reallocation failed");
            block = moved;
        }
        if (random.Below(15) == 0)
        {
            Leak(block, 4);
            continue;
        }
        if (random.Below(2))
            CoTaskMemFree(block);
        else
            chaos::Check(CoTaskMemRealloc(block, 0) == NULL, "CoTaskMemRealloc to zero bytes returned a block");
    }
    return 0;
}
}

int main()
{
    chaos::StartWatchdog(25000);
    chaos::Check(SUCCEEDED(CoInitializeEx(NULL, COINIT_MULTITHREADED)), "CoInitializeEx failed");
    SIZE_T base = VLDGetLeaksCount();

    chaos::SetPhase("COM task memory through an allocating malloc spy");
    BYTE* early = (BYTE*)CoTaskMemAlloc(64);
    chaos::Check(early != NULL, "CoTaskMemAlloc failed");
    memset(early, 0x3C, 64);
    chaos::Check(SUCCEEDED(CoRegisterMallocSpy(&g_spy)), "CoRegisterMallocSpy failed");
    // A block from before the spy is reallocated and freed through it.
    early = (BYTE*)CoTaskMemRealloc(early, 128);
    chaos::Check(early != NULL && early[0] == 0x3C && early[63] == 0x3C, "reallocating a block from before the spy lost its data");
    CoTaskMemFree(early);
    chaos::RunThreads(THREADS, Worker);
    // Revoking waits for the spied blocks, and some are leaked.
    CoRevokeMallocSpy();

    chaos::SetPhase("counting");
    chaos::Check(g_spied > 0, "the spy never allocated");
    SIZE_T counted = VLDGetLeaksCount() - base;
    if (counted != (SIZE_T)g_leaked)
    {
        char message[128];
        chaos::DescribeMissingLeaks();
        sprintf_s(message, "leaked %ld blocks (%ld spy calls) but VLD counts %zu", g_leaked, g_spied, counted);
        chaos::Fail(message);
    }

    chaos::SetPhase("reporting");
    chaos::Check(chaos::DescribeMissingLeaks() == 0, "the report misplaces leaked blocks");
    VLDMarkAllLeaksAsReported();
    chaos::Pass();
    return 0;
}
