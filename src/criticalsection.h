#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

// you should consider CriticalSectionLocker<> whenever possible instead of
// directly working with CriticalSection class - it is safer
class CriticalSection
{
public:
	void Initialize()
	{
		m_ownerThreadId = 0;
		__try {
			// Without debug info: Windows otherwise allocates it from the process
			// heap the first time the section is contended, which deadlocks when
			// another thread holds the heap lock (HeapLock) and waits for this
			// section.
			if (!InitializeCriticalSectionEx(&m_critRegion, 0, CRITICAL_SECTION_NO_DEBUG_INFO))
				InitializeCriticalSection(&m_critRegion);
		} __except (GetExceptionCode() == STATUS_NO_MEMORY ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
			assert(FALSE);
		}
	}
	void Delete()		{ DeleteCriticalSection(&m_critRegion); }

	// enter the section
	void Enter()
	{
		EnterCriticalSection(&m_critRegion);
		m_ownerThreadId = GetCurrentThreadId();
	}

	bool IsLocked()
	{
		return (m_ownerThreadId != 0);
	}

	bool IsLockedByCurrentThread()
	{
		return m_ownerThreadId == GetCurrentThreadId();
	}

	// Unlike IsLockedByCurrentThread, stays true after a nested Leave.
	bool IsOwnedByCurrentThread()
	{
		return (DWORD)(ULONG_PTR)m_critRegion.OwningThread == GetCurrentThreadId();
	}

	// True if the next Leave releases the section.
	bool IsOwnedOnceByCurrentThread()
	{
		return IsOwnedByCurrentThread() && m_critRegion.RecursionCount == 1;
	}

	// try enter the section
	bool TryEnter()
	{
		if (TryEnterCriticalSection(&m_critRegion) != 0)
		{
			m_ownerThreadId = GetCurrentThreadId();
			return true;
		}
		return false;
	}

	// leave the critical section
	void Leave()
	{
		m_ownerThreadId = 0;
		LeaveCriticalSection(&m_critRegion);
	}

private:
	CRITICAL_SECTION m_critRegion;
	volatile DWORD m_ownerThreadId;
};

template<typename T = CriticalSection>
class CriticalSectionLocker
{
public:
	CriticalSectionLocker(T& cs)
		: m_leave(false)
		, m_critSect(cs)
	{
		m_critSect.Enter();
	}

	// Takes over a lock the caller has already entered.
	struct Adopt {};
	CriticalSectionLocker(T& cs, Adopt)
		: m_leave(false)
		, m_critSect(cs)
	{
	}

	~CriticalSectionLocker()
	{
		LeaveLock();
	}

	void Leave()
	{
		LeaveLock();
	}

private:
	void LeaveLock()
	{
		if (!m_leave)
		{
			m_critSect.Leave();
			m_leave = true;
		}
	}
	CriticalSectionLocker(); // not allowed
	CriticalSectionLocker( const CriticalSectionLocker & ); // not allowed
	CriticalSectionLocker & operator=( const CriticalSectionLocker & ); // not allowed
	bool m_leave;
    T& m_critSect;
};
