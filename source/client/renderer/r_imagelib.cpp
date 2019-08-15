#include "r_local.h"
#include "r_imagelib.h"

#include "stb/stb_image.h"
#include "stb/stb_image_write.h"

r_imginfo_t IMG_LoadImage( const char * filename ) {
	MICROPROFILE_SCOPEI( "Assets", "IMG_LoadImage", 0xffffffff );

	r_imginfo_t ret;
	memset( &ret, 0, sizeof( ret ) );

	uint8_t * data;
	size_t size = R_LoadFile( filename, ( void ** ) &data );
	if( data == NULL )
		return ret;

	ret.pixels = stbi_load_from_memory( data, size, &ret.width, &ret.height, &ret.samples, 0 );

	R_FreeFile( data );

	return ret;
}

bool WritePNG( const char * filename, r_imginfo_t * img ) {
	FS_CreateAbsolutePath( filename );
	return stbi_write_png( filename, img->width, img->height, img->samples, img->pixels, 0 ) != 0;
}
