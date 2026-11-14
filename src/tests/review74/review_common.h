// review_common.h - helpers for the review74 tests.
//
// Each review74 test checks one PR #74 review comment and fails without its
// fix. Tests that need a separate process start a copy of themselves with
// "--child <mode>" and check its exit code or its debugger output.

#pragma once

#include "../chaos/chaos_common.h"

namespace review
{
// Returns the child mode of this process, or NULL in the parent.
inline const wchar_t* ChildMode()
{
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv == NULL || argc != 3 || wcscmp(argv[1], L"--child") != 0)
    {
        return NULL;
    }
    return argv[2];
}

inline bool StartChild(const wchar_t* mode, DWORD creationFlags, PROCESS_INFORMATION& process)
{
    wchar_t exe[MAX_PATH];
    wchar_t commandLine[MAX_PATH + 64];
    GetModuleFileNameW(NULL, exe, _countof(exe));
    swprintf_s(commandLine, L"\"%s\" --child %s", exe, mode);
    STARTUPINFOW startup = { sizeof(startup) };
    ZeroMemory(&process, sizeof(process));
    return CreateProcessW(exe, commandLine, NULL, NULL, FALSE, creationFlags, NULL, NULL, &startup, &process) != FALSE;
}

// Runs a copy of this test in the given child mode and returns its exit code.
inline DWORD RunChild(const wchar_t* mode, DWORD timeoutMs)
{
    PROCESS_INFORMATION process;
    chaos::Check(StartChild(mode, 0, process), "could not start the child process");
    CloseHandle(process.hThread);
    if (WaitForSingleObject(process.hProcess, timeoutMs) == WAIT_TIMEOUT)
    {
        TerminateProcess(process.hProcess, 99);
        WaitForSingleObject(process.hProcess, INFINITE);
    }
    DWORD exitCode = 0;
    GetExitCodeProcess(process.hProcess, &exitCode);
    CloseHandle(process.hProcess);
    return exitCode;
}

// Runs a copy of this test under this process as its debugger and collects
// what it sends to the debugger (OutputDebugString). Returns its exit code.
inline DWORD RunChildUnderDebugger(const wchar_t* mode, wchar_t* text, size_t textSize, DWORD timeoutMs)
{
    PROCESS_INFORMATION process;
    chaos::Check(StartChild(mode, DEBUG_ONLY_THIS_PROCESS, process), "could not start the child process");
    size_t length = 0;
    text[0] = L'\0';
    ULONGLONG deadline = GetTickCount64() + timeoutMs;
    DWORD exitCode = 99;
    for (;;)
    {
        DEBUG_EVENT event;
        if (!WaitForDebugEvent(&event, 1000))
        {
            if (GetTickCount64() > deadline)
            {
                TerminateProcess(process.hProcess, 99);
            }
            continue;
        }
        DWORD status = DBG_CONTINUE;
        switch (event.dwDebugEventCode)
        {
        case OUTPUT_DEBUG_STRING_EVENT:
        {
            const OUTPUT_DEBUG_STRING_INFO& info = event.u.DebugString;
            BYTE buffer[8192];
            SIZE_T bytes = info.nDebugStringLength * (info.fUnicode ? sizeof(WCHAR) : 1);
            if (bytes > sizeof(buffer))
                bytes = sizeof(buffer);
            SIZE_T read = 0;
            if (ReadProcessMemory(process.hProcess, info.lpDebugStringData, buffer, bytes, &read) && read > 0)
            {
                for (SIZE_T i = 0; i < (info.fUnicode ? read / sizeof(WCHAR) : read) && length + 1 < textSize; i++)
                {
                    WCHAR c = info.fUnicode ? ((WCHAR*)buffer)[i] : (WCHAR)buffer[i];
                    if (c != L'\0')
                        text[length++] = c;
                }
                text[length] = L'\0';
            }
            break;
        }
        case EXCEPTION_DEBUG_EVENT:
            // The loader's initial breakpoint is expected; anything else is
            // left to the child.
            if (event.u.Exception.ExceptionRecord.ExceptionCode != EXCEPTION_BREAKPOINT &&
                event.u.Exception.ExceptionRecord.ExceptionCode != 0x4000001F) // STATUS_WX86_BREAKPOINT
                status = DBG_EXCEPTION_NOT_HANDLED;
            break;
        case CREATE_PROCESS_DEBUG_EVENT:
            if (event.u.CreateProcessInfo.hFile != NULL)
                CloseHandle(event.u.CreateProcessInfo.hFile);
            break;
        case LOAD_DLL_DEBUG_EVENT:
            if (event.u.LoadDll.hFile != NULL)
                CloseHandle(event.u.LoadDll.hFile);
            break;
        case EXIT_PROCESS_DEBUG_EVENT:
            exitCode = event.u.ExitProcess.dwExitCode;
            break;
        }
        ContinueDebugEvent(event.dwProcessId, event.dwThreadId, status);
        if (event.dwDebugEventCode == EXIT_PROCESS_DEBUG_EVENT)
            break;
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return exitCode;
}
}
