#pragma once

#include <stddef.h>
#include <string.h>

template< typename T > constexpr size_t OpaqueSize = 0;
template< typename T > constexpr size_t OpaqueAlignment = alignof( void * );
template< typename T > constexpr bool OpaqueCopyable = true;

template< typename T >
struct Opaque {
	alignas( OpaqueAlignment< T > ) char opaque[ OpaqueSize< T > ];
	static_assert( sizeof( opaque ) > 0, "You need to define OpaqueSize< T >" );

	Opaque() = default;

	Opaque( const T & x ) requires( OpaqueCopyable< T > ) {
		memcpy( opaque, &x, sizeof( T ) );
	}

	Opaque( const Opaque< T > & other ) requires( OpaqueCopyable< T > ) {
		memcpy( opaque, other.opaque, sizeof( opaque ) );
	}

	void operator=( const Opaque< T > & other ) requires( OpaqueCopyable< T > ) {
		memcpy( opaque, other.opaque, sizeof( opaque ) );
	}

	Opaque( const Opaque< T > & ) requires( !OpaqueCopyable< T > ) = delete;
	void operator=( const Opaque< T > & ) requires( !OpaqueCopyable< T > ) = delete;

	T * unwrap() {
		static_assert( sizeof( T ) <= sizeof( opaque ) && alignof( T ) <= OpaqueAlignment< T >, "gg" );
		return ( T * ) opaque;
	}

	const T * unwrap() const {
		static_assert( sizeof( T ) <= sizeof( opaque ) && alignof( T ) <= OpaqueAlignment< T >, "gg" );
		return ( const T * ) opaque;
	}

	T * operator->() { return unwrap(); };
	const T * operator->() const { return unwrap(); };
};
