#include "qcommon/base.h"
#include "qcommon/fs.h"
#include "qcommon/span2d.h"
#include "qcommon/string.h"
#include "gameshared/q_shared.h"
#include "client/renderer/dds.h"

#include "rgbcx/rgbcx.h"

#include "stb/stb_image.h"
#include "stb/stb_image_resize2.h"

#include "zstd/zstd.h"

static u32 BlockFormatMipLevels( u32 w, u32 h ) {
	u32 dim = Min2( w, h );
	u32 levels = 0;

	while( dim >= 4 ) {
		dim /= 2;
		levels++;
	}

	return levels;
}

static void MipDims( u32 * mip_w, u32 * mip_h, u32 w, u32 h, u32 level ) {
	*mip_w = Max2( w >> level, u32( 1 ) );
	*mip_h = Max2( h >> level, u32( 1 ) );
}

static u32 MipSize( u32 w, u32 h, u32 level ) {
	MipDims( &w, &h, w, h, level );
	return w * h;
}

// TODO: should put this in qcommon/compression but gotta get rid of the Com_Printfs first
static Span< u8 > Compress( Allocator * a, Span< const u8 > data ) {
	size_t max_size = ZSTD_compressBound( data.n );
	u8 * compressed = AllocMany< u8 >( a, max_size );
	size_t compressed_size = ZSTD_compress( compressed, max_size, data.ptr, data.n, ZSTD_maxCLevel() );
	if( ZSTD_isError( compressed_size ) ) {
		Fatal( "ZSTD_compress: %s", ZSTD_getErrorName( compressed_size ) );
	}
	return Span< u8 >( compressed, compressed_size );
}

int main( int argc, char ** argv ) {
	if( argc != 2 && !( argc == 4 && StrEqual( argv[ 1 ], "--output-dir" ) ) ) {
		printf( "Usage: %s [--output-dir dir] <file.png>\n", argv[ 0 ] );
		return 1;
	}

	const char * png_path = argc == 2 ? argv[ 1 ] : argv[ 3 ];
	const char * output_dir = argc == 2 ? "." : argv[ 2 ];

	int w, h, num_channels;
	u8 * png = stbi_load( png_path, &w, &h, &num_channels, 0 );
	if( png == NULL ) {
		Fatal( "Can't load image: %s", stbi_failure_reason() );
	}
	defer { stbi_image_free( png ); };

	u8 * alpha_channel = AllocMany< u8 >( sys_allocator, w * h );
	defer { Free( sys_allocator, alpha_channel ); };
	for( size_t i = 0; i < checked_cast< size_t >( w * h ); i++ ) {
		alpha_channel[ i ] = png[ i * num_channels + num_channels - 1 ];
	}

	bool generate_mipmaps = IsPowerOf2( w ) && IsPowerOf2( h );
	if( !generate_mipmaps ) {
		printf( "Image isn't pow2 sized so we aren't computing mipmaps: %s\n", png_path );
	}

	u32 num_levels = generate_mipmaps ? BlockFormatMipLevels( w, h ) : 1;
	u32 total_size = 0;
	for( u32 i = 0; i < num_levels; i++ ) {
		total_size += MipSize( w, h, i );
	}

	// TODO: should use the stuff in material.cpp instead of duplicating it here
	constexpr u32 BC4BitsPerPixel = 4;

	u32 bc4_bytes = ( total_size * BC4BitsPerPixel ) / 8;
	Span< u8 > bc4 = AllocSpan< u8 >( sys_allocator, bc4_bytes );
	u32 bc4_cursor = 0;

	u8 * resized = AllocMany< u8 >( sys_allocator, w * h );

	defer { Free( sys_allocator, bc4.ptr ); };
	defer { Free( sys_allocator, resized ); };

	rgbcx::init();

	for( u32 i = 0; i < num_levels; i++ ) {
		u32 mip_w, mip_h;
		MipDims( &mip_w, &mip_h, w, h, i );

		u8 * ok = stbir_resize_uint8_linear(
			alpha_channel, w, h, 0,
			resized, mip_w, mip_h, 0,
			STBIR_1CHANNEL
		);

		if( ok == NULL ) {
			Fatal( "stb_image_resize died lol" );
		}

		Span2D< const u8 > mip = Span2D< u8 >( resized, mip_w, mip_h );

		for( u32 row = 0; row < mip_h / 4; row++ ) {
			for( u32 col = 0; col < mip_w / 4; col++ ) {
				Span2D< const u8 > src = mip.slice( col * 4, row * 4, 4, 4 );
				u8 src_block[ 16 ];
				CopySpan2D( Span2D< u8 >( src_block, 4, 4 ), src );

				Span< u8 > dst = bc4.slice( bc4_cursor, bc4_cursor + sizeof( BC4Block ) );
				rgbcx::encode_bc4( dst.ptr, src_block, 1 );
				bc4_cursor += sizeof( BC4Block );
			}
		}
	}

	Assert( bc4_cursor == bc4.num_bytes() );

	DDSHeader dds_header = { };
	dds_header.magic = DDSMagic;
	dds_header.height = h;
	dds_header.width = w;
	dds_header.mipmap_count = num_levels;
	dds_header.format_flags = DDSTextureFormatFlag_FourCC;
	dds_header.format = DDSTextureFormat_BC4;

	Span< u8 > packed = AllocSpan< u8 >( sys_allocator, sizeof( dds_header ) + bc4.num_bytes() );
	defer { Free( sys_allocator, packed.ptr ); };
	memcpy( packed.ptr, &dds_header, sizeof( dds_header ) );
	memcpy( packed.ptr + sizeof( dds_header ), bc4.ptr, bc4.num_bytes() );

	Span< u8 > compressed = Compress( sys_allocator, packed );
	defer { Free( sys_allocator, compressed.ptr ); };

	DynamicString dds_path( sys_allocator, "{}/{}.dds.zst", output_dir, StripExtension( png_path ) );
	if( !WriteFile( sys_allocator, dds_path.c_str(), compressed.ptr, compressed.num_bytes() ) ) {
		FatalErrno( "WriteFile" );
	}

	return 0;
}
