#include "qcommon/base.h"
#include "qcommon/fs.h"

#include "whereami/whereami.h"

char * FS_RootPath( Allocator * a ) {
	int len = wai_getExecutablePath( NULL, 0, NULL );
	if( len == -1 )
		return ".";

	char * buf = ALLOC_MANY( a, char, len + 1 );
	int dirlen;
	if( wai_getExecutablePath( buf, len, &dirlen ) == -1 ) {
		FREE( a, buf );
		return ".";
	}
	buf[ dirlen ] = '\0';

	return buf;
}

Span< char > ReadFileString( Allocator * a, const char * path ) {
	FILE * file = OpenFile( a, path, "rb" );
	if( file == NULL )
		return Span< char >();

	fseek( file, 0, SEEK_END );
	size_t size = ftell( file );
	fseek( file, 0, SEEK_SET );

	char * contents = ( char * ) ALLOC_SIZE( a, size + 1, 16 );
	size_t r = fread( contents, 1, size, file );
	fclose( file );
	if( r != size ) {
		FREE( a, contents );
		return Span< char >();
	}

	contents[ size ] = '\0';

	return Span< char >( contents, size + 1 );
}

bool WriteFile( TempAllocator * temp, const char * path, const void * data, size_t len ) {
	FILE * file = OpenFile( temp, path, "wb" );
	if( file == NULL )
		return false;

	size_t w = fwrite( data, 1, len, file );
	fclose( file );

	return w == len;
}
