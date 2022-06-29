#pragma once

#include "qcommon/platform.h"

#if __SANITIZE_ADDRESS__
#include <sanitizer/asan_interface.h>
#else
#define ASAN_POISON_MEMORY_REGION( mem, size )
#define ASAN_UNPOISON_MEMORY_REGION( mem, size )
#endif
