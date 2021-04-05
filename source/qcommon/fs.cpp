#include "qcommon/base.h"
#include "qcommon/fs.h"
#include "qcommon/utf8.h"

#include "whereami/whereami.h"

char * FS_RootPath( Allocator * a ) {
	int len = wai_getExecutablePath( NULL, 0, NULL );
	if( len == -1 ) {
		Com_Error( ERR_FATAL, "wai_getExecutablePath( NULL )" );
	}

	char * buf = ALLOC_MANY( a, char, len + 1 );
	int dirlen;
	if( wai_getExecutablePath( buf, len, &dirlen ) == -1 ) {
		Com_Error( ERR_FATAL, "wai_getExecutablePath( buf )" );
	}
	buf[ dirlen ] = '\0';

	return buf;
}

char * ReadFileString( Allocator * a, const char * path, size_t * len ) {
	FILE * file = OpenFile( a, path, "rb" );
	if( file == NULL )
		return NULL;

	fseek( file, 0, SEEK_END );
	size_t size = ftell( file );
	fseek( file, 0, SEEK_SET );

	char * contents = ( char * ) ALLOC_SIZE( a, size + 1, 16 );
	size_t r = fread( contents, 1, size, file );
	fclose( file );
	if( r != size ) {
		FREE( a, contents );
		return NULL;
	}

	contents[ size ] = '\0';
	if( len != NULL ) {
		*len = size;
	}
	return contents;
}

bool FileExists( Allocator * temp, const char * path ) {
	FILE * file = OpenFile( temp, path, "rb" );
	if( file == NULL )
		return false;
	fclose( file );
	return true;
}

bool WriteFile( TempAllocator * temp, const char * path, const void * data, size_t len ) {
	FILE * file = OpenFile( temp, path, "wb" );
	if( file == NULL )
		return false;

	size_t w = fwrite( data, 1, len, file );
	fclose( file );

	return w == len;
}

bool CreatePath( Allocator * a, const char * path ) {
	char * mutable_path = CopyString( a, path );
	defer { FREE( a, mutable_path ); };

	char * cursor = mutable_path;
	while( ( cursor = StrChrUTF8( cursor, '/' ) ) != NULL ) {
		*cursor = '\0';
		if( !CreateDirectory( a, path ) )
			return false;
		*cursor = '/';
	}

	return true;
}
