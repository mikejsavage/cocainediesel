#pragma once

#include "qcommon/types.h"

struct Library {
	void * handle;
};

Library OpenLibrary( Allocator * a, const char * path );
void CloseLibrary( Library library );
void * GetLibraryFunction( Library library, const char * name );
