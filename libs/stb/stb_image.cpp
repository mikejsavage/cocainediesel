#include "qcommon/base.h"

static void * StbMalloc( size_t size ) {
	return ALLOC_SIZE( sys_allocator, size, 16 );
}

static void * StbRealloc( void * ptr, size_t current_size, size_t new_size ) {
	return REALLOC( sys_allocator, ptr, current_size, new_size, 16 );
}

static void StbFree( void * ptr ) {
	return FREE( sys_allocator, ptr );
}

#define STB_IMAGE_IMPLEMENTATION
#define STBI_MALLOC StbMalloc
#define STBI_REALLOC_SIZED StbRealloc
#define STBI_FREE StbFree
#include "stb/stb_image.h"
