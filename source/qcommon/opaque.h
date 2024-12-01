#pragma once

#include <stddef.h>
#include <string.h>

template< typename T > constexpr size_t OpaqueSize = 0;
template< typename T > constexpr size_t OpaqueAlignment = sizeof( void * );

template< typename T, size_t Size = OpaqueSize< T >, size_t Alignment = OpaqueAlignment< T > >
struct Opaque {
	STATIC_ASSERT( Size > 0 );
	alignas( Alignment ) char opaque[ Size ];

	Opaque() = default;

	Opaque( const T & x ) {
		memcpy( opaque, &x, sizeof( T ) );
	}

	T * unwrap() {
		static_assert( sizeof( T ) <= Size && alignof( T ) <= Alignment, "gg" );
		return ( T * ) opaque;
	}

	const T * unwrap() const {
		static_assert( sizeof( T ) <= Size && alignof( T ) <= Alignment, "gg" );
		return ( const T * ) opaque;
	}
};
