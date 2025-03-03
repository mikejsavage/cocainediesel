#include "qcommon/base.h"
#include "qcommon/fs.h"
#include "qcommon/utf8.h"
#include "qcommon/platform/fs.h"
#include "gameshared/q_shared.h"

static Span< char > root_dir_path;
static Span< char > home_dir_path;

static Span< char > FindRootDir( Allocator * a ) {
	return BasePath( GetExePath( a ) ).constcast< char >();
}

void InitFS() {
	TracyZoneScoped;

	root_dir_path = FindRootDir( sys_allocator );

	if( !is_public_build ) {
		home_dir_path = CloneSpan( sys_allocator, root_dir_path );
	}
	else {
		home_dir_path = FindHomeDirectory( sys_allocator );
	}
}

void ShutdownFS() {
	Free( sys_allocator, root_dir_path.ptr );
	Free( sys_allocator, home_dir_path.ptr );
}

Span< const char > RootDirPath() {
	return root_dir_path;
}

Span< const char > HomeDirPath() {
	return home_dir_path;
}

size_t FileSize( FILE * file ) {
	if( fseek( file, 0, SEEK_END ) != 0 ) {
		FatalErrno( "fseek" );
	}
	size_t size = ftell( file );
	Seek( file, 0 );
	return size;
}

Span< u8 > ReadFileBinary( Allocator * a, const char * path, SourceLocation src_loc ) {
	FILE * file = OpenFile( a, path, OpenFile_Read );
	if( file == NULL )
		return Span< u8 >();
	defer { fclose( file ); };

	size_t size = FileSize( file );
	u8 * contents = ( u8 * ) a->allocate( size, 16, src_loc );
	size_t r = fread( contents, 1, size, file );
	if( r != size ) {
		Free( a, contents );
		return Span< u8 >();
	}

	return Span< u8 >( contents, size );
}

bool FileExists( Allocator * a, const char * path ) {
	FILE * file = OpenFile( a, path, OpenFile_Read );
	if( file == NULL )
		return false;
	fclose( file );
	return true;
}

bool CreatePathForFile( Allocator * a, const char * path ) {
	char * mutable_path = CopyString( a, path );
	defer { Free( a, mutable_path ); };

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

bool WriteFile( Allocator * a, const char * path, const void * data, size_t len ) {
	if( !CreatePathForFile( a, path ) )
		return false;

	FILE * file = OpenFile( a, path, OpenFile_WriteOverwrite );
	if( file == NULL )
		return false;

	size_t w = fwrite( data, 1, len, file );
	fclose( file );

	return w == len;
}

const char * OpenFileModeToString( OpenFileMode mode ) {
	switch( mode ) {
		case OpenFile_Read: return "rb";
		case OpenFile_WriteNew: return "wbx";
		case OpenFile_WriteOverwrite: return "wb";
		case OpenFile_ReadWriteNew: return "w+bx";
		case OpenFile_ReadWriteOverwrite: return "w+b";
		case OpenFile_AppendExisting: return "ab";
	}

	Assert( false );
	return NULL;
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

bool CloseFile( FILE * file ) {
	bool ok = ferror( file ) == 0;
	fclose( file );
	return ok;
}
