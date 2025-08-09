#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cassert>
#include <mutex>

namespace vld
{
    // A simple RAII-based critical section wrapper
    class criticalsection
    {
    public:
        criticalsection() noexcept
        {
            InitializeCriticalSection(&m_critRegion);
        }

        ~criticalsection() noexcept
        {
            DeleteCriticalSection(&m_critRegion);
        }

        criticalsection(const criticalsection&) = delete;
        criticalsection& operator=(const criticalsection&) = delete;
        criticalsection(criticalsection&&) = delete;
        criticalsection& operator=(criticalsection&&) = delete;

        void lock() noexcept
        {
            EnterCriticalSection(&m_critRegion);
        }

        bool try_lock() noexcept
        {
            return TryEnterCriticalSection(&m_critRegion) != 0;
        }

        void unlock() noexcept
        {
            LeaveCriticalSection(&m_critRegion);
        }

        bool is_locked_by_current_thread() const noexcept
        {
            // yes, it needs to be compared with the id and not the handle: 
            // https://stackoverflow.com/questions/12675301/why-is-the-owningthread-member-of-critical-section-of-type-handle-when-it-is-de
            return m_critRegion.OwningThread == ULongToHandle(GetCurrentThreadId());
        }

    private:
		// see this discussion on using std::mutex, CRITICAL_SECTION, and SRWLOCK:
        // https://stoyannk.wordpress.com/2016/04/30/msvc-mutex-is-slower-than-you-might-expect/
        CRITICAL_SECTION m_critRegion;
    };
}