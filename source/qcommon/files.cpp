/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "qcommon.h"
#include "qcommon/hash.h"

#include "sys_fs.h"

#include "zlib/zlib.h"

/*
=============================================================================

QUAKE FILESYSTEM

=============================================================================
*/

#define FS_MAX_PATH                 1024

#define FS_GZ_BUFSIZE               0x00020000

typedef struct filehandle_s {
	FILE *fstream;
	unsigned uncompressedSize;      // uncompressed size
	unsigned offset;                // current read/write pos
	gzFile gzstream;
	int gzlevel;

	void *mapping;
	size_t mapping_size;
	size_t mapping_offset;

	struct filehandle_s *prev, *next;
} filehandle_t;

typedef struct searchpath_s {
	char *path;
	struct searchpath_s *base;      // parent basepath
	struct searchpath_s *next;
} searchpath_t;

typedef struct {
	char *name;
	searchpath_t *searchPath;
} searchfile_t;

static searchfile_t *fs_searchfiles;
static int fs_numsearchfiles;
static int fs_cursearchfiles;

static cvar_t *fs_basepath;
static cvar_t *fs_cdpath;
static cvar_t *fs_usehomedir;
static cvar_t *fs_usedownloadsdir;
static cvar_t *fs_basegame;

static searchpath_t *fs_basepaths = NULL;       // directories without gamedirs
static searchpath_t *fs_searchpaths = NULL;     // game search directories
static qmutex_t *fs_searchpaths_mutex;

static searchpath_t *fs_base_searchpaths;       // same as above, but without extra gamedirs
static searchpath_t *fs_root_searchpath;        // base path directory
static searchpath_t *fs_write_searchpath;       // write directory
static searchpath_t *fs_downloads_searchpath;   // write directory for downloads from game servers

static mempool_t *fs_mempool;

#define FS_Malloc( size ) Mem_Alloc( fs_mempool, size )
#define FS_Realloc( data, size ) Mem_Realloc( data, size )
#define FS_Free( data ) Mem_Free( data )

#define FS_MAX_BLOCK_SIZE   0x10000
#define FS_MAX_HANDLES      1024

static filehandle_t fs_filehandles[FS_MAX_HANDLES];
static filehandle_t fs_filehandles_headnode, *fs_free_filehandles;
static qmutex_t *fs_fh_mutex;

static bool fs_initialized = false;

/*
* FS_CopyString
*/
static char *FS_CopyString( const char *in ) {
	int size;
	char *out;

	size = sizeof( char ) * ( strlen( in ) + 1 );
	out = ( char* )FS_Malloc( size );
	Q_strncpyz( out, in, size );

	return out;
}

/*
* FS_ListFiles
*/
static char **FS_ListFiles( char *findname, int *numfiles, unsigned musthave, unsigned canthave ) {
	const char *s;
	int nfiles = 0;
	static char **list = NULL;

	s = Sys_FS_FindFirst( findname, musthave, canthave );
	while( s ) {
		if( COM_ValidateFilename( s ) ) {
			nfiles++;
		}
		s = Sys_FS_FindNext( musthave, canthave );
	}
	Sys_FS_FindClose();

	if( !nfiles ) {
		return NULL;
	}

	*numfiles = nfiles;
	nfiles++; // add space for a guard
	list = ( char** )Mem_ZoneMalloc( sizeof( char * ) * nfiles );

	s = Sys_FS_FindFirst( findname, musthave, canthave );
	nfiles = 0;
	while( s ) {
		if( !COM_ValidateFilename( s ) ) {
			continue;
		}

		list[nfiles] = ZoneCopyString( s );

#ifdef _WIN32
		Q_strlwr( list[nfiles] );
#endif
		nfiles++;
		s = Sys_FS_FindNext( musthave, canthave );
	}
	Sys_FS_FindClose();

	list[nfiles] = NULL;
	return list;
}

/*
* FS_SearchDirectoryForFile
*/
static bool FS_SearchDirectoryForFile( searchpath_t *search, const char *filename, char *path, size_t path_size ) {
	FILE *f;
	char tempname[FS_MAX_PATH];
	bool found = false;

	assert( search );
	assert( filename );

	Q_snprintfz( tempname, sizeof( tempname ), "%s/%s", search->path, filename );

	f = fopen( tempname, "rb" );
	if( f ) {
		fclose( f );
		found = true;
	}

	if( found && path ) {
		Q_strncpyz( path, tempname, path_size );
	}

	return found;
}

/*
* FS_FileLength
*/
static int FS_FileLength( FILE *f, bool close ) {
	int pos, end;

	assert( f != NULL );
	if( !f ) {
		return -1;
	}

	pos = ftell( f );
	fseek( f, 0, SEEK_END );
	end = ftell( f );
	fseek( f, pos, SEEK_SET );

	if( close ) {
		fclose( f );
	}

	return end;
}

/*
* FS_SearchPathForFile
*
* Gives the searchpath element where this file exists, or NULL if it doesn't
*/
static searchpath_t *FS_SearchPathForFile( const char *filename, char *path, size_t path_size ) {
	searchpath_t *search;

	if( !COM_ValidateRelativeFilename( filename ) ) {
		return NULL;
	}

	if( path && path_size ) {
		path[0] = '\0';
	}

	// search through the path, one element at a time
	QMutex_Lock( fs_searchpaths_mutex );
	defer { QMutex_Unlock( fs_searchpaths_mutex ); };
	search = fs_searchpaths;
	while( search ) {
		if( FS_SearchDirectoryForFile( search, filename, path, path_size ) ) {
			return search;
		}

		search = search->next;
	}

	return NULL;
}

