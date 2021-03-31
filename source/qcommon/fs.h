#pragma once

#include <stdio.h>

#include "qcommon/types.h"

char * FS_RootPath( Allocator * a );

FILE * OpenFile( Allocator * a, const char * path, const char * mode );

Span< char > ReadFileString( Allocator * a, const char * path );
bool WriteFile( TempAllocator * temp, const char * path, const void * data, size_t len );

struct ListDirHandle {
	char impl[ 64 ];
};

ListDirHandle BeginListDir( Allocator * a, const char * path );
bool ListDirNext( ListDirHandle * handle, const char ** path, bool * dir );

s64 FileLastModifiedTime( TempAllocator * temp, const char * path );
