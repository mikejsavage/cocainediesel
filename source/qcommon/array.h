#pragma once

#include "qcommon/types.h"

template< typename T >
class DynamicArray {
	Allocator * a;
	size_t n;
	size_t capacity;
	T * elems;
	bool auto_destruct;

public:
	NONCOPYABLE( DynamicArray );

	DynamicArray( NoInit ) {
		auto_destruct = false;
	}

	DynamicArray( Allocator * a_, size_t initial_capacity = 0 ) {
		init( a_, initial_capacity );
		auto_destruct = true;
	}

	~DynamicArray() {
		if( auto_destruct ) {
			shutdown();
		}
	}

	void init( Allocator * a_, size_t initial_capacity = 0 ) {
		a = a_;
		n = 0;
		capacity = initial_capacity;
		elems = capacity == 0 ? NULL : ALLOC_MANY( a, T, capacity );
	}

	void shutdown() {
		FREE( a, elems );
	}

	size_t add( const T & x ) {
		size_t idx = extend( 1 );
		elems[ idx ] = x;
		return idx;
	}

	void clear() {
		n = 0;
	}

	void resize( size_t new_size ) {
		if( new_size < n ) {
			n = new_size;
			return;
		}

		if( new_size <= capacity ) {
			n = new_size;
			return;
		}

		size_t new_capacity = Max2( size_t( 64 ), capacity );
		while( new_capacity < new_size )
			new_capacity *= 2;

		elems = REALLOC_MANY( a, T, elems, capacity, new_capacity );
		capacity = new_capacity;
		n = new_size;
	}

	size_t extend( size_t by ) {
		size_t old_size = n;
		resize( n + by );
		return old_size;
	}

	T & operator[]( size_t i ) {
		assert( i < n );
		return elems[ i ];
	}

	const T & operator[]( size_t i ) const {
		assert( i < n );
		return elems[ i ];
	}

	T & top() {
		assert( n > 0 );
		return elems[ n - 1 ];
	}

	const T & top() const {
		assert( n > 0 );
		return elems[ n - 1 ];
	}

	T * ptr() { return elems; }
	const T * ptr() const { return elems; }
	size_t size() const { return n; }
	size_t num_bytes() const { return sizeof( T ) * n; }

	T * begin() { return elems; }
	T * end() { return elems + n; }
	const T * begin() const { return elems; }
	const T * end() const { return elems + n; }

	Span< T > span() { return Span< T >( elems, n ); }
	Span< const T > span() const { return Span< const T >( elems, n ); }
};
