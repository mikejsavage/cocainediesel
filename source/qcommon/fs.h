#pragma once

#include <stddef.h>

void FS_Init();
void FS_Shutdown();

const char * FS_RootPath();
const char * FS_WritePath();

void * FS_ReadEntireFile( const char * path, size_t * len );

bool FS_CreateDirectory( const char * path );
bool FS_CreateDirectories( const char * path );

struct ListDirHandle {
	char data[ 64 ];
};

ListDirHandle FS_BeginListDir( const char * path );
bool FS_ListDirNext( ListDirHandle scan, const char ** path, bool * dir );
void FS_EndListDir( ListDirHandle scan );
