#include "qcommon/base.h"
#include "gameshared/q_shared.h"

#include "zstd/zstd.h"

bool Decompress( Span< const char > name, Allocator * a, Span< const u8 > compressed, Span< u8 > * decompressed ) {
	if( compressed.n < 4 ) {
		Com_GGPrint( S_COLOR_RED "Compressed data too short: {}", name );
		return false;
	}

	u32 zstd_magic = ZSTD_MAGICNUMBER;
	if( memcmp( compressed.ptr, &zstd_magic, sizeof( zstd_magic ) ) != 0 ) {
		Com_GGPrint( S_COLOR_RED "{} isn't a zstd file", name );
		return false;
	}

	unsigned long long decompressed_size = ZSTD_getFrameContentSize( compressed.ptr, compressed.n );
	if( decompressed_size == ZSTD_CONTENTSIZE_ERROR || decompressed_size == ZSTD_CONTENTSIZE_UNKNOWN ) {
		Com_GGPrint( S_COLOR_RED "Can't decompress {}", name );
		return false;
	}

	*decompressed = AllocSpan< u8 >( a, decompressed_size );
	{
		TracyZoneScopedN( "ZSTD_decompress" );
		size_t r = ZSTD_decompress( decompressed->ptr, decompressed->n, compressed.ptr, compressed.n );
		if( r != decompressed_size ) {
			Com_GGPrint( S_COLOR_RED "Can't decompress {}: {}", name, ZSTD_getErrorName( r ) );
			Free( a, decompressed->ptr );
			return false;
		}
	}

	return true;
}
