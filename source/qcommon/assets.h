#pragma once

#include "qcommon/types.h"
#include "qcommon/hash.h"

void Assets_Init();
void Assets_Shutdown();

void Assets_LoadFromFS();

const void * Assets_Data( StringHash name, size_t * len = NULL );
Span< const char * > Assets_Names();
