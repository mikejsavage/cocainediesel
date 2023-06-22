#include "qcommon/base.h"

static void * StbMalloc( size_t size ) {
	return sys_allocator->allocate( size, 16 );
}

static void * StbRealloc( void * ptr, size_t current_size, size_t new_size ) {
	return sys_allocator->reallocate( ptr, current_size, new_size, 16 );
}

static void StbFree( void * ptr ) {
	return sys_allocator->deallocate( ptr );
}

#define STB_IMAGE_IMPLEMENTATION
#define STBI_MALLOC StbMalloc
#define STBI_REALLOC_SIZED StbRealloc
#define STBI_FREE StbFree
#include "stb_image.h"
