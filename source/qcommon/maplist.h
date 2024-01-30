#pragma once

#include "qcommon/types.h"

void InitMapList();
void ShutdownMapList();

void RefreshMapList( Allocator * a );
Span< Span< const char > > GetMapList();
bool MapExists( Span< const char > name );

Span< Span< const char > > CompleteMapName( TempAllocator * a, Span< const char > prefix );
