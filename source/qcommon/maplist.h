#pragma once

#include "qcommon/types.h"

void InitMapList();
void ShutdownMapList();

void RefreshMapList( Allocator * a );
Span< const char * > GetMapList();
bool MapExists( const char * name );

Span< const char * > CompleteMapName( TempAllocator * a, const char * prefix );
