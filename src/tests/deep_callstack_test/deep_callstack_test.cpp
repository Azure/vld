// deep_callstack_test.cpp
// Test to expose crash caused by excessively long stack traces (PR #37)
//
// PROBLEM:
// VLD's Print() function in utility.cpp uses wcstombs_s() with _TRUNCATE to
// convert Unicode messages to ANSI. The output buffer is MAXMESSAGELENGTH=5119
// bytes. When the message exceeds this size, wcstombs_s() returns STRUNCATE
// (not 0) to indicate successful truncation. The old code treated STRUNCATE
// as an error and called assert(err == 0), causing a crash.
//
// TEST STRATEGY:
// This test creates deeply nested function calls to generate long call stacks,
// then triggers a memory leak to force VLD to print the full stack trace.
//
// BUFFER SIZE CALCULATION:
// - MAXMESSAGELENGTH = 5119 bytes (defined in utility.cpp line ~896)
// - Each stack frame prints approximately:
//       "    recursive_func_15() + 0x42 (File: deep_callstack_test.cpp, Line: 78)\n"
//   which is roughly 70-80 characters per frame
// - 300 frames × ~70 chars = ~21,000 characters (about 4× the 5119 byte buffer)
// - This provides comfortable margin to reliably trigger truncation
//
// NOTE: This is an empirical estimate, not a programmatic check. The test uses
// 300 frames as a "definitely big enough" depth based on the calculation above.
//
// SUCCESS CRITERIA:
// The test completes and prints "Test completed successfully!" without crashing.
// If the STRUNCATE bug exists, this test will crash with an assertion failure.

#include <windows.h>
#include <cstdlib>
#include <cstdio>

// VLD_FORCE_ENABLE is defined via CMake for non-Debug builds
#include <vld.h>

// Use volatile to prevent compiler from optimizing away the recursion
volatile int g_depth = 0;
volatile void* g_leak = nullptr;

// Forward declarations for the recursive chain
// We use multiple functions to create unique stack frames with different names
// This makes the stack trace output longer (each frame has a different function name)

#define DECLARE_RECURSIVE_FUNC(N) \
    __declspec(noinline) void recursive_func_##N(int depth);

#define DEFINE_RECURSIVE_FUNC(N, NEXT) \
    __declspec(noinline) void recursive_func_##N(int depth) { \
        volatile char local_buffer[64] = {0}; \
        local_buffer[0] = (char)depth; \
        g_depth = depth; \
        if (depth > 0) { \
            recursive_func_##NEXT(depth - 1); \
        } else { \
            /* At the bottom of the recursion, allocate memory and leak it */ \
            g_leak = malloc(42); \
            printf("Leaked memory at depth %d, address: %p\n", g_depth, g_leak); \
        } \
    }

// Declare 20 different recursive functions to create varied stack frames
DECLARE_RECURSIVE_FUNC(0)
DECLARE_RECURSIVE_FUNC(1)
DECLARE_RECURSIVE_FUNC(2)
DECLARE_RECURSIVE_FUNC(3)
DECLARE_RECURSIVE_FUNC(4)
DECLARE_RECURSIVE_FUNC(5)
DECLARE_RECURSIVE_FUNC(6)
DECLARE_RECURSIVE_FUNC(7)
DECLARE_RECURSIVE_FUNC(8)
DECLARE_RECURSIVE_FUNC(9)
DECLARE_RECURSIVE_FUNC(10)
DECLARE_RECURSIVE_FUNC(11)
DECLARE_RECURSIVE_FUNC(12)
DECLARE_RECURSIVE_FUNC(13)
DECLARE_RECURSIVE_FUNC(14)
DECLARE_RECURSIVE_FUNC(15)
DECLARE_RECURSIVE_FUNC(16)
DECLARE_RECURSIVE_FUNC(17)
DECLARE_RECURSIVE_FUNC(18)
DECLARE_RECURSIVE_FUNC(19)

