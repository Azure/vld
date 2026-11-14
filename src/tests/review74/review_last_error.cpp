// review_last_error.cpp - PR #74 review comment on vld.cpp:1703.
//
// VLD's hooks must leave the thread's last error alone: code such as
// "if (!ReadFile(...)) { free(buffer); return GetLastError(); }" must see the
// error of the call that failed, not the 0 that TlsGetValue stores on success.

#include "review_common.h"
#include "vld.h"
#include <stdlib.h>

namespace
{
void CheckPreserved(DWORD expected, const char* message)
{
    chaos::Check(GetLastError() == expected, message);
}
}

int main()
{
    chaos::StartWatchdog(20000);
    VLDMarkAllLeaksAsReported();

    // Each call runs on a fresh thread too, whose TLS VLD creates on first use.
    for (int pass = 0; pass < 2; pass++)
    {
        chaos::SetPhase("HeapAlloc and HeapFree");
        SetLastError(ERROR_HANDLE_EOF);
        void* block = HeapAlloc(GetProcessHeap(), 0, 32);
        CheckPreserved(ERROR_HANDLE_EOF, "HeapAlloc changed the last error");
        chaos::Check(block != NULL, "HeapAlloc failed");
        SetLastError(ERROR_ACCESS_DENIED);
        HeapFree(GetProcessHeap(), 0, block);
        CheckPreserved(ERROR_ACCESS_DENIED, "HeapFree changed the last error");

        chaos::SetPhase("malloc and free");
        SetLastError(ERROR_BROKEN_PIPE);
        void* crtBlock = malloc(48);
        CheckPreserved(ERROR_BROKEN_PIPE, "malloc changed the last error");
        chaos::Check(crtBlock != NULL, "malloc failed");
        SetLastError(ERROR_FILE_NOT_FOUND);
        free(crtBlock);
        CheckPreserved(ERROR_FILE_NOT_FOUND, "free changed the last error");

        chaos::SetPhase("HeapReAlloc");
        void* moved = HeapAlloc(GetProcessHeap(), 0, 16);
        chaos::Check(moved != NULL, "HeapAlloc failed");
        SetLastError(ERROR_SHARING_VIOLATION);
        moved = HeapReAlloc(GetProcessHeap(), 0, moved, 64 * 1024);
        CheckPreserved(ERROR_SHARING_VIOLATION, "HeapReAlloc changed the last error");
        chaos::Check(moved != NULL, "HeapReAlloc failed");
        HeapFree(GetProcessHeap(), 0, moved);

        if (pass == 0)
        {
            HANDLE thread = CreateThread(NULL, 0, [](LPVOID) -> DWORD {
                SetLastError(ERROR_INVALID_DATA);
                void* block = HeapAlloc(GetProcessHeap(), 0, 24);
                if (GetLastError() != ERROR_INVALID_DATA)
                    chaos::Fail("the first allocation of a thread changed the last error");
                HeapFree(GetProcessHeap(), 0, block);
                return 0;
            }, NULL, 0, NULL);
            chaos::Check(thread != NULL, "CreateThread failed");
            WaitForSingleObject(thread, INFINITE);
            CloseHandle(thread);
        }
    }

    chaos::Check(VLDGetLeaksCount() == 0, "a freed block is still counted");
    chaos::Pass();
    return 0;
}
