#if 0
cd/client
cd/assets/assets.cdpak
cd/assets/maps/t.bsp

cd/autoexec.txt
cd/config.txt
cd/avi
cd/demos
cd/screenshots

~/.local/share/cd/avi
~/.local/share/cd/demos
~/.local/share/cd/screenshots
#endif

#include "fs.h"
#include "qcommon/qcommon.h"
#include "qcommon/sys_fs.h"

static cvar_t * fs_writepath;
static const char * rootpath;

#if 0
void FS_Init() {
#if PUBLIC_BUILD && !DEDICATED
	const char * home = Sys_FS_GetHomeDirectory();
	fs_writepath = Cvar_Get( "fs_writepath", home == NULL ? "" : home, CVAR_NOSET );
#else
	fs_writepath = Cvar_Get( "fs_writepath", "", CVAR_NOSET );
#endif

	// TODO: whereami
	rootpath = ".";
}

void FS_Shutdown() {
}
#endif

const char * FS_RootPath() {
	return rootpath;
}

const char * FS_WritePath() {
	return fs_writepath->string;
}

void * FS_ReadEntireFile( const char * path, size_t * len ) {
	FILE * file = fopen( path, "rb" );
	if( file == NULL )
		return NULL;

	fseek( file, 0, SEEK_END );
	size_t size = ftell( file );
	fseek( file, 0, SEEK_SET );

	char * contents = ( char * ) malloc( size + 1 );
	size_t r = fread( contents, 1, size, file );
	if( r != size ) {
		fclose( file );
		free( contents );
		return NULL;
	}

	contents[ size ] = '\0';
	fclose( file );

	return contents;
}

bool FS_CreateDirectory( const char * path ) {
	return Sys_FS_CreateDirectory( path );
}

bool FS_CreateDirectories( const char * path ) {
	return false;
}
