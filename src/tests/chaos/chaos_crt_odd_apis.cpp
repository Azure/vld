// chaos_crt_odd_apis.cpp - the less common CRT allocation APIs must be
// tracked exactly.
//
// Several threads allocate through _expand, _recalloc, the aligned family,
// _strdup/_wcsdup, nothrow new and _malloca, free most blocks and
// keep a known number. VLD must count exactly the kept blocks.

#include "chaos_common.h"
#include "vld.h"
#include <malloc.h>
#include <new>
#include <stdlib.h>

namespace
{
const int THREADS = 8;
const int ROUNDS = 400;

struct Kept
{
    DWORD id;
    int count;
    void* blocks[ROUNDS];
    bool aligned[ROUNDS];
    bool isNew[ROUNDS];
};
Kept g_kept[THREADS];

void Release(void* block, bool aligned, bool isNew)
{
    if (aligned)
        _aligned_free(block);
    else if (isNew)
        delete[] (char*)block;
    else
        free(block);
}

DWORD WINAPI Worker(LPVOID p)
{
    int index = (int)(INT_PTR)p;
    Kept* kept = &g_kept[index];
    kept->id = GetCurrentThreadId();
    chaos::Random random(index * 7919 + 17);
    for (int round = 0; round < ROUNDS; round++)
    {
        void* block = NULL;
        bool aligned = false;
        bool isNew = false;
        switch (random.Below(8))
        {
        case 0:
            block = malloc(256);
            if (block != NULL && _expand(block, 64 + random.Below(150)) == NULL)
            {
                // The block stays as it was.
            }
            break;
        case 1:
            block = _recalloc(NULL, 4, 16);
            block = _recalloc(block, 8 + random.Below(64), 16);
            break;
        case 2:
            block = _aligned_malloc(40 + random.Below(400), (size_t)16 << random.Below(5));
            block = _aligned_realloc(block, 900 + random.Below(900), 64);
            aligned = true;
            break;
        case 3:
            block = _aligned_offset_malloc(100, 32, 8);
            block = _aligned_offset_realloc(block, 2000, 32, 8);
            aligned = true;
            break;
        case 4:
            block = _strdup("chaos_crt_odd_apis");
            break;
        case 5:
            block = _wcsdup(L"chaos_crt_odd_apis");
            break;
        case 6:
            block = _aligned_recalloc(NULL, 3, 40, 32);
            block = _aligned_recalloc(block, 30 + random.Below(60), 40, 32);
            aligned = true;
            break;
        default:
        {
            block = new (std::nothrow) char[1 + random.Below(500)];
            isNew = true;
            void* scratch = _malloca(64 + random.Below(4000));
            _freea(scratch);
            break;
        }
        }
        if (block == NULL)
            continue;
        if (random.Below(4) == 0)
        {
            kept->blocks[kept->count] = block;
            kept->aligned[kept->count] = aligned;
            kept->isNew[kept->count] = isNew;
            kept->count++;
        }
        else
        {
            Release(block, aligned, isNew);
        }
    }
    return 0;
}
}

int main()
{
    chaos::StartWatchdog(20000);
    VLDMarkAllLeaksAsReported();

    chaos::SetPhase("allocating through odd CRT APIs");
    chaos::RunThreads(THREADS, Worker);

    chaos::SetPhase("counting kept blocks");
    for (int i = 0; i < THREADS; i++)
    {
        UINT counted = VLDGetThreadLeaksCount(g_kept[i].id);
        if (counted != (UINT)g_kept[i].count)
        {
            char message[128];
            sprintf_s(message, "thread %d kept %d blocks but VLD counts %u", i, g_kept[i].count, counted);
            chaos::Fail(message);
        }
    }

    chaos::SetPhase("freeing kept blocks");
    for (int i = 0; i < THREADS; i++)
    {
        for (int j = 0; j < g_kept[i].count; j++)
        {
            Release(g_kept[i].blocks[j], g_kept[i].aligned[j], g_kept[i].isNew[j]);
        }
        chaos::Check(VLDGetThreadLeaksCount(g_kept[i].id) == 0, "freed blocks are still counted");
    }
    chaos::Pass();
    return 0;
}
