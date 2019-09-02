#include "qcommon/base.h"
#include "qcommon/fs.h"
#include "qcommon/sys_fs.h"

const char * FS_RootPath() {
	// TODO:
	return ".";
}

Span< char > FS_ReadFileString( Allocator * a, const char * path ) {
	FILE * file = fopen( path, "rb" );
	if( file == NULL )
		return Span< char >();

	fseek( file, 0, SEEK_END );
	size_t size = ftell( file );
	fseek( file, 0, SEEK_SET );

	Span< char > contents = ALLOC_SPAN( a, char, size + 1 );
	size_t r = fread( contents.ptr, 1, size, file );
	fclose( file );
	if( r != size ) {
		FREE( a, contents.ptr );
		return Span< char >();
	}

	contents[ size ] = '\0';

	return contents;
}
