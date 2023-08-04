#pragma once

#if defined( _WIN32 )
#  define PLATFORM_WINDOWS 1
#elif defined( __APPLE__ )
#  define PLATFORM_MACOS 1
#  define PLATFORM_UNIX 1
#elif defined( __linux__ )
#  define PLATFORM_LINUX 1
#  define PLATFORM_UNIX 1
#else
#  error unsupported platform
#endif

#if defined( _MSC_VER )
#  define COMPILER_MSVC 1
#elif defined( __clang__ )
#  define COMPILER_CLANG 1
#  define COMPILER_GCC_OR_CLANG 1
#elif defined( __GNUC__ )
#  define COMPILER_GCC 1
#  define COMPILER_GCC_OR_CLANG 1
#else
#  error unsupported compiler
#endif
