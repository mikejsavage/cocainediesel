#pragma once

#include <stdio.h>

#include "qcommon/types.h"

enum MoveFileReplace {
	MoveFile_DoReplace,
	MoveFile_DontReplace,
};

void InitFS();
void ShutdownFS();

const char * RootDirPath();
const char * HomeDirPath();
const char * OldHomeDirPath();

char * ReadFileString( Allocator * a, const char * path, size_t * len = NULL );
Span< u8 > ReadFileBinary( Allocator * a, const char * path );

FILE * OpenFile( Allocator * a, const char * path, const char * mode );
bool ReadPartialFile( FILE * file, void * data, size_t len, size_t * bytes_read );
bool WritePartialFile( FILE * file, const void * data, size_t len );
void Seek( FILE * file, size_t cursor );
size_t FileSize( FILE * file );

bool FileExists( Allocator * temp, const char * path );
bool WriteFile( TempAllocator * temp, const char * path, const void * data, size_t len );
bool MoveFile( Allocator * a, const char * old_path, const char * new_path, MoveFileReplace replace );
bool RemoveFile( Allocator * a, const char * path );

struct ListDirHandle {
	char impl[ 64 ];
};

ListDirHandle BeginListDir( Allocator * a, const char * path );
bool ListDirNext( ListDirHandle * handle, const char ** path, bool * dir );

struct FSChangeMonitor;

FSChangeMonitor * NewFSChangeMonitor( Allocator * a, const char * path );
void DeleteFSChangeMonitor( Allocator * a, FSChangeMonitor * monitor );
Span< const char * > PollFSChangeMonitor( TempAllocator * temp, FSChangeMonitor * monitor, const char ** results, size_t n );
