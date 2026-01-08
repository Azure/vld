// Test to see if _ReturnAddress() matches frames in RtlCaptureStackBackTrace
// This helps debug why ARM64 frame skipping needs a fallback

#include <windows.h>
#include <stdio.h>
#include <intrin.h>

void PrintFrameMatch()
{
    // Capture return address
    UINT_PTR returnAddr = (UINT_PTR)_ReturnAddress();
    
    // Capture stack frames
    UINT_PTR frames[20];
    ZeroMemory(frames, sizeof(frames));
    ULONG hash;
    UINT32 count = RtlCaptureStackBackTrace(0, 20, reinterpret_cast<PVOID*>(frames), &hash);
    
    printf("_ReturnAddress() = 0x%p\n", (void*)returnAddr);
    printf("RtlCaptureStackBackTrace captured %u frames:\n", count);
    
    bool found = false;
    for (UINT32 i = 0; i < count; i++) {
        bool isMatch = (frames[i] == returnAddr);
        printf("  Frame[%2u] = 0x%p %s\n", i, (void*)frames[i], isMatch ? "<-- MATCH!" : "");
        if (isMatch) {
            found = true;
        }
    }
    
    if (!found) {
        printf("\n*** WARNING: _ReturnAddress() NOT found in captured frames! ***\n");
        printf("This explains why ARM64 needs the fallback to skip 3 frames.\n");
        
        // Check if any frame is "close" to returnAddr
        printf("\nChecking for near matches (within 32 bytes):\n");
        for (UINT32 i = 0; i < count; i++) {
            INT_PTR diff = (INT_PTR)frames[i] - (INT_PTR)returnAddr;
            if (diff >= -32 && diff <= 32 && diff != 0) {
                printf("  Frame[%2u] = 0x%p (offset %+lld bytes)\n", i, (void*)frames[i], (long long)diff);
            }
        }
    }
}

__declspec(noinline) void Level3()
{
    PrintFrameMatch();
}

__declspec(noinline) void Level2()
{
    Level3();
}

__declspec(noinline) void Level1()
{
    Level2();
}

int main()
{
    printf("Frame Matching Test - Checking if _ReturnAddress() matches RtlCaptureStackBackTrace\n");
    printf("===================================================================================\n\n");
    
#if defined(_M_ARM64)
    printf("Architecture: ARM64\n\n");
#elif defined(_M_X64)
    printf("Architecture: x64\n\n");
#elif defined(_M_IX86)
    printf("Architecture: x86\n\n");
#endif
    
    Level1();
    
    printf("\nIf _ReturnAddress() matches a frame, VLD can skip internal frames accurately.\n");
    printf("If NOT matched, VLD falls back to blindly skipping N frames (fragile).\n");
    
    return 0;
}
