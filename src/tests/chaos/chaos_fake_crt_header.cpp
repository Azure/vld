// chaos_fake_crt_header.cpp - user data that looks like a CRT debug header
// must not hide a leak.
//
// Uninitialized heap memory holds whatever was there before, such as a freed
// block's CRT debug header. A leaked block whose first bytes look like a
// header of a freed or CRT-internal block must still be counted.

#include "chaos_common.h"
#include "vld.h"
#include <stdlib.h>

namespace
{
// The UCRT debug heap block header.
struct FakeHeader
{
    void* next;
    void* prev;
    const char* file;
    int line;
    int use;
    size_t size;
    long request;
    unsigned char gap[4];
};

const int USES[] = { 0 /* free */, 2 /* CRT */, 3 /* ignore */ };
const size_t BLOCK_SIZE = 96;
}

int main()
{
    chaos::StartWatchdog(20000);
    VLDMarkAllLeaksAsReported();

    chaos::SetPhase("leaking blocks that look like CRT debug headers");
    void* blocks[_countof(USES)];
    for (int i = 0; i < _countof(USES); i++)
    {
        blocks[i] = malloc(BLOCK_SIZE);
        chaos::Check(blocks[i] != NULL, "malloc failed");
        memset(blocks[i], 0, BLOCK_SIZE);
        FakeHeader* header = (FakeHeader*)blocks[i];
        header->use = USES[i];
        header->size = BLOCK_SIZE - sizeof(FakeHeader) - 4;
        memset(header->gap, 0xFD, sizeof(header->gap));
    }

    chaos::SetPhase("counting");
    UINT counted = VLDGetLeaksCount();
    if (counted != _countof(USES))
    {
        char message[96];
        sprintf_s(message, "leaked %d blocks but VLD counts %u", (int)_countof(USES), counted);
        chaos::Fail(message);
    }

    chaos::SetPhase("a header that changes between queries");
    void* changing = malloc(BLOCK_SIZE);
    chaos::Check(changing != NULL, "malloc failed");
    memset(changing, 0, BLOCK_SIZE);
    FakeHeader* header = (FakeHeader*)changing;
    header->use = 1;
    header->size = BLOCK_SIZE - sizeof(FakeHeader) - 4;
    memset(header->gap, 0xFD, sizeof(header->gap));
    UINT before = VLDGetLeaksCount();
    header->use = 0;
    UINT after = VLDGetLeaksCount();
    if (before != _countof(USES) + 1 || after != before)
    {
        char message[128];
        sprintf_s(message, "counted %u leaks, then %u after the fake header changed; expected %d", before, after, (int)_countof(USES) + 1);
        chaos::Fail(message);
    }
    free(changing);

    chaos::SetPhase("cleanup");
    for (int i = 0; i < _countof(USES); i++)
    {
        free(blocks[i]);
    }
    chaos::Pass();
    return 0;
}
