#pragma once

#include "qcommon/types.h"

const char * FS_RootPath();
Span< char > ReadFileString( Allocator * a, const char * path );

struct ListDirHandle {
	char impl[ 64 ];
};

ListDirHandle BeginListDir( const char * path );
bool ListDirNext( ListDirHandle * handle, const char ** path, bool * dir );

s64 FileLastModifiedTime( const char * path );
