#include "qcommon/types.h"

static void * StbMalloc( size_t size, void * userdata ) {
	return ( ( Allocator * ) userdata )->allocate( size, 16 );
}

static void StbFree( void * ptr, void * userdata ) {
	return ( ( Allocator * ) userdata )->deallocate( ptr );
}

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_malloc StbMalloc
#define STBTT_free StbFree
#include "stb_truetype.h"
