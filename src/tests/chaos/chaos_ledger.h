// chaos_ledger.h - records the blocks a chaos test leaks on purpose, and on a
// miscount lists the ones VLD does not report.

#pragma once

#include "chaos_common.h"
#include <stdlib.h>
#include "vld.h"

namespace chaos
{
const LONG LEDGER_SIZE = 8192;

struct LedgerEntry
{
    void* volatile block;
    volatile LONG tag;
};

inline LedgerEntry* Ledger()
{
    static LedgerEntry entries[LEDGER_SIZE];
    return entries;
}

inline volatile LONG& LedgerCount()
{
    static volatile LONG count = 0;
    return count;
}

// The tag says where the block came from; it is printed if the block is missed.
inline void RecordLeak(void* block, LONG tag)
{
    LONG slot = InterlockedIncrement(&LedgerCount()) - 1;
    if (slot < LEDGER_SIZE)
    {
        Ledger()[slot].tag = tag;
        Ledger()[slot].block = block;
    }
}

inline void** ReportedBlocks()
{
    static void* blocks[LEDGER_SIZE * 2];
    return blocks;
}

inline volatile LONG& ReportedCount()
{
    static volatile LONG count = 0;
    return count;
}

inline int __cdecl CollectReportedBlock(int, wchar_t* message, int*)
{
    const wchar_t* at = wcsstr(message, L"---------- Block ");
    if (at != NULL)
    {
        at = wcsstr(at, L" at 0x");
        if (at != NULL)
        {
            ULONG_PTR address = (ULONG_PTR)_wcstoui64(at + 6, NULL, 16);
            LONG slot = InterlockedIncrement(&ReportedCount()) - 1;
            if (slot < LEDGER_SIZE * 2)
                ReportedBlocks()[slot] = (void*)address;
        }
    }
    return 1;
}

// Reports the leaks through a collecting hook and prints every recorded block
// the report does not list, with its tag and first bytes. Returns how many
// are missing. Other report hooks must be removed first, or they may swallow
// the report. A CRT block is listed at its data, at most 64 bytes past the
// heap block.
inline int DescribeMissingLeaks()
{
    InterlockedExchange(&ReportedCount(), 0);
    VLDSetReportHook(VLD_RPTHOOK_INSTALL, CollectReportedBlock);
    VLDReportLeaks();
    VLDSetReportHook(VLD_RPTHOOK_REMOVE, CollectReportedBlock);
    LONG reported = min(ReportedCount(), LEDGER_SIZE * 2);
    LONG recorded = min(LedgerCount(), LEDGER_SIZE);
    char line[256];
    sprintf_s(line, "diagnostic report lists %ld blocks, %ld recorded\n", reported, recorded);
    Write(line);
    int missing = 0;
    for (LONG i = 0; i < recorded; i++)
    {
        const LedgerEntry& entry = Ledger()[i];
        bool found = false;
        for (LONG j = 0; j < reported && !found; j++)
            found = (ULONG_PTR)entry.block - (ULONG_PTR)ReportedBlocks()[j] <= 64;
        if (found)
            continue;
        if (++missing > 40)
            continue;
        const unsigned char* bytes = (const unsigned char*)entry.block;
        int length = sprintf_s(line, "missing #%ld tag %ld at %p:", i, entry.tag, bytes);
        __try
        {
            for (int b = 0; b < 16; b++)
                length += sprintf_s(line + length, _countof(line) - length, " %02X", bytes[b]);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            length += sprintf_s(line + length, _countof(line) - length, " (not readable)");
        }
        sprintf_s(line + length, _countof(line) - length, "\n");
        Write(line);
    }
    sprintf_s(line, "%d blocks missing from the diagnostic report\n", missing);
    Write(line);
    return missing;
}
}
