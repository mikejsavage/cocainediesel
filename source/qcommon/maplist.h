#pragma once

#include "qcommon/types.h"

void InitMapList();
void ShutdownMapList();

void RefreshMapList( Allocator * a );
Span< const char * > GetMapList();
bool MapExists( const char * name );

const char ** CompleteMapName( const char * prefix );
