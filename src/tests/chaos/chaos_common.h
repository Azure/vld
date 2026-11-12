// chaos_common.h - shared helpers for the VLD chaos tests.
//
// Every chaos test is a console program that ends with "CHAOS PASS" on
// success. A watchdog turns a hang into a failure: it prints the test's
// current phase and terminates the process with exit code 3, without running
// any exit code that could itself need a lock held by the hung threads.

#pragma once

#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

namespace chaos
{
inline HANDLE& WatchdogDone()
{
    static HANDLE done = NULL;
    return done;
}

inline volatile LONG& WatchdogMs()
{
    static volatile LONG ms = 0;
    return ms;
}

inline const char* volatile& Phase()
{
    static const char* volatile phase = "start";
    return phase;
}

inline void Write(const char* text)
{
    DWORD written;
    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), text, (DWORD)strlen(text), &written, NULL);
}

inline void SetPhase(const char* phase)
{
    Phase() = phase;
}

// Prints "FAIL: <message>" and terminates without running exit handlers. Uses
// no CRT, whose locks the hung or crashed threads may hold.
inline void Fail(const char* message, UINT exitCode = 1)
{
    Write("FAIL: ");
    Write(message);
    Write(" (phase: ");
    Write(Phase());
    Write(")\n");
    TerminateProcess(GetCurrentProcess(), exitCode);
}

// With CHAOS_DUMP_DIR set, StartWatchdog starts a copy of the test as a dumper
// that waits for a hang and writes a minidump of the test into that directory,
// from outside the hung process.
inline HANDLE& DumpRequest()
{
    static HANDLE request = NULL;
    return request;
}

inline HANDLE& DumpWritten()
{
    static HANDLE written = NULL;
    return written;
}

inline void DumpEventName(wchar_t* name, size_t size, const wchar_t* kind, DWORD pid)
{
    swprintf_s(name, size, L"Local\\chaos_%s_%lu", kind, pid);
}

// MiniDumpWriteDump, loaded at run time; the MINIDUMP_TYPE values are those of
// MiniDumpWithFullMemory, MiniDumpWithHandleData, MiniDumpWithUnloadedModules
// and MiniDumpWithThreadInfo.
typedef BOOL (WINAPI *MiniDumpWriteDump_t)(HANDLE, DWORD, HANDLE, DWORD, PVOID, PVOID, PVOID);
const DWORD CHAOS_MINIDUMP_TYPE = 0x2 | 0x4 | 0x20 | 0x1000;

// Runs the dumper and exits if this process was started as one.
inline void RunDumperIfRequested()
{
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv == NULL || argc != 3 || wcscmp(argv[1], L"--chaos-dumper") != 0)
    {
        return;
    }
    DWORD pid = wcstoul(argv[2], NULL, 10);
    wchar_t name[64];
    DumpEventName(name, _countof(name), L"hang", pid);
    HANDLE request = OpenEventW(SYNCHRONIZE, FALSE, name);
    DumpEventName(name, _countof(name), L"dumped", pid);
    HANDLE written = OpenEventW(EVENT_MODIFY_STATE, FALSE, name);
    HANDLE test = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_DUP_HANDLE, FALSE, pid);
    if (request == NULL || written == NULL || test == NULL)
    {
        ExitProcess(1);
    }
    HANDLE waits[2] = { request, test };
    if (WaitForMultipleObjects(2, waits, FALSE, INFINITE) == WAIT_OBJECT_0)
    {
        wchar_t dir[MAX_PATH];
        wchar_t exe[MAX_PATH];
        wchar_t path[MAX_PATH * 2];
        DWORD length = GetEnvironmentVariableW(L"CHAOS_DUMP_DIR", dir, _countof(dir));
        GetModuleFileNameW(NULL, exe, _countof(exe));
        const wchar_t* base = wcsrchr(exe, L'\\');
        base = base ? base + 1 : exe;
        HMODULE dbghelp = LoadLibraryW(L"dbghelp.dll");
        MiniDumpWriteDump_t writeDump = dbghelp ? (MiniDumpWriteDump_t)GetProcAddress(dbghelp, "MiniDumpWriteDump") : NULL;
        if (length > 0 && length < _countof(dir) && writeDump != NULL)
        {
            CreateDirectoryW(dir, NULL);
            swprintf_s(path, L"%s\\%s.%lu.dmp", dir, base, pid);
            HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
            if (file != INVALID_HANDLE_VALUE)
            {
                writeDump(test, pid, file, CHAOS_MINIDUMP_TYPE, NULL, NULL, NULL);
                CloseHandle(file);
            }
        }
        SetEvent(written);
    }
    ExitProcess(0);
}

