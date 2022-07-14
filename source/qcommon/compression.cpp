#include "qcommon/base.h"
#include "qcommon/qcommon.h"

#include "zstd/zstd.h"

bool Decompress( const char * name, Allocator * a, Span< const u8 > compressed, Span< u8 > * decompressed ) {
	if( compressed.n < 4 ) {
		Com_Printf( S_COLOR_RED "Compressed data too short: %s\n", name );
		return false;
	}

	u32 zstd_magic = ZSTD_MAGICNUMBER;
	if( memcmp( compressed.ptr, &zstd_magic, sizeof( zstd_magic ) ) != 0 ) {
		Com_Printf( S_COLOR_RED "%s isn't a zstd file\n", name );
		return false;
	}

	unsigned long long decompressed_size = ZSTD_getFrameContentSize( compressed.ptr, compressed.n );
	if( decompressed_size == ZSTD_CONTENTSIZE_ERROR || decompressed_size == ZSTD_CONTENTSIZE_UNKNOWN ) {
		Com_Printf( S_COLOR_RED "Can't decompress %s\n", name );
		return false;
	}

	*decompressed = ALLOC_SPAN( a, u8, decompressed_size );
	{
		TracyZoneScopedN( "ZSTD_decompress" );
		size_t r = ZSTD_decompress( decompressed->ptr, decompressed->n, compressed.ptr, compressed.n );
		if( r != decompressed_size ) {
			Com_Printf( S_COLOR_RED "Can't decompress %s: %s\n", name, ZSTD_getErrorName( r ) );
			FREE( a, decompressed->ptr );
			return false;
		}
	}

	return true;
}