/*
* FS_SearchPathForBaseFile
*
* Gives the searchpath element where this file exists, or NULL if it doesn't
*/
static searchpath_t *FS_SearchPathForBaseFile( const char *filename, char *path, size_t path_size ) {
	searchpath_t *search;

	if( !COM_ValidateRelativeFilename( filename ) ) {
		return NULL;
	}

	if( path && path_size ) {
		path[0] = '\0';
	}

	// search through the path, one element at a time
	search = fs_basepaths;
	while( search ) {
		if( FS_SearchDirectoryForFile( search, filename, path, path_size ) ) {
			return search;
		}

		search = search->next;
	}

	return NULL;
}

/*
* FS_OpenFileHandle
*/
static int FS_OpenFileHandle( void ) {
	filehandle_t *fh;

	QMutex_Lock( fs_fh_mutex );

	if( !fs_free_filehandles ) {
		QMutex_Unlock( fs_fh_mutex );
		Sys_Error( "FS_OpenFileHandle: no free file handles" );
	}

	fh = fs_free_filehandles;
	fs_free_filehandles = fh->next;

	// put the handle at the start of the list
	memset( fh, 0, sizeof( *fh ) );
	fh->prev = &fs_filehandles_headnode;
	fh->next = fs_filehandles_headnode.next;
	fh->next->prev = fh;
	fh->prev->next = fh;

	QMutex_Unlock( fs_fh_mutex );

	return ( fh - fs_filehandles ) + 1;
}

/*
* FS_FileHandleForNum
*/
static inline filehandle_t *FS_FileHandleForNum( int file ) {
	if( file < 1 || file > FS_MAX_HANDLES ) {
		Sys_Error( "FS_FileHandleForNum: bad handle: %i", file );
	}
	return &fs_filehandles[file - 1];
}

/*
* FS_FileNumForHandle
*/
static inline int FS_FileNumForHandle( filehandle_t *fh ) {
	int file;

	file = ( fh - fs_filehandles ) + 1;
	if( file < 1 || file > FS_MAX_HANDLES ) {
		Sys_Error( "FS_FileHandleForNum: bad handle: %i", file );
	}
	return file;
}

/*
* FS_CloseFileHandle
*/
static void FS_CloseFileHandle( filehandle_t *fh ) {
	QMutex_Lock( fs_fh_mutex );

	// remove from linked open list
	fh->prev->next = fh->next;
	fh->next->prev = fh->prev;

	// insert into linked free list
	fh->next = fs_free_filehandles;
	fs_free_filehandles = fh;

	QMutex_Unlock( fs_fh_mutex );
}

/*
* FS_FirstExtension
* Searches the paths for file matching with one of the extensions
* If found returns the extension otherwise NULL
* extensions parameter is string with extensions separated by spaces
*/
const char *FS_FirstExtension( const char *filename, const char * const * extensions, int num_extensions ) {
	char **filenames;           // slots for testable filenames
	size_t filename_size;       // size of one slot
	int i;
	size_t max_extension_length;
	searchpath_t *search;

	assert( filename && extensions );

	if( !num_extensions ) {
		return NULL;
	}

#ifndef NDEBUG
	for( i = 0; i < num_extensions; i++ )
		assert( extensions[i] && extensions[i][0] );
#endif

	if( !COM_ValidateRelativeFilename( filename ) ) {
		return NULL;
	}

	max_extension_length = 0;
	for( i = 0; i < num_extensions; i++ ) {
		if( strlen( extensions[i] ) > max_extension_length ) {
			max_extension_length = strlen( extensions[i] );
		}
	}

	// set the filenames to be tested
	filenames = ( char** )alloca( sizeof( char * ) * num_extensions );
	filename_size = sizeof( char ) * ( strlen( filename ) + max_extension_length + 1 );

	for( i = 0; i < num_extensions; i++ ) {
		if( i ) {
			filenames[i] = ( char * )( ( uint8_t * )filenames[0] + filename_size * i );
		} else {
			filenames[i] = ( char* )alloca( filename_size * num_extensions );
		}
		Q_strncpyz( filenames[i], filename, filename_size );
		COM_ReplaceExtension( filenames[i], extensions[i], filename_size );
	}

	// search through the path, one element at a time
	QMutex_Lock( fs_searchpaths_mutex );
	defer { QMutex_Unlock( fs_searchpaths_mutex ); };
	search = fs_searchpaths;
	while( search ) {
		for( i = 0; i < num_extensions; i++ ) {
			if( FS_SearchDirectoryForFile( search, filenames[i], NULL, 0 ) ) {
				return extensions[i];
			}
		}

		search = search->next;
	}

	return NULL;
}

/*
* FS_FileExists
*/
static int FS_FileExists( const char *filename, bool base ) {
	searchpath_t *search;
	char tempname[FS_MAX_PATH];

	if( base ) {
		search = FS_SearchPathForBaseFile( filename, tempname, sizeof( tempname ) );
	} else {
		search = FS_SearchPathForFile( filename, tempname, sizeof( tempname ) );
	}

	if( !search ) {
		return -1;
	}

	assert( tempname[0] != '\0' );
	if( tempname[0] != '\0' ) {
		return FS_FileLength( fopen( tempname, "rb" ), true );
	}

	return -1;
}

/*
* FS_AbsoluteFileExists
*/
static int FS_AbsoluteFileExists( const char *filename ) {
	FILE *f;

	if( !COM_ValidateFilename( filename ) ) {
		return -1;
	}

	f = fopen( filename, "rb" );
	if( !f ) {
		return -1;
	}

	return FS_FileLength( f, true );
}

/*
* FS_FileModeStr
*/
static void FS_FileModeStr( int mode, char *modestr, size_t size ) {
	int rwa = mode & FS_RWA_MASK;
	Q_snprintfz( modestr, size, "%sb%s",
				 rwa == FS_WRITE ? "w" : ( rwa == FS_APPEND ? "a" : "r" ),
				 mode & FS_UPDATE ? "+" : "" );
}

