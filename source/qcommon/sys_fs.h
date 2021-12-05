#pragma once

#include "qcommon/types.h"

char * GetExePath( Allocator * a );
char * FindHomeDirectory( Allocator * a );
bool CreateDirectory( Allocator * a, const char * path );