inline void StartDumper()
{
    wchar_t dir[MAX_PATH];
    if (GetEnvironmentVariableW(L"CHAOS_DUMP_DIR", dir, _countof(dir)) == 0)
    {
        return;
    }
    DWORD pid = GetCurrentProcessId();
    wchar_t name[64];
    DumpEventName(name, _countof(name), L"hang", pid);
    HANDLE request = CreateEventW(NULL, TRUE, FALSE, name);
    DumpEventName(name, _countof(name), L"dumped", pid);
    HANDLE written = CreateEventW(NULL, TRUE, FALSE, name);
    wchar_t exe[MAX_PATH];
    wchar_t commandLine[MAX_PATH + 64];
    GetModuleFileNameW(NULL, exe, _countof(exe));
    swprintf_s(commandLine, L"\"%s\" --chaos-dumper %lu", exe, pid);
    STARTUPINFOW startup = { sizeof(startup) };
    PROCESS_INFORMATION process = { 0 };
    if (request == NULL || written == NULL ||
        !CreateProcessW(exe, commandLine, NULL, NULL, FALSE, DETACHED_PROCESS, NULL, NULL, &startup, &process))
    {
        return;
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    DumpWritten() = written;
    DumpRequest() = request;
}

inline DWORD WINAPI WatchdogThread(LPVOID)
{
    if (WaitForSingleObject(WatchdogDone(), (DWORD)WatchdogMs()) == WAIT_TIMEOUT)
    {
        // Allocates nothing: the hung threads may hold the heap or loader lock.
        if (DumpRequest() != NULL && SetEvent(DumpRequest()))
        {
            WaitForSingleObject(DumpWritten(), 120000);
        }
        Fail("HANG detected by the watchdog", 3);
    }
    return 0;
}

inline void StartWatchdog(DWORD ms)
{
    RunDumperIfRequested();
    WatchdogMs() = (LONG)ms;
    StartDumper();
    WatchdogDone() = CreateEventW(NULL, TRUE, FALSE, NULL);
    HANDLE thread = CreateThread(NULL, 0, WatchdogThread, NULL, 0, NULL);
    if (WatchdogDone() == NULL || thread == NULL)
    {
        Fail("could not start the watchdog", 2);
    }
    CloseHandle(thread);
}

inline void StopWatchdog()
{
    SetEvent(WatchdogDone());
}

inline void Check(bool condition, const char* message)
{
    if (!condition)
    {
        Fail(message);
    }
}

// Small deterministic PRNG (xorshift32), one per thread.
struct Random
{
    unsigned int state;
    explicit Random(unsigned int seed) : state(seed ? seed : 0x9E3779B9u) {}
    unsigned int Next()
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    }
    unsigned int Below(unsigned int n) { return Next() % n; }
};

struct RunThreadsStart
{
    LPTHREAD_START_ROUTINE routine;
    volatile LONG started;
    HANDLE go;
};

struct RunThreadsArg
{
    RunThreadsStart* start;
    INT_PTR index;
};

inline DWORD WINAPI RunThreadsTrampoline(LPVOID p)
{
    RunThreadsArg* arg = (RunThreadsArg*)p;
    InterlockedIncrement(&arg->start->started);
    WaitForSingleObject(arg->start->go, INFINITE);
    return arg->start->routine((LPVOID)arg->index);
}

inline void RunThreads(int count, LPTHREAD_START_ROUTINE routine, volatile LONG* stop = NULL, DWORD stopAfterMs = INFINITE)
{
    HANDLE threads[64];
    RunThreadsArg args[64];
    Check(count <= 64, "too many threads");
    RunThreadsStart start = { routine, 0, CreateEventW(NULL, TRUE, FALSE, NULL) };
    Check(start.go != NULL, "CreateEvent failed");
    for (int i = 0; i < count; i++)
    {
        args[i].start = &start;
        args[i].index = i;
        threads[i] = CreateThread(NULL, 0, RunThreadsTrampoline, &args[i], 0, NULL);
        Check(threads[i] != NULL, "CreateThread failed");
    }
    // Starting a thread takes the loader lock, which a worker calling
    // VLDRefreshModules in a loop can keep for itself; so no worker runs until
    // all have started.
    while (InterlockedCompareExchange(&start.started, 0, 0) < count)
    {
        Sleep(1);
    }
    SetEvent(start.go);
    // Backstop for a worker that stops the others but cannot get to it; that
    // worker not having stopped the run by then is a failure.
    bool stopperLate = false;
    if (WaitForMultipleObjects(count, threads, TRUE, stopAfterMs) == WAIT_TIMEOUT && stop != NULL)
    {
        stopperLate = InterlockedExchange(stop, 1) == 0;
    }
    WaitForMultipleObjects(count, threads, TRUE, INFINITE);
    for (int i = 0; i < count; i++)
    {
        CloseHandle(threads[i]);
    }
    CloseHandle(start.go);
    Check(!stopperLate, "the stopping worker did not stop the run in time");
}

inline void Pass()
{
    StopWatchdog();
    Write("CHAOS PASS\n");
}
}
