#pragma once

#include "qcommon/types.h"
#include "qcommon/array.h"


void InitMapList();
void FreeMaps( NonRAIIDynamicArray< char * > * list );
void ShutdownMapList();

void GetFolderMapList( Allocator * a, const char * folder, NonRAIIDynamicArray< char * > * list );
void RefreshMapList( Allocator * a );
Span< const char * > GetMapList();
bool MapExists( const char * name );

const char ** CompleteMapName( const char * prefix );
