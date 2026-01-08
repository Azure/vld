// Test that exposes TLS initialization reentrancy bug in old VLD code
// 
// The bug: When a new thread's first allocation happens, getTls() needs to
// allocate a tls_t structure. In the old code, the reentrancy check happened
// AFTER calling getTls(), so the allocation of tls_t itself would trigger
// infinite recursion.
//
// Old code flow (crashes):
//   1. Thread calls malloc() -> _RtlAllocateHeap
//   2. _RtlAllocateHeap calls getTls()
//   3. getTls() sees tls==NULL, calls new tls_t
//   4. new tls_t triggers _RtlAllocateHeap again
//   5. _RtlAllocateHeap calls getTls() again (TLS not set yet!)
//   6. INFINITE RECURSION -> stack overflow
//
// New code with s_inVldCall (works):
//   1. Thread calls malloc() -> _RtlAllocateHeap
//   2. _RtlAllocateHeap calls getTls()
//   3. getTls() increments s_inVldCall, calls new tls_t
//   4. new tls_t triggers _RtlAllocateHeap again
//   5. _RtlAllocateHeap sees s_inVldCall > 0, skips tracking, returns
//   6. getTls() completes, decrements s_inVldCall
//   7. Original allocation proceeds normally

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
    // Old VLD code: This will crash with stack overflow during getTls()
    // New VLD code: s_inVldCall protects getTls() from reentrancy
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
    printf("TLS Initialization Reentrancy Test\n");
    printf("===================================\n");
    printf("This test exposes a bug in old VLD where getTls() could recurse infinitely.\n");
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
