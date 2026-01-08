// Multi-threaded TLS initialization stress test
// 
// This test spawns multiple threads that simultaneously perform their first
// allocations, stressing VLD's per-thread TLS (Thread Local Storage) initialization.
//
// What it tests:
//   - Multiple threads calling getTls() concurrently
//   - VLD's CriticalSection protection during TLS map operations
//   - First allocation in each thread triggers TLS structure creation
//   - Recursive CriticalSection behavior (same thread can re-acquire)
//
// Thread flow:
//   1. Thread calls malloc() -> _RtlAllocateHeap hook
//   2. Hook calls getTls() to get thread-local state
//   3. If first time: getTls() takes m_tlsLock, calls new tls_t
//   4. new tls_t may trigger _RtlAllocateHeap again (recursive)
//   5. Recursive call also calls getTls()
//   6. Windows CriticalSection is recursive - same thread can re-acquire
//   7. Second getTls() finds TLS already in map (inserted by first call)
//   8. Both calls complete successfully
//
// This test verifies VLD handles concurrent TLS initialization across threads
// and recursive calls within a single thread during TLS initialization.

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

// Number of threads to spawn - more threads = more likely to hit the race
#define NUM_THREADS 50

// Simple barrier to make all threads start at once
volatile LONG g_readyCount = 0;
volatile LONG g_goFlag = 0;

DWORD WINAPI ThreadFunc(LPVOID param)
{
    int threadId = (int)(INT_PTR)param;
    
    // Signal we're ready
    InterlockedIncrement(&g_readyCount);
    
    // Wait for all threads to be ready
    while (g_goFlag == 0) {
        Sleep(0);
    }
    
    // This is the first allocation in this thread
    // Tests VLD's handling of concurrent TLS initialization
    void* ptr = malloc(100);
    
    // Do more allocations to stress test
    for (int i = 0; i < 10; i++) {
        void* temp = malloc(i * 10 + 1);
        free(temp);
    }
    
    free(ptr);
    
    return 0;
}

int main()
{
    printf("TLS Initialization Stress Test\n");
    printf("===============================\n");
    printf("Testing VLD's handling of concurrent TLS initialization.\n");
    printf("Creating %d threads that will all allocate simultaneously...\n\n", NUM_THREADS);
    
    HANDLE threads[NUM_THREADS];
    
    // Create all threads (they'll wait at barrier)
    for (int i = 0; i < NUM_THREADS; i++) {
        threads[i] = CreateThread(NULL, 0, ThreadFunc, (LPVOID)(INT_PTR)i, 0, NULL);
        if (threads[i] == NULL) {
            printf("Failed to create thread %d\n", i);
            return 1;
        }
    }
    
    // Wait for all threads to be ready
    while (g_readyCount < NUM_THREADS) {
        Sleep(1);
    }
    
    printf("All threads ready, releasing them simultaneously...\n");
    
    // Release all threads at once to maximize race condition
    g_goFlag = 1;
    
    // Wait for all threads to complete
    DWORD result = WaitForMultipleObjects(NUM_THREADS, threads, TRUE, 30000); // 30 second timeout
    
    if (result == WAIT_TIMEOUT) {
        printf("\nFAILED: Threads deadlocked or hung (likely infinite recursion)\n");
        printf("This indicates the TLS initialization bug is present.\n");
        return 1;
    } else if (result >= WAIT_OBJECT_0 && result < WAIT_OBJECT_0 + NUM_THREADS) {
        printf("SUCCESS: All threads completed without crashing\n");
        printf("The s_inVldCall reentrancy protection is working correctly.\n");
    } else {
        printf("FAILED: WaitForMultipleObjects returned unexpected result: 0x%X\n", result);
        return 1;
    }
    
    // Clean up
    for (int i = 0; i < NUM_THREADS; i++) {
        CloseHandle(threads[i]);
    }
    
    printf("\nTest PASSED: TLS initialization is properly protected from reentrancy\n");
    return 0;
}
