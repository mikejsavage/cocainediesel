#pragma once

#include <wchar.h>

#include "qcommon/types.h"

wchar_t * UTF8ToWide( Allocator * a, const char * utf8 );
char * WideToUTF8( Allocator * a, Span< const wchar_t > wide );
char * WideToUTF8( Allocator * a, const wchar_t * wide );