/*
* FS_FOpenAbsoluteFile
*/
int FS_FOpenAbsoluteFile( const char *filename, int *filenum, int mode ) {
	FILE *f = NULL;
	gzFile gzf = NULL;
	filehandle_t *file;
	int end;
	bool gz;
	bool update;
	int realmode;
	char modestr[4] = { 0, 0, 0, 0 };

	realmode = mode;
	gz = mode & FS_GZ ? true : false;
	update = mode & FS_UPDATE ? true : false;
	mode = mode & FS_RWA_MASK;

	assert( filenum || mode == FS_READ );

	if( gz && update ) {
		return -1; // unsupported

	}
	if( filenum ) {
		*filenum = 0;
	}

	if( !filenum ) {
		if( mode == FS_READ ) {
			return FS_AbsoluteFileExists( filename );
		}
		return -1;
	}

	if( !COM_ValidateFilename( filename ) ) {
		return -1;
	}

	if( mode == FS_WRITE || mode == FS_APPEND ) {
		FS_CreateAbsolutePath( filename );
	}

	FS_FileModeStr( realmode, modestr, sizeof( modestr ) );

	if( gz ) {
		gzf = gzopen( filename, modestr );
	} else {
		f = fopen( filename, modestr );
	}
	if( !f && !gzf ) {
		Com_DPrintf( "FS_FOpenAbsoluteFile: can't %s %s\n", ( mode == FS_READ ? "find" : "write to" ), filename );
		return -1;
	}

	end = ( mode == FS_WRITE || gz ? 0 : FS_FileLength( f, false ) );

	*filenum = FS_OpenFileHandle();
	file = &fs_filehandles[*filenum - 1];
	file->fstream = f;
	file->uncompressedSize = end;
	file->gzstream = gzf;
	file->gzlevel = Z_DEFAULT_COMPRESSION;

	if( gzf ) {
		gzbuffer( gzf, FS_GZ_BUFSIZE );
	}

	return end;
}

/*
* FS_IsUrl
*/
bool FS_IsUrl( const char *url ) {
	if( !strncmp( url, "http://", 7 ) ) {
		return true;
	}
	if( !strncmp( url, "https://", 8 ) ) {
		return true;
	}
	return false;
}

/*
* _FS_FOpenFile
*
* Finds the file in the search path. Returns filesize and an open handle
*/
static int _FS_FOpenFile( const char *filename, int *filenum, int mode, bool base ) {
	searchpath_t *search;
	filehandle_t *file;
	bool gz;
	bool update;
	bool cache;
	gzFile gzf = NULL;
	int realmode;
	char tempname[FS_MAX_PATH];

	realmode = mode;
	gz = mode & FS_GZ ? true : false;
	update = mode & FS_UPDATE ? true : false;
	cache = mode & FS_CACHE ? true : false;
	mode = mode & FS_RWA_MASK;

	assert( mode == FS_READ || mode == FS_WRITE || mode == FS_APPEND );
	assert( filenum || mode == FS_READ );

	if( filenum ) {
		*filenum = 0;
	}

	if( !filenum ) {
		if( mode == FS_READ ) {
			return FS_FileExists( filename, base );
		}
		return -1;
	}

	if( mode == FS_WRITE || mode == FS_APPEND || update || cache ) {
		int end;
		char modestr[4] = { 0, 0, 0, 0 };
		FILE *f = NULL;
		const char *dir;

		dir = cache ? FS_CacheDirectory() : FS_WriteDirectory();

		if( base ) {
			Q_snprintfz( tempname, sizeof( tempname ), "%s/%s", dir, filename );
		} else {
			Q_snprintfz( tempname, sizeof( tempname ), "%s/%s/%s", dir, FS_GameDirectory(), filename );
		}
		FS_CreateAbsolutePath( tempname );

		FS_FileModeStr( realmode, modestr, sizeof( modestr ) );

		if( gz ) {
			gzf = gzopen( tempname, modestr );
		} else {
			f = fopen( tempname, modestr );
		}
		if( !f && !gzf ) {
			return -1;
		}

		end = 0;
		if( mode == FS_APPEND || mode == FS_READ || update ) {
			end = f ? FS_FileLength( f, false ) : 0;
		}

		*filenum = FS_OpenFileHandle();
		file = &fs_filehandles[*filenum - 1];
		file->fstream = f;
		file->uncompressedSize = end;
		file->gzstream = gzf;
		file->gzlevel = Z_DEFAULT_COMPRESSION;

		if( gzf ) {
			gzbuffer( gzf, FS_GZ_BUFSIZE );
		}
		return end;
	}

	if( base ) {
		search = FS_SearchPathForBaseFile( filename, tempname, sizeof( tempname ) );
	} else {
		search = FS_SearchPathForFile( filename, tempname, sizeof( tempname ) );
	}

	if( !search ) {
		goto notfound_dprint;
	}

	int end;
	FILE *f;

	assert( tempname[0] != '\0' );
	if( tempname[0] == '\0' ) {
		goto error;
	}

	f = fopen( tempname, "rb" );
	end = FS_FileLength( f, gz );

	if( gz ) {
		f = NULL;
		gzf = gzopen( tempname, "rb" );
		assert( gzf );
	}

	*filenum = FS_OpenFileHandle();
	file = &fs_filehandles[*filenum - 1];
	file->fstream = f;
	file->uncompressedSize = end;
	file->gzstream = gzf;
	file->gzlevel = Z_DEFAULT_COMPRESSION;

	Com_DPrintf( "FS_FOpen%sFile: %s\n", ( base ? "Base" : "" ), tempname );
	return end;

notfound_dprint:
	Com_DPrintf( "FS_FOpen%sFile: can't find %s\n", ( base ? "Base" : "" ), filename );

error:
	*filenum = 0;
	return -1;
}

/*
* FS_FOpenFile
*/
int FS_FOpenFile( const char *filename, int *filenum, int mode ) {
	return _FS_FOpenFile( filename, filenum, mode, false );
}

