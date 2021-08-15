#include "qcommon/base.h"
#include "qcommon/fs.h"
#include "qcommon/span2d.h"
#include "qcommon/string.h"
#include "client/renderer/dds.h"

#include "rgbcx/rgbcx.h"
#include "stb/stb_image.h"
#include "stb/stb_image_resize.h"

void ShowErrorAndAbortImpl( const char * msg, const char * file, int line ) {
	printf( "%s\n", msg );
	abort();
}

static u32 BlockFormatMipLevels( u32 w, u32 h ) {
	u32 dim = Max2( w, h );
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

int main( int argc, char ** argv ) {
	if( argc != 2 ) {
		printf( "Usage: bc4 <single channel image.png>\n" );
		return 1;
	}

	int w, h, comp;
	u8 * pixels = stbi_load( argv[ 1 ], &w, &h, &comp, 1 );
	if( pixels == NULL ) {
		printf( "Can't load image: %s\n", stbi_failure_reason() );
		return 1;
	}
	defer { stbi_image_free( pixels ); };

	if( !IsPowerOf2( w ) || !IsPowerOf2( h ) ) {
		printf( "Image must be power of 2 dimensions\n" );
		return 1;
	}

	if( comp != 1 ) {
		printf( "Image must be single channel\n" );
		return 1;
	}

	u32 num_levels = BlockFormatMipLevels( w, h );
	u32 total_size = 0;
	for( u32 i = 0; i < num_levels; i++ ) {
		total_size += MipSize( w, h, i );
	}

	constexpr u32 BC4BitsPerPixel = 4;
	constexpr u32 BC4BlockSize = ( 4 * 4 * BC4BitsPerPixel ) / 8;

	u32 bc4_bytes = ( total_size * BC4BitsPerPixel ) / 8;
	Span< u8 > bc4 = ALLOC_SPAN( sys_allocator, u8, bc4_bytes );
	u32 bc4_cursor = 0;

	u8 * resized = ALLOC_MANY( sys_allocator, u8, w * h );

	defer { FREE( sys_allocator, bc4.ptr ); };
	defer { FREE( sys_allocator, resized ); };

	rgbcx::init();

	for( u32 i = 0; i < num_levels; i++ ) {
		u32 mip_w, mip_h;
		MipDims( &mip_w, &mip_h, w, h, i );

		int ok = stbir_resize_uint8(
			pixels, w, h, 0,
			resized, mip_w, mip_h, 0,
			1
		);

		if( ok == 0 ) {
			printf( "stb_image_resize died lol\n" );
			return 1;
		}

		Span2D< const u8 > mip = Span2D< u8 >( resized, mip_w, mip_h );

		for( u32 row = 0; row < mip_h / 4; row++ ) {
			for( u32 col = 0; col < mip_w / 4; col++ ) {
				Span2D< const u8 > src = mip.slice( col * 4, row * 4, 4, 4 );
				u8 src_block[ 16 ];
				CopySpan2D( Span2D< u8 >( src_block, 4, 4 ), src );

				Span< u8 > dst = bc4.slice( bc4_cursor, bc4_cursor + BC4BlockSize );
				rgbcx::encode_bc4( dst.ptr, src_block, 1 );
				bc4_cursor += BC4BlockSize;
			}
		}
	}

	assert( bc4_cursor == bc4.num_bytes() );

	DDSHeader dds_header = { };
	dds_header.magic = DDSMagic;
	dds_header.height = h;
	dds_header.width = w;
	dds_header.mipmap_count = num_levels;
	dds_header.format = DDSTextureFormat_BC4;

	DynamicString dds_path( sys_allocator, "{}.dds", argv[ 1 ] );

	FILE * dds = OpenFile( sys_allocator, dds_path.c_str(), "wb" );
	if( dds == NULL ) {
		printf( "Can't open %s for writing\n", dds_path.c_str() );
		return 1;
	}

	fwrite( &dds_header, sizeof( dds_header ), 1, dds );
	fwrite( bc4.ptr, sizeof( bc4[ 0 ] ), bc4.n, dds );
	fclose( dds );

	return 0;
}
