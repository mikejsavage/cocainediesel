#pragma once

#include "qcommon/types.h"
#include "qcommon/fs.h"

Span< char > GetExePath( Allocator * a );
Span< char > FindHomeDirectory( Allocator * a );
bool CreateDirectory( Allocator * a, const char * path );
const char * OpenFileModeToString( OpenFileMode mode );

struct ListDirHandle;
template<> inline constexpr size_t OpaqueSize< ListDirHandle > = 64;

Opaque< ListDirHandle > BeginListDir( Allocator * a, const char * path );
bool ListDirNext( Opaque< ListDirHandle > * handle, const char ** path, bool * dir );
