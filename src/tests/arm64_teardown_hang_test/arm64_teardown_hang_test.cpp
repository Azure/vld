#include <Windows.h>

#include <cstdlib>
#include <cstring>

#include "vld.h"

namespace
{
using QueryVirtualMemoryInformationFunction = decltype(&QueryVirtualMemoryInformation);

volatile LONG g_blockQueryVirtualMemoryInformation = 0;
QueryVirtualMemoryInformationFunction g_originalQueryVirtualMemoryInformation = nullptr;
void* volatile g_leak = nullptr;

BOOL WINAPI BlockingQueryVirtualMemoryInformation(
    HANDLE process,
    const VOID* virtualAddress,
    WIN32_MEMORY_INFORMATION_CLASS memoryInformationClass,
    PVOID memoryInformation,
    SIZE_T memoryInformationSize,
    PSIZE_T returnSize)
{
    // A teardown regression becomes a bounded CTest timeout instead of relying
    // on the intermittent OS livelock that originally exposed this path.
    if (InterlockedCompareExchange(&g_blockQueryVirtualMemoryInformation, 0, 0) != 0)
    {
        Sleep(INFINITE);
    }

    return g_originalQueryVirtualMemoryInformation(
        process,
        virtualAddress,
        memoryInformationClass,
        memoryInformation,
        memoryInformationSize,
        returnSize);
}

bool ReplaceVldQueryVirtualMemoryInformationImport()
{
    HMODULE vldModule;
    if (!GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&VLDEnable),
            &vldModule))
    {
        return false;
    }

    auto* imageBase = reinterpret_cast<unsigned char*>(vldModule);
    auto* dosHeader = reinterpret_cast<IMAGE_DOS_HEADER*>(imageBase);
    if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE || dosHeader->e_lfanew <= 0)
    {
        return false;
    }

    auto* ntHeaders = reinterpret_cast<IMAGE_NT_HEADERS*>(
        imageBase + static_cast<size_t>(dosHeader->e_lfanew));
    if (ntHeaders->Signature != IMAGE_NT_SIGNATURE)
    {
        return false;
    }

    const IMAGE_DATA_DIRECTORY& importDirectory =
        ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (importDirectory.VirtualAddress == 0)
    {
        return false;
    }

    auto* importDescriptor = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(
        imageBase + importDirectory.VirtualAddress);
    for (; importDescriptor->Name != 0; ++importDescriptor)
    {
        if (importDescriptor->OriginalFirstThunk == 0)
        {
            continue;
        }

        auto* nameThunk = reinterpret_cast<IMAGE_THUNK_DATA*>(
            imageBase + importDescriptor->OriginalFirstThunk);
        auto* addressThunk = reinterpret_cast<IMAGE_THUNK_DATA*>(
            imageBase + importDescriptor->FirstThunk);

        for (; nameThunk->u1.AddressOfData != 0; ++nameThunk, ++addressThunk)
        {
            if (IMAGE_SNAP_BY_ORDINAL(nameThunk->u1.Ordinal))
            {
                continue;
            }

            auto* importByName = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(
                imageBase + nameThunk->u1.AddressOfData);
            if (std::strcmp(
                    reinterpret_cast<const char*>(importByName->Name),
                    "QueryVirtualMemoryInformation") != 0)
            {
                continue;
            }

            DWORD oldProtection;
            if (!VirtualProtect(
                    &addressThunk->u1.Function,
                    sizeof(addressThunk->u1.Function),
                    PAGE_READWRITE,
                    &oldProtection))
            {
                return false;
            }

            g_originalQueryVirtualMemoryInformation =
                reinterpret_cast<QueryVirtualMemoryInformationFunction>(
                    addressThunk->u1.Function);
            InterlockedExchangePointer(
                reinterpret_cast<PVOID volatile*>(&addressThunk->u1.Function),
                reinterpret_cast<PVOID>(&BlockingQueryVirtualMemoryInformation));

            DWORD ignoredProtection;
            if (!VirtualProtect(
                    &addressThunk->u1.Function,
                    sizeof(addressThunk->u1.Function),
                    oldProtection,
                    &ignoredProtection))
            {
                return false;
            }
            return true;
        }
    }

    return false;
}
}

int main()
{
    VLDEnable();
    g_leak = std::malloc(64);
    if (g_leak == nullptr)
    {
        return 1;
    }

    if (!ReplaceVldQueryVirtualMemoryInformationImport())
    {
        return 2;
    }

    InterlockedExchange(&g_blockQueryVirtualMemoryInformation, 1);
    ExitProcess(0);
}
