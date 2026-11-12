// chaos_longname_dll_source_file_name_source_file_name_source_file_name_source_file_name_source_file_name_end.cpp - helper for chaos_long_frame: a DLL with a very long
// file name exporting a function with a very long name, so that the leak
// report line for its frame exceeds VLD's report line length.

#include <windows.h>
#include <stdlib.h>
#include "vld.h"

#pragma optimize("", off)
extern "C" __declspec(dllexport) __declspec(noinline) void* LongName_abcdefghijabcdefghijabcdefghijabcdefghijabcdefghijabcdefghijabcdefghijabcdefghijabcdefghijabcdefghijabcdefghijabcdefghijabcdefghijabcdefghijabcdefghijabcdefghijabcdefghijabcdefghijabcdefghijabcdefghijabcdefghijabcdefghijabcdefghijabcdefghijxyz(size_t size)
{
    return malloc(size);
}
#pragma optimize("", on)