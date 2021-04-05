#pragma once

#include <stdio.h>

#include "qcommon/types.h"

enum MoveFileReplace {
	MoveFile_DoReplace,
	MoveFile_DontReplace,
};

char * FS_RootPath( Allocator * a );

Span< char > ReadFileString( Allocator * a, const char * path );

FILE * OpenFile( Allocator * a, const char * path, const char * mode );
bool FileExists( Allocator * temp, const char * path );
bool WriteFile( TempAllocator * temp, const char * path, const void * data, size_t len );
bool MoveFile( Allocator * a, const char * old_path, const char * new_path, MoveFileReplace replace );
bool DeleteFile( Allocator * a, const char * path );

bool CreateDirectory( Allocator * a, const char * path );
bool CreatePath( Allocator * a, const char * path );

struct ListDirHandle {
	char impl[ 64 ];
};

ListDirHandle BeginListDir( Allocator * a, const char * path );
bool ListDirNext( ListDirHandle * handle, const char ** path, bool * dir );

s64 FileLastModifiedTime( TempAllocator * temp, const char * path );
