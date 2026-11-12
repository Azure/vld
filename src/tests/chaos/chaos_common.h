// chaos_common.h - shared helpers for the VLD chaos tests.
//
// Every chaos test is a console program that ends with "CHAOS PASS" on
// success. A watchdog turns a hang into a failure: it prints the test's
// current phase and terminates the process with exit code 3, without running
// any exit code that could itself need a lock held by the hung threads.

#pragma once

#include <windows.h>
#include <stdio.h>
#include <string.h>

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

// Prints "FAIL: <message>" and terminates without running exit handlers.
inline void Fail(const char* message, UINT exitCode = 1)
{
    char line[512];
    _snprintf_s(line, _TRUNCATE, "FAIL: %s (phase: %s)\n", message, Phase());
    Write(line);
    TerminateProcess(GetCurrentProcess(), exitCode);
}

inline DWORD WINAPI WatchdogThread(LPVOID)
{
    if (WaitForSingleObject(WatchdogDone(), (DWORD)WatchdogMs()) == WAIT_TIMEOUT)
    {
        Fail("HANG detected by the watchdog", 3);
    }
    return 0;
}

inline void StartWatchdog(DWORD ms)
{
    WatchdogMs() = (LONG)ms;
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

inline void RunThreads(int count, LPTHREAD_START_ROUTINE routine)
{
    HANDLE threads[64];
    Check(count <= 64, "too many threads");
    for (int i = 0; i < count; i++)
    {
        threads[i] = CreateThread(NULL, 0, routine, (LPVOID)(INT_PTR)i, 0, NULL);
        Check(threads[i] != NULL, "CreateThread failed");
    }
    WaitForMultipleObjects(count, threads, TRUE, INFINITE);
    for (int i = 0; i < count; i++)
    {
        CloseHandle(threads[i]);
    }
}

inline void Pass()
{
    StopWatchdog();
    Write("CHAOS PASS\n");
}
}
