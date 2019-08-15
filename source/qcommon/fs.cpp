#include "qcommon/base.h"
#include "qcommon/fs.h"
#include "qcommon/sys_fs.h"

const char * FS_RootPath() {
	// TODO:
	return ".";
}

Span< u8 > FS_ReadEntireFile( Allocator * a, const char * path ) {
	FILE * file = fopen( path, "rb" );
	if( file == NULL )
		return Span< u8 >();

	fseek( file, 0, SEEK_END );
	size_t size = ftell( file );
	fseek( file, 0, SEEK_SET );

	Span< u8 > contents = ALLOC_SPAN( a, u8, size + 1 );
	size_t r = fread( contents.ptr, 1, size, file );
	fclose( file );
	if( r != size ) {
		FREE( a, contents.ptr );
		return Span< u8 >();
	}

	contents[ size ] = '\0';

	return contents;
}