/*
* FS_FOpenBaseFile
*/
int FS_FOpenBaseFile( const char *filename, int *filenum, int mode ) {
	return _FS_FOpenFile( filename, filenum, mode, true );
}

/*
* FS_FCloseFile
*/
void FS_FCloseFile( int file ) {
	filehandle_t *fh;

	if( !file ) {
		return; // return silently

	}
	fh = FS_FileHandleForNum( file );

	if( fh->fstream ) {
		fclose( fh->fstream );
		fh->fstream = NULL;
	}
	if( fh->gzstream ) {
		gzclose( fh->gzstream );
		fh->gzstream = NULL;
	}

	FS_CloseFileHandle( fh );
}

/*
* FS_ReadFile
*
* Properly handles partial reads
*/
static int FS_ReadFile( uint8_t *buf, size_t len, filehandle_t *fh ) {
	return (int)fread( buf, 1, len, fh->fstream );
}

/*
* FS_Read
*
* Properly handles partial reads
*/
int FS_Read( void *buffer, size_t len, int file ) {
	filehandle_t *fh;
	int total;

	// read in chunks for progress bar
	if( !len || !buffer ) {
		return 0;
	}

	fh = FS_FileHandleForNum( file );

	if( fh->gzstream ) {
		total = gzread( fh->gzstream, buffer, len );
	} else if( fh->fstream ) {
		total = FS_ReadFile( ( uint8_t * )buffer, len, fh );
	} else {
		return 0;
	}

	if( total < 0 ) {
		return total;
	}

	fh->offset += (unsigned)total;
	return total;
}

/*
* FS_Print
*/
int FS_Print( int file, const char *msg ) {
	return ( msg ? FS_Write( msg, strlen( msg ), file ) : 0 );
}

/*
* FS_Printf
*/
int FS_Printf( int file, const char *format, ... ) {
	char msg[8192];
	size_t len;
	va_list argptr;

	va_start( argptr, format );
	if( ( len = Q_vsnprintfz( msg, sizeof( msg ), format, argptr ) ) >= sizeof( msg ) - 1 ) {
		msg[sizeof( msg ) - 1] = '\0';
		Com_Printf( "FS_Printf: Buffer overflow" );
	}
	va_end( argptr );

	return FS_Write( msg, len, file );
}

/*
* FS_Write
*
* Properly handles partial writes
*/
int FS_Write( const void *buffer, size_t len, int file ) {
	filehandle_t *fh;
	size_t written;
	uint8_t *buf;

	fh = FS_FileHandleForNum( file );

	if( fh->gzstream ) {
		return gzwrite( fh->gzstream, buffer, len );
	}

	if( !fh->fstream ) {
		return 0;
	}

	buf = ( uint8_t * )buffer;
	if( !buf ) {
		return 0;
	}

	written = fwrite( buf, 1, len, fh->fstream );
	if( written != len ) {
		Sys_Error( "FS_Write: can't write %" PRIuPTR " bytes", (uintptr_t)len );
	}

	fh->offset += written;

	return written;
}

/*
* FS_Tell
*/
int FS_Tell( int file ) {
	filehandle_t *fh;

	fh = FS_FileHandleForNum( file );

	if( fh->gzstream ) {
		return gztell( fh->gzstream );
	}
	return (int)fh->offset;
}

/*
* FS_Seek
*/
int FS_Seek( int file, int offset, int whence ) {
	filehandle_t *fh;
	int currentOffset;

	fh = FS_FileHandleForNum( file );

	if( fh->gzstream ) {
		return gzseek( fh->gzstream, offset,
						whence == FS_SEEK_CUR ? SEEK_CUR :
						( whence == FS_SEEK_END ? SEEK_END :
						  ( whence == FS_SEEK_SET ? SEEK_SET : -1 ) ) );
	}

	currentOffset = (int)fh->offset;

	if( whence == FS_SEEK_CUR ) {
		offset += currentOffset;
	} else if( whence == FS_SEEK_END ) {
		offset += fh->uncompressedSize;
	} else if( whence != FS_SEEK_SET ) {
		return -1;
	}

	// clamp so we don't get out of bounds
	if( offset < 0 ) {
		return -1;
	}

	if( offset == currentOffset ) {
		return 0;
	}

	if( !fh->fstream ) {
		return -1;
	}
	if( offset > (int)fh->uncompressedSize ) {
		return -1;
	}

	fh->offset = offset;
	return fseek( fh->fstream, offset, SEEK_SET );
}

/*
* FS_FFlush
*/
int FS_Flush( int file ) {
	filehandle_t *fh;

	fh = FS_FileHandleForNum( file );
	if( fh->gzstream ) {
		return gzflush( fh->gzstream, Z_FINISH );
	}
	if( !fh->fstream ) {
		return 0;
	}

	return fflush( fh->fstream );
}

/*
* FS_FileNo
*
* Returns the file handle that can be used in system calls.
*/
int FS_FileNo( int file ) {
	filehandle_t *fh;

	fh = FS_FileHandleForNum( file );
	if( fh->fstream && !fh->gzstream ) {
		return Sys_FS_FileNo( fh->fstream );
	}

	return -1;
}

/*
* FS_SetCompressionLevel
*/
void FS_SetCompressionLevel( int file, int level ) {
	filehandle_t *fh = FS_FileHandleForNum( file );
	if( fh->gzstream ) {
		fh->gzlevel = level;
		gzsetparams( fh->gzstream, level,  Z_DEFAULT_STRATEGY );
	}
}

/*
* FS_GetCompressionLevel
*/
int FS_GetCompressionLevel( int file ) {
	filehandle_t *fh = FS_FileHandleForNum( file );
	if( fh->gzstream ) {
		return fh->gzlevel;
	}
	return 0;
}