// Define the recursive chain - each function calls the next
DEFINE_RECURSIVE_FUNC(0, 1)
DEFINE_RECURSIVE_FUNC(1, 2)
DEFINE_RECURSIVE_FUNC(2, 3)
DEFINE_RECURSIVE_FUNC(3, 4)
DEFINE_RECURSIVE_FUNC(4, 5)
DEFINE_RECURSIVE_FUNC(5, 6)
DEFINE_RECURSIVE_FUNC(6, 7)
DEFINE_RECURSIVE_FUNC(7, 8)
DEFINE_RECURSIVE_FUNC(8, 9)
DEFINE_RECURSIVE_FUNC(9, 10)
DEFINE_RECURSIVE_FUNC(10, 11)
DEFINE_RECURSIVE_FUNC(11, 12)
DEFINE_RECURSIVE_FUNC(12, 13)
DEFINE_RECURSIVE_FUNC(13, 14)
DEFINE_RECURSIVE_FUNC(14, 15)
DEFINE_RECURSIVE_FUNC(15, 16)
DEFINE_RECURSIVE_FUNC(16, 17)
DEFINE_RECURSIVE_FUNC(17, 18)
DEFINE_RECURSIVE_FUNC(18, 19)

// Last function in the chain - allocates the leak
__declspec(noinline) void recursive_func_19(int depth) {
    volatile char local_buffer[64] = {0};
    local_buffer[0] = (char)depth;
    g_depth = depth;
    if (depth > 0) {
        // Loop back to the first function to continue recursion
        recursive_func_0(depth - 1);
    } else {
        // At the bottom of the recursion, allocate memory and leak it
        g_leak = malloc(42);
        printf("Leaked memory at final depth, address: %p\n", g_leak);
    }
}

// Test with configurable recursion depth
// Each "cycle" through the 20 functions adds 20 stack frames
// depth=300 means 300 total frames (15 cycles through the 20 functions)
//
// Why 300? See calculation in file header:
// - 300 frames × ~70 chars/frame = ~21KB output
// - MAXMESSAGELENGTH = 5119 bytes
// - 21KB >> 5KB, so truncation is guaranteed
void test_deep_callstack(int total_depth) {
    printf("Testing with call stack depth: %d\n", total_depth);
    printf("This will generate a stack trace with ~%d frames\n", total_depth);

    // Configure VLD to capture all frames
    // MaxTraceFrames is the key - set it high to capture deep stacks
    VLDSetOptions(VLD_OPT_TRACE_INTERNAL_FRAMES, (UINT)total_depth + 50, 64);

    // Start the recursive chain
    recursive_func_0(total_depth);

    // Force VLD to report the leak now
    printf("\n=== Forcing VLD leak report (this may crash if bug exists) ===\n");
    fflush(stdout);

    int leaks = (int)VLDGetLeaksCount();
    printf("VLD reports %d leak(s)\n", leaks);

    VLDReportLeaks();

    printf("=== VLD report complete (no crash!) ===\n");
}

int main(int argc, char* argv[]) {
    // Default depth: 300 stack frames
    // This generates ~21KB of output, well exceeding MAXMESSAGELENGTH (5119 bytes)
    // See file header for detailed calculation
    int depth = 300;

    if (argc > 1) {
        depth = atoi(argv[1]);
        if (depth < 10) depth = 10;
        if (depth > 1000) depth = 1000; // Cap to avoid actual stack overflow
    }

    printf("Deep Call Stack Test for VLD\n");
    printf("=============================\n");
    printf("This test creates deeply nested function calls to stress-test\n");
    printf("VLD's Print() function with excessively long stack trace messages.\n");
    printf("If PR #37's bug exists, this test will crash.\n\n");

    test_deep_callstack(depth);

    printf("\nTest completed successfully!\n");
    return 0; // Return 0 = success (VLD handled long stack trace without crash)
}
