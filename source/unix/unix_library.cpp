#include <dlfcn.h>

#include "qcommon/base.h"
#include "qcommon/library.h"

Library OpenLibrary( Allocator * a, const char * path ) {
	char * path_with_so = ( *a )( "{}.so", path );
	defer { FREE( a, path_with_so ); };
	printf( "%s\n", path_with_so );
	return { dlopen( path_with_so, RTLD_NOW ) };
}

void CloseLibrary( Library library ) {
	dlclose( library.handle );
}

void * GetLibraryFunction( Library library, const char * name ) {
	return dlsym( library.handle, name );
}