/*
* _FS_LoadFile
*/
static int _FS_LoadFile( int fhandle, unsigned int len, void **buffer, void *stack, size_t stackSize, const char *filename, int fileline ) {
	uint8_t *buf;

	if( !fhandle ) {
		if( buffer ) {
			*buffer = NULL;
		}
		return -1;
	}

	if( !buffer ) {
		FS_FCloseFile( fhandle );
		return len;
	}

	if( stack && ( stackSize > len ) ) {
		buf = ( uint8_t* )stack;
	} else {
		buf = ( uint8_t* )_Mem_AllocExt( tempMemPool, len + 1, 0, 0, 0, 0, filename, fileline );
	}
	buf[len] = 0;
	*buffer = buf;

	FS_Read( buf, len, fhandle );
	FS_FCloseFile( fhandle );

	return len;
}

/*
* FS_LoadFileExt
*
* Filename are relative to the quake search path
* a null buffer will just return the file length without loading
*/
int FS_LoadFileExt( const char *path, int flags, void **buffer, void *stack, size_t stackSize, const char *filename, int fileline ) {
	unsigned int len;
	int fhandle;

	len = FS_FOpenFile( path, &fhandle, FS_READ | flags );
	return _FS_LoadFile( fhandle, len, buffer, stack, stackSize, filename, fileline );
}

/*
* FS_LoadBaseFileExt
*
* a NULL buffer will just return the file length without loading
*/
int FS_LoadBaseFileExt( const char *path, int flags, void **buffer, void *stack, size_t stackSize, const char *filename, int fileline ) {
	unsigned int len;
	int fhandle;

	// look for it in the filesystem
	len = FS_FOpenBaseFile( path, &fhandle, FS_READ | flags );
	return _FS_LoadFile( fhandle, len, buffer, stack, stackSize, filename, fileline );
}

/*
* FS_FreeFile
*/
void FS_FreeFile( void *buffer ) {
	Mem_TempFree( buffer );
}

/*
* FS_FreeBaseFile
*/
void FS_FreeBaseFile( void *buffer ) {
	FS_FreeFile( buffer );
}

/*
* FS_ChecksumAbsoluteFile
*/
unsigned FS_ChecksumAbsoluteFile( const char *filename ) {
	Com_DPrintf( "Calculating checksum for file: %s\n", filename );

	uint32_t hash = Hash32( NULL, 0 );

	int filenum;
	int left = FS_FOpenAbsoluteFile( filename, &filenum, FS_READ );
	if( left == -1 ) {
		return 0;
	}

	int length;
	uint8_t buffer[FS_MAX_BLOCK_SIZE];
	while( ( length = FS_Read( buffer, sizeof( buffer ), filenum ) ) ) {
		left -= length;
		hash = Hash32( buffer, length, hash );
	}

	FS_FCloseFile( filenum );

	if( left != 0 ) {
		return 0;
	}

	return hash;
}

/*
* FS_ChecksumBaseFile
*/
unsigned FS_ChecksumBaseFile( const char *filename ) {
	const char *fullname;

	fullname = FS_AbsoluteNameForBaseFile( filename );
	if( !fullname ) {
		return false;
	}

	return FS_ChecksumAbsoluteFile( fullname );
}

/*
* FS_RemoveAbsoluteFile
*/
bool FS_RemoveAbsoluteFile( const char *filename ) {
	if( !COM_ValidateFilename( filename ) ) {
		return false;
	}

	// ch : this should return false on error, true on success, c++'ify:
	// return ( !remove( filename ) );
	return ( remove( filename ) == 0 ? true : false );
}

/*
* _FS_RemoveFile
*/
static bool _FS_RemoveFile( const char *filename, bool base ) {
	const char *fullname;

	if( base ) {
		fullname = FS_AbsoluteNameForBaseFile( filename );
	} else {
		fullname = FS_AbsoluteNameForFile( filename );
	}

	if( !fullname ) {
		return false;
	}

	if( strncmp( fullname, FS_WriteDirectory(), strlen( FS_WriteDirectory() ) ) ) {
		return false;
	}

	return ( FS_RemoveAbsoluteFile( fullname ) );
}

/*
* FS_RemoveBaseFile
*/
bool FS_RemoveBaseFile( const char *filename ) {
	return _FS_RemoveFile( filename, true );
}

/*
* FS_RemoveFile
*/
bool FS_RemoveFile( const char *filename ) {
	return _FS_RemoveFile( filename, false );
}

/*
* _FS_MoveFile
*/
bool _FS_MoveFile( const char *src, const char *dst, bool base, const char *dir ) {
	char temp[FS_MAX_PATH];
	const char *fullname, *fulldestname;

	if( base ) {
		fullname = FS_AbsoluteNameForBaseFile( src );
	} else {
		fullname = FS_AbsoluteNameForFile( src );
	}

	if( !fullname ) {
		return false;
	}

	if( strncmp( fullname, dir, strlen( dir ) ) ) {
		return false;
	}

	if( !COM_ValidateRelativeFilename( dst ) ) {
		return false;
	}

	if( base ) {
		fulldestname = va_r( temp, sizeof( temp ), "%s/%s", dir, dst );
	} else {
		fulldestname = va_r( temp, sizeof( temp ), "%s/%s/%s", dir, FS_GameDirectory(), dst );
	}
	return rename( fullname, fulldestname ) == 0 ? true : false;
}

/*
* FS_MoveFile
*/
bool FS_MoveFile( const char *src, const char *dst ) {
	return _FS_MoveFile( src, dst, false, FS_WriteDirectory() );
}

/*
* FS_MoveBaseFile
*/
bool FS_MoveBaseFile( const char *src, const char *dst ) {
	return _FS_MoveFile( src, dst, true, FS_WriteDirectory() );
}

