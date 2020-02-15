#pragma once

#include "qcommon/types.h"

bool Decompress( const char * label, Allocator * a, Span< const u8 > compressed, Span< u8 > * decompressed );
