// chaos_proc_ordinal.cpp - GetProcAddress by ordinal on modules VLD patches.
//
// Looks up every export of the modules whose imports VLD replaces by ordinal
// instead of by name. The lookup must not crash, and an allocation API found
// by ordinal must be tracked like the same API found by name.

#include "chaos_common.h"
#include "vld.h"

namespace
{
typedef LPVOID (WINAPI *HeapAlloc_t)(HANDLE, DWORD, SIZE_T);

// Returns the ordinal of a named export, or 0.
WORD OrdinalOf(HMODULE module, const char* name)
{
    BYTE* base = (BYTE*)module;
    IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)(base + ((IMAGE_DOS_HEADER*)base)->e_lfanew);
    IMAGE_DATA_DIRECTORY dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (dir.VirtualAddress == 0)
        return 0;
    IMAGE_EXPORT_DIRECTORY* exports = (IMAGE_EXPORT_DIRECTORY*)(base + dir.VirtualAddress);
    DWORD* names = (DWORD*)(base + exports->AddressOfNames);
    WORD* ordinals = (WORD*)(base + exports->AddressOfNameOrdinals);
    for (DWORD i = 0; i < exports->NumberOfNames; i++)
    {
        if (strcmp((const char*)(base + names[i]), name) == 0)
            return (WORD)(exports->Base + ordinals[i]);
    }
    return 0;
}

void LookUpAllOrdinals(HMODULE module)
{
    BYTE* base = (BYTE*)module;
    IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)(base + ((IMAGE_DOS_HEADER*)base)->e_lfanew);
    IMAGE_DATA_DIRECTORY dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (dir.VirtualAddress == 0)
        return;
    IMAGE_EXPORT_DIRECTORY* exports = (IMAGE_EXPORT_DIRECTORY*)(base + dir.VirtualAddress);
    for (DWORD i = 0; i < exports->NumberOfFunctions; i++)
    {
        GetProcAddress(module, MAKEINTRESOURCEA(exports->Base + i));
    }
}
}

int main()
{
    chaos::StartWatchdog(20000);

    const wchar_t* modules[] = { L"kernel32.dll", L"kernelbase.dll", L"ntdll.dll", L"ucrtbase.dll",
        L"ucrtbased.dll", L"msvcrt.dll", L"ole32.dll", L"combase.dll", L"vcruntime140.dll", L"vcruntime140d.dll" };
    chaos::SetPhase("GetProcAddress by ordinal for every export");
    for (int i = 0; i < _countof(modules); i++)
    {
        HMODULE module = GetModuleHandleW(modules[i]);
        if (module != NULL)
            LookUpAllOrdinals(module);
    }

    chaos::SetPhase("allocating through HeapAlloc found by ordinal");
    HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
    WORD ordinal = OrdinalOf(kernel32, "HeapAlloc");
    chaos::Check(ordinal != 0, "HeapAlloc has no ordinal");
    HeapAlloc_t heapAlloc = (HeapAlloc_t)GetProcAddress(kernel32, MAKEINTRESOURCEA(ordinal));
    chaos::Check(heapAlloc != NULL, "GetProcAddress(HeapAlloc ordinal) failed");
    VLDMarkAllLeaksAsReported();
    void* block = heapAlloc(GetProcessHeap(), 0, 40);
    chaos::Check(block != NULL, "HeapAlloc failed");
    chaos::Check(VLDGetLeaksCount() == 1, "a block from HeapAlloc found by ordinal is not tracked");
    HeapFree(GetProcessHeap(), 0, block);
    chaos::Check(VLDGetLeaksCount() == 0, "the freed block is still counted");

    chaos::Pass();
    return 0;
}