/*
* FS_PathGetFileListExt
*/
static int FS_PathGetFileListExt( searchpath_t *search, const char *dir, const char *extension, searchfile_t *files, size_t size ) {
	int i;
	unsigned found;
	size_t dirlen, extlen;
	char tempname[FS_MAX_PATH];

	assert( search );
	assert( !dir || dir[strlen( dir ) - 1] != '/' );
	assert( files );
	assert( size );

	if( !search || ( dir && dir[strlen( dir ) - 1] == '/' ) || !files || !size ) {
		return 0;
	}

	found = 0;
	dirlen = 0;
	extlen = 0;

	if( dir ) {
		dirlen = strlen( dir );
	}

	if( extension /* && extension[0] != '/'*/ ) {
		extlen = strlen( extension );
	} else {
		extlen = strlen( "*.*" );
	}

	size_t pathlen;
	int numfiles = 0;
	char **filenames, *filepath;
	const char *filename;
	unsigned int musthave, canthave;

	musthave = 0;
	canthave = SFF_HIDDEN | SFF_SYSTEM;

	pathlen = strlen( search->path ) + 1;

	if( extension ) {
		if( extension[0] != '/' ) {
			canthave |= SFF_SUBDIR;
		} else {
			musthave |= SFF_SUBDIR;
		}
	}

	Q_snprintfz( tempname, sizeof( tempname ), "%s/%s%s*%s",
				 search->path, dir, dirlen ? "/" : "", ( extension && ( extension[0] != '/' ) ) ? extension : ".*" );
	filenames = FS_ListFiles( tempname, &numfiles, musthave, canthave );

	Q_snprintfz( tempname, sizeof( tempname ), "%s%s*%s",
				 dir, dirlen ? "/" : "", ( extension && ( extension[0] != '/' ) ) ? extension : "" );

	for( i = 0; i < numfiles; i++ ) {
		filepath = filenames[i];
		filename = filepath + pathlen + ( dirlen ? dirlen + 1 : 0 );

		if( found < size ) {
			size_t len = strlen( filename );

			if( ( musthave & SFF_SUBDIR ) ) {
				if( filename[len - 1] != '/' ) {
					files[found].name = ( char* )Mem_ZoneMalloc( len + 2 );
					strcpy( files[found].name, filename );
					files[found].name[len] = '/';
					files[found].name[len + 1] = 0;
				} else {
					files[found].name = ZoneCopyString( filename );
				}
			} else {
				if( extension && ( len <= extlen ) ) {
					Mem_ZoneFree( filepath );
					continue;
				}
				files[found].name = ZoneCopyString( filename );
			}
			files[found].searchPath = search;
			found++;
		}

		Mem_ZoneFree( filepath );
	}
	Mem_ZoneFree( filenames );

	return found;
}

#define FS_MIN_SEARCHFILES      0x400
#define FS_MAX_SEARCHFILES      0xFFFF          // cap
static int FS_SortFilesCmp( const searchfile_t *file1, const searchfile_t *file2 ) {
	return Q_stricmp( ( file1 )->name, ( file2 )->name );
}

/*
* FS_GetFileListExt_
*
* Directory names should not contain a trailing /
* Directory names, beginning with a '<' only return downloaded files.
* Directory names, beginning with a '>' only return stock/official files.
*/
static int FS_GetFileListExt_( const char *dir, const char *extension, char *buf, size_t *bufsize, int maxFiles, int start, int end ) {
	int i;
	int allfound = 0, found, limit;
	size_t len, alllen;
	searchpath_t *search;
	searchfile_t *files;
	static int maxFilesCache;
	static char dircache[MAX_QPATH], extcache[MAX_QPATH];
	bool useCache;
	bool onlyDownloads = false, skipDownloads = false;

	assert( !dir || dir[strlen( dir ) - 1] != '/' );

	if( dir && dir[strlen( dir ) - 1] == '/' ) {
		return 0;
	}

	if( fs_cursearchfiles ) {
		useCache = ( maxFilesCache == maxFiles ? true : false );
		if( useCache ) {
			useCache = dir ? ( !strcmp( dircache, dir ) ? true : false ) : ( dircache[0] == '\0' ? true : false );
			if( useCache ) {
				useCache = extension ? ( !strcmp( extcache, extension ) ? true : false ) : ( extcache[0] == '\0' ? true : false );
			}
		}
	} else {
		useCache = false;
	}

	maxFilesCache = maxFiles;
	if( dir ) {
		Q_strncpyz( dircache, dir, sizeof( dircache ) );
	} else {
		dircache[0] = '\0';
	}
	if( extension ) {
		Q_strncpyz( extcache, extension, sizeof( extcache ) );
	} else {
		extcache[0] = '\0';
	}

	if( dir[0] == '<' ) {
		onlyDownloads = true;
		dir++;
	} else if( dir[0] == '>' ) {
		skipDownloads = true;
		dir++;
	}

	files = fs_searchfiles;
	if( !useCache ) {
		QMutex_Lock( fs_searchpaths_mutex );

		search = fs_searchpaths;
		while( search ) {
			if( fs_downloads_searchpath ) {
				if( ( onlyDownloads && search->base != fs_downloads_searchpath ) ||
					( skipDownloads && search->base == fs_downloads_searchpath ) ) {
					search = search->next;
					continue;
				}
			}

			limit = maxFiles ? min( fs_numsearchfiles, maxFiles ) : fs_numsearchfiles;
			found = FS_PathGetFileListExt( search, dir, extension, files + allfound,
										   fs_numsearchfiles - allfound );

			if( allfound + found == fs_numsearchfiles ) {
				if( limit == maxFiles || fs_numsearchfiles == FS_MAX_SEARCHFILES ) {
					break; // we are done
				}
				fs_numsearchfiles *= 2;
				if( fs_numsearchfiles > FS_MAX_SEARCHFILES ) {
					fs_numsearchfiles = FS_MAX_SEARCHFILES;
				}
				fs_searchfiles = files = ( searchfile_t* )FS_Realloc( fs_searchfiles, sizeof( searchfile_t ) * fs_numsearchfiles );
				for( i = 0; i < found; i++ )
					Mem_ZoneFree( files[allfound + i].name );
				continue;
			}

			allfound += found;
			search = search->next;
		}

		QMutex_Unlock( fs_searchpaths_mutex );

		qsort( files, allfound, sizeof( searchfile_t ), ( int ( * )( const void *, const void * ) )FS_SortFilesCmp );

		// remove all duplicates
		for( i = 1; i < allfound; ) {
			if( FS_SortFilesCmp( &files[i - 1], &files[i] ) ) {
				i++;
				continue;
			}

			Mem_ZoneFree( files[i - 1].name );
			memmove( &files[i - 1], &files[i], ( allfound - i ) * sizeof( *files ) );
			allfound--;
		}
	}

	if( !useCache ) {
		fs_cursearchfiles = allfound;
	} else {
		allfound = fs_cursearchfiles;
	}

	if( start < 0 ) {
		start = 0;
	}
	if( !end ) {
		end = allfound;
	} else if( end > allfound ) {
		end = allfound;
	}

	if( bufsize ) {
		found = 0;

		if( buf ) {
			alllen = 0;
			for( i = start; i < end; i++ ) {
				len = strlen( files[i].name );
				if( *bufsize < len + 1 + alllen + 1 ) {
					break; // we are done
				}
				strcpy( buf + alllen, files[i].name );
				alllen += len + 1;
				found++;
			}
			buf[alllen] = '\0';
		} else {
			*bufsize = 0;
			for( i = start; i < end; found++, i++ )
				*bufsize += strlen( files[i].name ) + 1;
			*bufsize = *bufsize + 1;
		}

		return found;
	}

	return allfound;
}

