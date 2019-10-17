#pragma once

#include "qcommon/types.h"

const char * FS_RootPath( TempAllocator * a );

Span< char > ReadFileString( Allocator * a, const char * path );
bool WriteFile( const char * path, const void * data, size_t len );

struct ListDirHandle {
	char impl[ 64 ];
};

ListDirHandle BeginListDir( const char * path );
bool ListDirNext( ListDirHandle * handle, const char ** path, bool * dir );

s64 FileLastModifiedTime( const char * path );
