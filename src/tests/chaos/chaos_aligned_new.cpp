// chaos_aligned_new.cpp - leaks from the over-aligned and nothrow forms of
// operator new must be counted.
//
// C++17 routes new of an over-aligned type to operator new(size_t,
// align_val_t), a separate export from the plain operator new.

#include "chaos_common.h"
#include "vld.h"
#include <new>

namespace
{
struct alignas(64) Wide
{
    char bytes[64];
};

struct alignas(256) Wider
{
    char bytes[24];
};
}

int main()
{
    chaos::StartWatchdog(20000);
    VLDMarkAllLeaksAsReported();

    chaos::SetPhase("leaking over-aligned objects");
    Wide* wide = new Wide;
    Wider* wider = new Wider[3];
    Wide* nothrowWide = new (std::nothrow) Wide;
    void* raw = ::operator new(40, std::align_val_t(128));
    chaos::Check(wide != NULL && wider != NULL && nothrowWide != NULL && raw != NULL, "allocation failed");

    chaos::SetPhase("counting");
    UINT counted = VLDGetLeaksCount();
    if (counted != 4)
    {
        char message[96];
        sprintf_s(message, "leaked 4 over-aligned blocks but VLD counts %u", counted);
        chaos::Fail(message);
    }

    chaos::SetPhase("freeing");
    delete wide;
    delete[] wider;
    delete nothrowWide;
    ::operator delete(raw, std::align_val_t(128));
    chaos::Check(VLDGetLeaksCount() == 0, "freed blocks are still counted");
    chaos::Pass();
    return 0;
}
