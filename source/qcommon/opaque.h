#pragma once

#include "qcommon/types.h"

template< typename T > constexpr size_t OpaqueSize = 0;
template< typename T > constexpr size_t OpaqueAlignment = sizeof( void * );

template< typename T, size_t Size = OpaqueSize< T >, size_t Alignment = OpaqueAlignment< T > >
struct Opaque {
	STATIC_ASSERT( Size > 0 );
	alignas( Alignment ) char opaque[ Size ];

	Opaque() = default;

	Opaque( const T & x ) {
		*unwrap() = x;
	}

	T * unwrap() {
		STATIC_ASSERT( sizeof( T ) <= Size && alignof( T ) <= Alignment );
		return ( T * ) opaque;
	}

	const T * unwrap() const {
		STATIC_ASSERT( sizeof( T ) <= Size && alignof( T ) <= Alignment );
		return ( const T * ) opaque;
	}
};
