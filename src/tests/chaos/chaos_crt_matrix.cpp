// chaos_crt_matrix.cpp - exact leak counts through every CRT allocation shape.
//
// Each step allocates through one CRT entry point (including the edge cases
// realloc(NULL), realloc(p, 0), _recalloc, _expand and the aligned family)
// and checks VLD's count before and after freeing. A block must be counted
// exactly once while live and never after it is freed or moved.

#include "chaos_common.h"
#include <malloc.h>
#include <string.h>
#include <wchar.h>
#include "vld.h"

namespace
{
SIZE_T g_base = 0;

void ExpectLive(SIZE_T live, const char* step)
{
    SIZE_T leaks = VLDGetLeaksCount();
    if (leaks != g_base + live)
    {
        char message[256];
        _snprintf_s(message, _TRUNCATE, "%s: expected %zu live blocks, VLD counts %zd", step, live,
            (ptrdiff_t)(leaks - g_base));
        chaos::Fail(message);
    }
}
}

int main()
{
    chaos::StartWatchdog(20000);
    g_base = VLDGetLeaksCount();

    for (int round = 0; round < 50; round++)
    {
        chaos::SetPhase("realloc(NULL, n) and realloc(p, 0)");
        void* p = realloc(NULL, 73 + round);
        ExpectLive(1, "realloc(NULL)");
        p = realloc(p, 4096 + round);
        ExpectLive(1, "realloc grow");
        p = realloc(p, 8);
        ExpectLive(1, "realloc shrink");
        p = realloc(p, 0);
        if (p != NULL)
        {
            free(p);
        }
        ExpectLive(0, "realloc(p, 0)");

        chaos::SetPhase("calloc and _recalloc");
        p = calloc(7, 9);
        ExpectLive(1, "calloc");
        p = _recalloc(p, 70, 9);
        ExpectLive(1, "_recalloc grow");
        free(p);
        ExpectLive(0, "_recalloc free");

        chaos::SetPhase("_expand and _msize");
        p = malloc(256);
        void* expanded = _expand(p, 128);
        chaos::Check(_msize(expanded != NULL ? expanded : p) >= 128, "_msize after _expand");
        ExpectLive(1, "_expand");
        free(expanded != NULL ? expanded : p);
        ExpectLive(0, "_expand free");

        chaos::SetPhase("aligned allocations");
        void* a = _aligned_malloc(100, 64);
        chaos::Check(((UINT_PTR)a & 63) == 0, "_aligned_malloc alignment");
        ExpectLive(1, "_aligned_malloc");
        a = _aligned_realloc(a, 5000, 64);
        chaos::Check(((UINT_PTR)a & 63) == 0, "_aligned_realloc alignment");
        ExpectLive(1, "_aligned_realloc");
        a = _aligned_recalloc(a, 10, 100, 64);
        ExpectLive(1, "_aligned_recalloc");
        void* o = _aligned_offset_malloc(200, 32, 8);
        ExpectLive(2, "_aligned_offset_malloc");
        _aligned_free(o);
        _aligned_free(a);
        ExpectLive(0, "_aligned_free");

        chaos::SetPhase("strdup family and new[]");
        char* s = _strdup("chaos");
        wchar_t* w = _wcsdup(L"chaos");
        int* n = new int[17];
        char* single = new char;
        ExpectLive(4, "_strdup/_wcsdup/new[]/new");
        free(s);
        free(w);
        delete[] n;
        delete single;
        ExpectLive(0, "strdup family freed");
    }
    chaos::Pass();
    return 0;
}