/*
* FS_GetFileList
*/
int FS_GetFileListExt( const char *dir, const char *extension, char *buf, size_t *bufsize, int start, int end ) {
	//	return FS_GetFileListExt_( dir, extension, buf, bufsize, buf2, buf2size, 0, 0, 0 );		// 0 - no limit
	return FS_GetFileListExt_( dir, extension, buf, bufsize, FS_MAX_SEARCHFILES, start, end );
}

/*
* FS_GetFileList
*/
int FS_GetFileList( const char *dir, const char *extension, char *buf, size_t bufsize, int start, int end ) {
	//	return FS_GetFileListExt_( dir, extension, buf, &bufsize, 0, start, end );				// 0 - no limit
	return FS_GetFileListExt_( dir, extension, buf, &bufsize, FS_MAX_SEARCHFILES, start, end );
}

/*
* FS_GameDirectory
*
* Returns the current game directory, without the path
*/
const char *FS_GameDirectory( void ) {
	return fs_basegame->string;
}

/*
* FS_BaseGameDirectory
*
* Returns the current base game directory, without the path
*/
const char *FS_BaseGameDirectory( void ) {
	assert( fs_basegame && fs_basegame->string && fs_basegame->string[0] );
	return fs_basegame->string;
}

/*
* FS_WriteDirectory
*
* Returns directory where we can write, no gamedir attached
*/
const char *FS_WriteDirectory( void ) {
	return fs_write_searchpath->path;
}

/*
* FS_CacheDirectory
*
* Returns directory where we can write cached files
*/
const char *FS_CacheDirectory( void ) {
	const char *dir = Sys_FS_GetCacheDirectory();
	return dir ? dir : FS_WriteDirectory();
}

/*
* FS_DownloadsDirectory
*
* Returns directory where we can store downloads to, no gamedir attached.
* Returns NULL if downloads are disabled.
*/
const char *FS_DownloadsDirectory( void ) {
	if( fs_downloads_searchpath ) {
		return fs_downloads_searchpath->path;
	}
	return NULL;
}

/*
* FS_CreateAbsolutePath
*
* Creates any directories needed to store the given filename
*/
void FS_CreateAbsolutePath( const char *path ) {
	char *ofs;

	for( ofs = ( char * )path + 1; *ofs; ofs++ ) {
		if( *ofs == '/' ) {
			// create the directory
			*ofs = 0;
			Sys_FS_CreateDirectory( path );
			*ofs = '/';
		}
	}
}

/*
* FS_AbsoluteNameForFile
*
* Gives absolute name for a game file
* NULL if not found
*/
const char *FS_AbsoluteNameForFile( const char *filename ) {
	static char absolutename[FS_MAX_PATH];
	searchpath_t *search = FS_SearchPathForFile( filename, NULL, 0 );

	if( !search ) {
		return NULL;
	}

	Q_snprintfz( absolutename, sizeof( absolutename ), "%s/%s", search->path, filename );
	return absolutename;
}

/*
* FS_AbsoluteNameForBaseFile
*
* Gives absolute name for a base file
* NULL if not found
*/
const char *FS_AbsoluteNameForBaseFile( const char *filename ) {
	static char absolutename[FS_MAX_PATH];
	searchpath_t *search = FS_SearchPathForBaseFile( filename, NULL, 0 );

	if( !search ) {
		return NULL;
	}

	Q_snprintfz( absolutename, sizeof( absolutename ), "%s/%s", search->path, filename );
	return absolutename;
}

/*
* FS_BaseNameForFile
*/
const char *FS_BaseNameForFile( const char *filename ) {
	const char *p;
	searchpath_t *search = FS_SearchPathForFile( filename, NULL, 0 );

	if( !search ) {
		return NULL;
	}

	// only give the basename part
	p = strrchr( search->path, '/' );

	if( !p ) {
		return va( "%s/%s", search->path, filename );
	}
	return va( "%s/%s", p + 1, filename );
}

/*
* FS_TouchGamePath
*/
static void FS_TouchGamePath( searchpath_t *basepath, const char *gamedir ) {
	searchpath_t *search;
	size_t path_size;

	search = ( searchpath_t* )FS_Malloc( sizeof( searchpath_t ) );

	path_size = sizeof( char ) * ( strlen( basepath->path ) + 1 + strlen( gamedir ) + 1 );
	search->path = ( char* )FS_Malloc( path_size );
	search->base = basepath;
	Q_snprintfz( search->path, path_size, "%s/%s", basepath->path, gamedir );

	search->next = fs_searchpaths;
	fs_searchpaths = search;
}

