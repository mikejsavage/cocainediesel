#pragma once

#include "qcommon/types.h"

constexpr u32 FourCC( const char ( &fourcc )[ 5 ] ) {
	return u32( fourcc[ 0 ] ) | ( u32( fourcc[ 1 ] ) << 8 ) | ( u32( fourcc[ 2 ] ) << 16 ) | ( u32( fourcc[ 3 ] ) << 24 );
}

enum DDSTextureFormat : u32 {
	DDSTextureFormat_BC1 = FourCC( "DXT1" ),
	DDSTextureFormat_BC3 = FourCC( "DXT5" ),
	DDSTextureFormat_BC4 = FourCC( "ATI1" ),
	DDSTextureFormat_BC5 = FourCC( "ATI2" ),
};

constexpr u32 DDSTextureFormatFlag_FourCC = 0x4;
constexpr u32 DDSMagic = FourCC( "DDS " );

struct DDSHeader {
	u32 magic;
	u32 shit0[ 2 ];
	u32 height, width;
	u32 shit1[ 2 ];
	u32 mipmap_count;
	u32 shit2[ 12 ];
	u32 format_flags;
	DDSTextureFormat format;
	u32 shit3[ 10 ];
};

struct BC4Block {
	u8 data[ 8 ];
};
