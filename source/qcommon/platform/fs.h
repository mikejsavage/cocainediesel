#pragma once

#include "qcommon/types.h"
#include "qcommon/fs.h"

Span< char > GetExePath( Allocator * a );
Span< char > FindHomeDirectory( Allocator * a );
bool CreateDirectory( Allocator * a, const char * path );
const char * OpenFileModeToString( OpenFileMode mode );
