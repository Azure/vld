// chaos_heapdestroy_deadlock.cpp - HeapDestroy of a heap with live blocks must
// not resolve symbols while holding g_heapMapLock.
//
// VLD's HeapDestroy hook reports the heap's remaining blocks before the heap
// goes away. Same hazard as the leak queries: if dbghelp loads a module's
// symbols there (a symbol server load needs the loader lock) while VLD holds
// g_heapMapLock, a thread in DLL_THREAD_ATTACH that allocates waits for
// g_heapMapLock while holding the loader lock. A dbghelp callback on the
// helper DLL's deferred symbol load makes that ordering deterministic.

#include "chaos_common.h"
#include "vld.h"

namespace
{
const ULONG CBA_DEFERRED_SYMBOL_LOAD_START = 0x00000001;
struct DeferredSymbolLoadW64
{
    DWORD SizeOfStruct;
    DWORD64 BaseOfImage;
    DWORD CheckSum;
    DWORD TimeDateStamp;
    WCHAR FileName[MAX_PATH + 1];
};
typedef BOOL (CALLBACK* SymbolRegisteredCallback64)(HANDLE, ULONG, ULONG64, ULONG64);
typedef BOOL (WINAPI* SymRegisterCallbackW64_t)(HANDLE, SymbolRegisteredCallback64, ULONG64);
typedef void (*ArmThreadAttach_t)(HANDLE);
typedef void* (*HelperHeapAlloc_t)(HANDLE, size_t);

ArmThreadAttach_t g_armThreadAttach = NULL;
HANDLE g_inThreadAttach = NULL;
HANDLE g_thread = NULL;
volatile LONG g_callbackArmed = 0;
volatile LONG g_callbackFired = 0;

DWORD WINAPI NewThread(LPVOID)
{
    return 0;
}

BOOL CALLBACK SymbolCallback(HANDLE, ULONG action, ULONG64 data, ULONG64)
{
    if (action != CBA_DEFERRED_SYMBOL_LOAD_START || data == 0)
    {
        return FALSE;
    }
    const DeferredSymbolLoadW64* load = (const DeferredSymbolLoadW64*)data;
    if (wcsstr(load->FileName, L"chaos_helper_dll") == NULL || InterlockedExchange(&g_callbackArmed, 0) == 0)
    {
        return FALSE;
    }
    InterlockedExchange(&g_callbackFired, 1);
    chaos::SetPhase("dbghelp deferred load of the helper: new thread in DLL_THREAD_ATTACH + LoadLibrary");
    g_armThreadAttach(g_inThreadAttach);
    g_thread = CreateThread(NULL, 0, NewThread, NULL, 0, NULL);
    chaos::Check(g_thread != NULL, "CreateThread failed");
    WaitForSingleObject(g_inThreadAttach, 2000);
    HMODULE version = LoadLibraryW(L"version.dll");
    if (version != NULL)
    {
        FreeLibrary(version);
    }
    return FALSE;
}

int __cdecl SwallowReport(int, wchar_t*, int*)
{
    return 1;
}
}

int main()
{
    chaos::StartWatchdog(20000);
    g_inThreadAttach = CreateEventW(NULL, TRUE, FALSE, NULL);

    HMODULE helper = LoadLibraryW(L"chaos_helper_dll.dll");
    chaos::Check(helper != NULL, "LoadLibrary(chaos_helper_dll.dll) failed");
    g_armThreadAttach = (ArmThreadAttach_t)GetProcAddress(helper, "ArmThreadAttach");
    HelperHeapAlloc_t helperHeapAlloc = (HelperHeapAlloc_t)GetProcAddress(helper, "HelperHeapAlloc");
    HMODULE dbghelp = GetModuleHandleW(L"dbghelp.dll");
    SymRegisterCallbackW64_t symRegisterCallbackW64 = dbghelp == NULL ? NULL :
        (SymRegisterCallbackW64_t)GetProcAddress(dbghelp, "SymRegisterCallbackW64");
    chaos::Check(g_armThreadAttach != NULL && helperHeapAlloc != NULL && symRegisterCallbackW64 != NULL &&
        symRegisterCallbackW64(GetCurrentProcess(), SymbolCallback, 0), "setup");

    HANDLE heap = HeapCreate(0, 0, 0);
    chaos::Check(heap != NULL, "HeapCreate failed");
    chaos::Check(helperHeapAlloc(heap, 64) != NULL, "HeapAlloc failed");

    // The destroyed heap's block is reported by design; keep it out of stdout.
    VLDSetReportHook(VLD_RPTHOOK_INSTALL, SwallowReport);
    InterlockedExchange(&g_callbackArmed, 1);
    chaos::SetPhase("HeapDestroy of a heap with a live block allocated from the helper");
    HeapDestroy(heap);
    VLDSetReportHook(VLD_RPTHOOK_REMOVE, SwallowReport);
    if (g_thread != NULL)
    {
        WaitForSingleObject(g_thread, INFINITE);
        CloseHandle(g_thread);
    }
    chaos::Check(InterlockedCompareExchange(&g_callbackFired, 0, 0) != 0,
        "the helper's deferred symbol load never happened (inconclusive)");
    chaos::Pass();
    return 0;
}
