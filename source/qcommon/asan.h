#pragma once

#include "qcommon/platform.h"

#if COMPILER_CLANG
#  if __has_feature( address_sanitizer )
#    include <sanitizer/asan_interface.h>
#    define DID_ASAN
#  endif
#elif __SANITIZE_ADDRESS__
#  include <sanitizer/asan_interface.h>
#  define DID_ASAN
#endif

#ifdef DID_ASAN
#undef DID_ASAN
#else
#define ASAN_POISON_MEMORY_REGION( mem, size )
#define ASAN_UNPOISON_MEMORY_REGION( mem, size )
#endif
