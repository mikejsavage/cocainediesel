#pragma once

#include <stdio.h>

#include "qcommon/types.h"

const char * FS_RootPath( TempAllocator * a );

FILE * OpenFile( TempAllocator * temp, const char * path, const char * mode );

Span< char > ReadFileString( Allocator * a, TempAllocator * temp, const char * path );
bool WriteFile( const char * path, const void * data, size_t len );

struct ListDirHandle {
	char impl[ 64 ];
};

ListDirHandle BeginListDir( Allocator * a, const char * path );
bool ListDirNext( ListDirHandle * handle, const char ** path, bool * dir );

s64 FileLastModifiedTime( TempAllocator * temp, const char * path );
