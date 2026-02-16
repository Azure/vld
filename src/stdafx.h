#pragma once

#include <cassert>
#include <cerrno>
#include <cstdio>
#include <windows.h>
#include <winternl.h>
#ifndef __out_xcount
#define __out_xcount(x) // Workaround for the specstrings.h bug in the Platform SDK.
#endif
#define DBGHELP_TRANSLATE_TCHAR
#include "dbghelp.hpp"    // Provides portable executable (PE) image access functions.
