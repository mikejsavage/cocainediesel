#include "qcommon/base.h"
#include "qcommon/fs.h"
#include "qcommon/qcommon.h"
#include "qcommon/utf8.h"
#include "qcommon/sys_fs.h"

#include "whereami/whereami.h"

static char * root_dir_path;
static const char * home_dir_path;

static char * FindRootDir( Allocator * a ) {
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

void InitFS() {
	root_dir_path = FindRootDir( sys_allocator );
#if PUBLIC_BUILD
	home_dir_path = Sys_FS_GetHomeDirectory();
#else
	home_dir_path = root_dir_path;
#endif
	// TODO: replace \ with /? utf-8 aware pls.
}

void ShutdownFS() {
	FREE( sys_allocator, root_dir_path );
	// FREE( sys_allocator, home_dir_path );
}

const char * RootDirPath() {
	return root_dir_path;
}

const char * HomeDirPath() {
	return home_dir_path;
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

Span< u8 > ReadFileBinary( Allocator * a, const char * path ) {
	FILE * file = OpenFile( a, path, "rb" );
	if( file == NULL )
		return Span< u8 >();

	fseek( file, 0, SEEK_END );
	size_t size = ftell( file );
	fseek( file, 0, SEEK_SET );

	u8 * contents = ( u8 * ) ALLOC_SIZE( a, size, 16 );
	size_t r = fread( contents, 1, size, file );
	fclose( file );
	if( r != size ) {
		FREE( a, contents );
		return Span< u8 >();
	}

	return Span< u8 >( contents, size );
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
		if( !CreateDirectory( a, mutable_path ) )
			return false;
		*cursor = '/';
		cursor++;
	}

	return CreateDirectory( a, mutable_path );
}
