#pragma once

#include "qcommon/types.h"
#include "qcommon/hash.h"

void InitAssets( TempAllocator * temp );
void ShutdownAssets();

Span< const u8 > AssetData( StringHash name );
Span< const char * > AssetNames();
