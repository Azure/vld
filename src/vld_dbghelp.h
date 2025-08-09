#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <DbgHelp.h>
#include "criticalsection.h"

class DbgHelp
{
public:
    DbgHelp() = default;
    ~DbgHelp() = default; 

    void lock()
    {
        m_lock.lock();
    }
    void unlock()
    {
        m_lock.unlock();
    }
    bool try_lock()
    {
        return m_lock.try_lock();
    }
    BOOL IsLockedByCurrentThread() {
        return
            m_lock.is_locked_by_current_thread();
    }
    BOOL SymInitializeW(_In_ HANDLE hProcess, _In_opt_ PCWSTR UserSearchPath, _In_ BOOL fInvadeProcess, std::scoped_lock<DbgHelp>&) {
        return ::SymInitializeW(hProcess, UserSearchPath, fInvadeProcess);
    }
    BOOL SymInitializeW(_In_ HANDLE hProcess, _In_opt_ PCWSTR UserSearchPath, _In_ BOOL fInvadeProcess) {
        std::scoped_lock lock(m_lock);
        return ::SymInitializeW(hProcess, UserSearchPath, fInvadeProcess);
    }
    BOOL SymCleanup(_In_ HANDLE hProcess, std::scoped_lock<DbgHelp>&) {
        return ::SymCleanup(hProcess);
    }
    BOOL SymCleanup(_In_ HANDLE hProcess) {
        std::scoped_lock lock(m_lock);
        return ::SymCleanup(hProcess);
    }
    DWORD SymSetOptions(__in DWORD SymOptions, std::scoped_lock<DbgHelp>&) {
        return ::SymSetOptions(SymOptions);
    }
    DWORD SymSetOptions(__in DWORD SymOptions) {
        std::scoped_lock lock(m_lock);
        return ::SymSetOptions(SymOptions);
    }
    BOOL SymFromAddrW(_In_ HANDLE hProcess, _In_ DWORD64 Address, _Out_opt_ PDWORD64 Displacement, _Inout_ PSYMBOL_INFOW Symbol, std::scoped_lock<DbgHelp>&) {
        return ::SymFromAddrW(hProcess, Address, Displacement, Symbol);
    }
    BOOL SymFromAddrW(_In_ HANDLE hProcess, _In_ DWORD64 Address, _Out_opt_ PDWORD64 Displacement, _Inout_ PSYMBOL_INFOW Symbol) {
        std::scoped_lock lock(m_lock);
        return ::SymFromAddrW(hProcess, Address, Displacement, Symbol);
    }
    BOOL SymGetLineFromAddrW64(_In_ HANDLE hProcess, _In_ DWORD64 dwAddr, _Out_ PDWORD pdwDisplacement, _Out_ PIMAGEHLP_LINEW64 Line, std::scoped_lock<DbgHelp>&) {
        return ::SymGetLineFromAddrW64(hProcess, dwAddr, pdwDisplacement, Line);
    }
    BOOL SymGetLineFromAddrW64(_In_ HANDLE hProcess, _In_ DWORD64 dwAddr, _Out_ PDWORD pdwDisplacement, _Out_ PIMAGEHLP_LINEW64 Line) {
        std::scoped_lock lock(m_lock);
        return ::SymGetLineFromAddrW64(hProcess, dwAddr, pdwDisplacement, Line);
    }
    BOOL SymGetModuleInfoW64(_In_ HANDLE hProcess, _In_ DWORD64 qwAddr, _Out_ PIMAGEHLP_MODULEW64 ModuleInfo, std::scoped_lock<DbgHelp>&) {
        return ::SymGetModuleInfoW64(hProcess, qwAddr, ModuleInfo);
    }
    BOOL SymGetModuleInfoW64(_In_ HANDLE hProcess, _In_ DWORD64 qwAddr, _Out_ PIMAGEHLP_MODULEW64 ModuleInfo) {
        std::scoped_lock lock(m_lock);
        return ::SymGetModuleInfoW64(hProcess, qwAddr, ModuleInfo);
    }
    DWORD64 SymLoadModuleExW(_In_ HANDLE hProcess, _In_opt_ HANDLE hFile, _In_opt_ PCWSTR ImageName, _In_opt_ PCWSTR ModuleName, _In_ DWORD64 BaseOfDll, _In_ DWORD DllSize, _In_opt_ PMODLOAD_DATA Data, _In_opt_ DWORD Flags, std::scoped_lock<DbgHelp>&) {
        return ::SymLoadModuleExW(hProcess, hFile, ImageName, ModuleName, BaseOfDll, DllSize, Data, Flags);
    }
    DWORD64 SymLoadModuleExW(_In_ HANDLE hProcess, _In_opt_ HANDLE hFile, _In_opt_ PCWSTR ImageName, _In_opt_ PCWSTR ModuleName, _In_ DWORD64 BaseOfDll, _In_ DWORD DllSize, _In_opt_ PMODLOAD_DATA Data, _In_opt_ DWORD Flags) {
        std::scoped_lock lock(m_lock);
        return ::SymLoadModuleExW(hProcess, hFile, ImageName, ModuleName, BaseOfDll, DllSize, Data, Flags);
    }
    BOOL SymUnloadModule64(_In_ HANDLE hProcess, _In_ DWORD64 BaseOfDll, std::scoped_lock<DbgHelp>&) {
        return ::SymUnloadModule64(hProcess, BaseOfDll);
    }
    BOOL SymUnloadModule64(_In_ HANDLE hProcess, _In_ DWORD64 BaseOfDll) {
        std::scoped_lock lock(m_lock);
        return ::SymUnloadModule64(hProcess, BaseOfDll);
    }
    BOOL StackWalk64(__in DWORD MachineType, __in HANDLE hProcess, __in HANDLE hThread,
        __inout LPSTACKFRAME64 StackFrame, __inout PVOID ContextRecord,
        __in_opt PREAD_PROCESS_MEMORY_ROUTINE64 ReadMemoryRoutine,
        __in_opt PFUNCTION_TABLE_ACCESS_ROUTINE64 FunctionTableAccessRoutine,
        __in_opt PGET_MODULE_BASE_ROUTINE64 GetModuleBaseRoutine,
        __in_opt PTRANSLATE_ADDRESS_ROUTINE64 TranslateAddress, std::scoped_lock<DbgHelp>&)
    {
        return ::StackWalk64(MachineType, hProcess, hThread, StackFrame, ContextRecord, ReadMemoryRoutine,
            FunctionTableAccessRoutine, GetModuleBaseRoutine, TranslateAddress);
    }
    BOOL StackWalk64(__in DWORD MachineType, __in HANDLE hProcess, __in HANDLE hThread,
        __inout LPSTACKFRAME64 StackFrame, __inout PVOID ContextRecord,
        __in_opt PREAD_PROCESS_MEMORY_ROUTINE64 ReadMemoryRoutine,
        __in_opt PFUNCTION_TABLE_ACCESS_ROUTINE64 FunctionTableAccessRoutine,
        __in_opt PGET_MODULE_BASE_ROUTINE64 GetModuleBaseRoutine,
        __in_opt PTRANSLATE_ADDRESS_ROUTINE64 TranslateAddress)
    {
        std::scoped_lock lock(m_lock);
        return ::StackWalk64(MachineType, hProcess, hThread, StackFrame, ContextRecord, ReadMemoryRoutine,
            FunctionTableAccessRoutine, GetModuleBaseRoutine, TranslateAddress);
    }
private:
    DbgHelp(const DbgHelp&) = delete;
    DbgHelp& operator=(const DbgHelp&) = delete;