/*
* FS_TouchGameDirectory
*/
static void FS_TouchGameDirectory( const char *gamedir ) {
	searchpath_t *old, *prev, *basepath;

	// add for every basepath, in reverse order
	old = fs_searchpaths;
	prev = NULL;
	while( prev != fs_basepaths ) {
		basepath = fs_basepaths;
		while( basepath->next != prev )
			basepath = basepath->next;
		FS_TouchGamePath( basepath, gamedir );
		prev = basepath;
	}
}

/*
* FS_AddBasePath
*/
static void FS_AddBasePath( const char *path ) {
	searchpath_t *newpath;

	newpath = ( searchpath_t* )FS_Malloc( sizeof( searchpath_t ) );
	newpath->path = FS_CopyString( path );
	newpath->next = fs_basepaths;
	fs_basepaths = newpath;
	COM_SanitizeFilePath( newpath->path );
}

/*
* FS_FreeSearchFiles
*/
static void FS_FreeSearchFiles( void ) {
	// free temp memory
	for( int i = 0; i < fs_cursearchfiles; i++ ) {
		Mem_ZoneFree( fs_searchfiles[i].name );
	}
	fs_cursearchfiles = 0;
}

/*
* FS_Init
*/
void FS_Init( void ) {
	int i;
	const char *homedir;
	const char *cachedir;
	char downloadsdir[FS_MAX_PATH];

	assert( !fs_initialized );

	fs_fh_mutex = QMutex_Create();
	fs_searchpaths_mutex = QMutex_Create();

	fs_mempool = Mem_AllocPool( NULL, "Filesystem" );

	fs_numsearchfiles = FS_MIN_SEARCHFILES;
	fs_searchfiles = ( searchfile_t* )FS_Malloc( sizeof( searchfile_t ) * fs_numsearchfiles );

	memset( fs_filehandles, 0, sizeof( fs_filehandles ) );

	//
	// link filehandles
	//
	fs_free_filehandles = fs_filehandles;
	fs_filehandles_headnode.prev = &fs_filehandles_headnode;
	fs_filehandles_headnode.next = &fs_filehandles_headnode;
	for( i = 0; i < FS_MAX_HANDLES - 1; i++ )
		fs_filehandles[i].next = &fs_filehandles[i + 1];

	//
	// set basepaths
	//
	fs_cdpath = Cvar_Get( "fs_cdpath", "", CVAR_NOSET );
	fs_basepath = Cvar_Get( "fs_basepath", ".", CVAR_NOSET );
	homedir = Sys_FS_GetHomeDirectory();
#if PUBLIC_BUILD && !DEDICATED
	fs_usehomedir = Cvar_Get( "fs_usehomedir", homedir == NULL ? "0" : "1", CVAR_NOSET );
#else
	fs_usehomedir = Cvar_Get( "fs_usehomedir", "0", CVAR_NOSET );
#endif
	fs_usedownloadsdir = Cvar_Get( "fs_usedownloadsdir", "1", CVAR_NOSET );

	fs_downloads_searchpath = NULL;
	if( fs_usedownloadsdir->integer ) {
		if( homedir != NULL && fs_usehomedir->integer ) {
			Q_snprintfz( downloadsdir, sizeof( downloadsdir ), "%s/%s", homedir, "downloads" );
		} else {
			Q_snprintfz( downloadsdir, sizeof( downloadsdir ), "%s", "downloads" );
		}

		FS_AddBasePath( downloadsdir );
		fs_downloads_searchpath = fs_basepaths;
	}

	if( fs_cdpath->string[0] ) {
		FS_AddBasePath( fs_cdpath->string );
	}

	FS_AddBasePath( fs_basepath->string );
	fs_root_searchpath = fs_basepaths;
	fs_write_searchpath = fs_basepaths;

	if( homedir != NULL && fs_usehomedir->integer ) {
		FS_AddBasePath( homedir );
		fs_write_searchpath = fs_basepaths;
	}

	cachedir = Sys_FS_GetCacheDirectory();
	if( cachedir ) {
		FS_AddBasePath( cachedir );
	}

	//
	// set game directories
	//
	fs_basegame = Cvar_Get( "fs_basegame", DEFAULT_BASEGAME, CVAR_READONLY );
	if( !fs_basegame->string[0] ) {
		Cvar_ForceSet( "fs_basegame", DEFAULT_BASEGAME );
	}

	FS_TouchGameDirectory( fs_basegame->string );

	fs_base_searchpaths = fs_searchpaths;

	// done
	Com_Printf( "Using %s for writing\n", FS_WriteDirectory() );

	fs_cursearchfiles = 0;

	fs_initialized = true;
}

/*
* FS_Frame
*/
void FS_Frame( void ) {
	ZoneScoped;
	FS_FreeSearchFiles();
}

/*
* FS_Shutdown
*/
void FS_Shutdown( void ) {
	searchpath_t *search;

	if( !fs_initialized ) {
		return;
	}

	FS_FreeSearchFiles();
	FS_Free( fs_searchfiles );
	fs_numsearchfiles = 0;

	QMutex_Lock( fs_searchpaths_mutex );

	while( fs_searchpaths ) {
		search = fs_searchpaths;
		fs_searchpaths = search->next;

		FS_Free( search->path );
		FS_Free( search );
	}

	QMutex_Unlock( fs_searchpaths_mutex );

	while( fs_basepaths ) {
		search = fs_basepaths;
		fs_basepaths = search->next;

		FS_Free( search->path );
		FS_Free( search );
	}

	Mem_FreePool( &fs_mempool );

	QMutex_Destroy( &fs_fh_mutex );
	QMutex_Destroy( &fs_searchpaths_mutex );

	fs_initialized = false;
}
