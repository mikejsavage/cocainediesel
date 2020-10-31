#pragma once

#include "qcommon/types.h"
#include "qcommon/hash.h"

void InitAssets( TempAllocator * temp );
void ShutdownAssets();

void HotloadAssets( TempAllocator * temp );
void DoneHotloadingAssets();

Span< const char > AssetString( StringHash path );
Span< const char > AssetString( const char * path );

Span< const u8 > AssetBinary( StringHash path );
Span< const u8 > AssetBinary( const char * path );

Span< const char * > AssetPaths();
Span< const char * > ModifiedAssetPaths();
