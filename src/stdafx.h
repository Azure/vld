#pragma once
#define _DECL_DLLMAIN    // Enables _CRT_INIT, needs to be specified before any header referring to process.h (like mutex)

#include <cassert>
#include <cerrno>
#include <cstdio>
#include <windows.h>
#include <winternl.h>
#ifndef __out_xcount
#define __out_xcount(x) // Workaround for the specstrings.h bug in the Platform SDK.
#endif
#define DBGHELP_TRANSLATE_TCHAR
#include "vld_dbghelp.h"    // Provides portable executable (PE) image access functions.
