// Simple test that intentionally leaks to trigger VLD callstack capture
// This will show if the ARM64 fallback code ever executes

#include <stdio.h>
#include <stdlib.h>

int main()
{
    printf("Creating intentional leaks to test callstack capture...\n");
    
    // Create 5 different leaks to trigger multiple callstack captures
    for (int i = 0; i < 5; i++) {
        void* leak = malloc(100 + i * 10);
        printf("Leaked %d bytes at %p\n", 100 + i * 10, leak);
    }
    
    printf("\nDone. VLD should now report these leaks with callstacks.\n");
    printf("Check output for 'VLD DEBUG: context.fp=0x... NOT found' messages.\n");
    printf("If no debug messages appear, the fallback is never used.\n");
    
    return 0;
}
