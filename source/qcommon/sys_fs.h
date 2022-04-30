#pragma once

#include "qcommon/types.h"
#include "qcommon/fs.h"

char * GetExePath( Allocator * a );
char * FindHomeDirectory( Allocator * a );
bool CreateDirectory( Allocator * a, const char * path );
const char * OpenFileModeToString( OpenFileMode mode );