    DbgHelp(DbgHelp&&) = delete;
    DbgHelp& operator=(DbgHelp&&) = delete;

    vld::criticalsection m_lock;
};

class ImageDirectoryEntries
{
public:
    ImageDirectoryEntries() = default;
    ~ImageDirectoryEntries() = default;

    PVOID ImageDirectoryEntryToDataEx(__in PVOID Base, __in BOOLEAN MappedAsImage, __in USHORT DirectoryEntry, __out PULONG Size, __out_opt PIMAGE_SECTION_HEADER *FoundHeader) {
        std::scoped_lock lock(_mutex);
        return ::ImageDirectoryEntryToDataEx(Base, MappedAsImage, DirectoryEntry, Size, FoundHeader);
    }
private:
    ImageDirectoryEntries(const ImageDirectoryEntries&) = delete;
    ImageDirectoryEntries& operator=(const ImageDirectoryEntries&) = delete;

    ImageDirectoryEntries(ImageDirectoryEntries&&) = delete;
    ImageDirectoryEntries& operator=(ImageDirectoryEntries&&) = delete;

    vld::criticalsection _mutex{};
};

class LoadedModules
{
public:
    LoadedModules() = default;
    ~LoadedModules() = default;
    
    BOOL EnumerateLoadedModulesW64(__in HANDLE hProcess, __in PENUMLOADED_MODULES_CALLBACKW64 EnumLoadedModulesCallback, __in_opt PVOID UserContext) {
        std::scoped_lock lock(_mutex);
        return ::EnumerateLoadedModulesW64(hProcess, EnumLoadedModulesCallback, UserContext);
    }
private:
    LoadedModules(const LoadedModules&) = delete;
    LoadedModules& operator=(const LoadedModules&) = delete;

    LoadedModules(LoadedModules&&) = delete;
    LoadedModules& operator=(LoadedModules&&) = delete;

    vld::criticalsection _mutex{};
};
