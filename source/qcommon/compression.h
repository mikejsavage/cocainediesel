#pragma once

#include "qcommon/types.h"

bool Decompress( const char * name, Allocator * a, Span< const u8 > compressed, Span< u8 > * decompressed );
