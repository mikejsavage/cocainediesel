#pragma once

#include "qcommon/types.h"

const char * FS_RootPath();
Span< char > FS_ReadFileString( Allocator * a, const char * path );

struct ListDirHandle {
	char impl[ 64 ];
};

ListDirHandle FS_BeginListDir( const char * path );
bool FS_ListDirNext( ListDirHandle * handle, const char ** path, bool * dir );
