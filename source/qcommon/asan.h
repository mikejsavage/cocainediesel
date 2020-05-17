#pragma once

#include "qcommon/platform.h"

#if COMPILER_GCCORCLANG
#include <sanitizer/asan_interface.h>
#else
#define ASAN_POISON_MEMORY_REGION( mem, size )
#define ASAN_UNPOISON_MEMORY_REGION( mem, size )
#endif
