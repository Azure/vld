// review_proc_name_address.cpp - PR #74 review comment on vld.cpp:2535.
//
// GetProcAddress takes a name or an ordinal; a value above 0xFFFF is a name.
// On 64-bit Windows a name can sit at an address whose bits 16-31 are zero
// (0x0000000100001234), which HIWORD alone mistakes for an ordinal. VLD must
// still hand out its replacement for such a name.

#include "review_common.h"
#include "vld.h"

namespace
{
typedef LPVOID (WINAPI *HeapAlloc_t)(HANDLE, DWORD, SIZE_T);
}

int main()
{
    chaos::StartWatchdog(20000);
#ifdef _WIN64
    chaos::SetPhase("placing a name at an address with bits 16-31 zero");
    const SIZE_T size = 64 * 1024;
    char* page = NULL;
    for (ULONG_PTR high = 1; high < 0x8000 && page == NULL; high++)
    {
        page = (char*)VirtualAlloc((LPVOID)(high << 32), size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    }
    chaos::Check(page != NULL, "no 4 GB aligned address is free");
    char* name = page + 0x1234;
    strcpy_s(name, 32, "HeapAlloc");
    chaos::Check(((ULONG_PTR)name >> 16 & 0xFFFF) == 0 && (ULONG_PTR)name > 0xFFFF, "the name is not where it should be");

    chaos::SetPhase("allocating through the HeapAlloc found by that name");
    HeapAlloc_t heapAlloc = (HeapAlloc_t)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), name);
    chaos::Check(heapAlloc != NULL, "GetProcAddress failed");
    VLDMarkAllLeaksAsReported();
    void* block = heapAlloc(GetProcessHeap(), 0, 40);
    chaos::Check(block != NULL, "HeapAlloc failed");
    chaos::Check(VLDGetLeaksCount() == 1, "a block from HeapAlloc found by a name at that address is not tracked");
    HeapFree(GetProcessHeap(), 0, block);
    chaos::Check(VLDGetLeaksCount() == 0, "the freed block is still counted");
    VirtualFree(page, 0, MEM_RELEASE);
#endif
    chaos::Pass();
    return 0;
}
