#include "qcommon/base.h"
#include "qcommon/fs.h"
#include "qcommon/utf8.h"
#include "qcommon/sys_fs.h"
#include "gameshared/q_shared.h"

static char * root_dir_path;
static char * home_dir_path;
static char * old_home_dir_path;

static char * FindRootDir( Allocator * a ) {
	char * root = GetExePath( a );
	root[ BasePath( root ).n ] = '\0';
	return root;
}

void InitFS() {
	root_dir_path = FindRootDir( sys_allocator );

	if( !is_public_build ) {
		home_dir_path = CopyString( sys_allocator, root_dir_path );
		old_home_dir_path = CopyString( sys_allocator, home_dir_path );
	}
	else {
		home_dir_path = FindHomeDirectory( sys_allocator );

		const char * fmt = IFDEF( PLATFORM_WINDOWS ) ? "{} 0.0" : "{}-0.0";
		old_home_dir_path = ( *sys_allocator )( fmt, home_dir_path );
	}
}

void ShutdownFS() {
	FREE( sys_allocator, root_dir_path );
	FREE( sys_allocator, home_dir_path );
	FREE( sys_allocator, old_home_dir_path );
}

const char * RootDirPath() {
	return root_dir_path;
}

const char * HomeDirPath() {
	return home_dir_path;
}

const char * OldHomeDirPath() {
	return old_home_dir_path;
}

size_t FileSize( FILE * file ) {
	if( fseek( file, 0, SEEK_END ) != 0 ) {
		FatalErrno( "fseek" );
	}
	size_t size = ftell( file );
	Seek( file, 0 );
	return size;
}

char * ReadFileString( Allocator * a, const char * path, size_t * len ) {
	FILE * file = OpenFile( a, path, "rb" );
	if( file == NULL )
		return NULL;

	size_t size = FileSize( file );
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

	size_t size = FileSize( file );
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

bool CreatePathForFile( Allocator * a, const char * path ) {
	char * mutable_path = CopyString( a, path );
	defer { FREE( a, mutable_path ); };

	char * cursor = mutable_path;

	// don't try to create drives on windows or "" on linux
	if( IFDEF( PLATFORM_WINDOWS ) ) {
		if( strlen( cursor ) >= 2 && cursor[ 1 ] == ':' ) {
			cursor += 3;
		}
	}
	else {
		if( strlen( cursor ) >= 1 && cursor[ 0 ] == '/' ) {
			cursor++;
		}
	}

	while( ( cursor = strchr( cursor, '/' ) ) != NULL ) {
		*cursor = '\0';
		if( !CreateDirectory( a, mutable_path ) )
			return false;
		*cursor = '/';
		cursor++;
	}

	return true;
}

bool WriteFile( TempAllocator * temp, const char * path, const void * data, size_t len ) {
	if( !CreatePathForFile( temp, path ) )
		return false;

	FILE * file = OpenFile( temp, path, "wb" );
	if( file == NULL )
		return false;

	size_t w = fwrite( data, 1, len, file );
	fclose( file );

	return w == len;
}

bool ReadPartialFile( FILE * file, void * data, size_t len, size_t * bytes_read ) {
	*bytes_read = fread( data, 1, len, file );
	return ferror( file ) == 0;
}

bool WritePartialFile( FILE * file, const void * data, size_t len ) {
	return fwrite( data, 1, len, file ) == len && ferror( file ) == 0;
}

void Seek( FILE * file, size_t cursor ) {
	if( fseek( file, checked_cast< long >( cursor ), SEEK_SET ) != 0 ) {
		FatalErrno( "fseek" );
	}
}
