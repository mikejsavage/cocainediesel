#pragma once

#include "qcommon/types.h"
#include "qcommon/asan.h"

template< typename T >
class NonRAIIDynamicArray {
	Allocator * a;
	size_t n;
	size_t capacity;
	T * elems;

public:
	void init( Allocator * a_, size_t initial_capacity = 0 ) {
		a = a_;
		capacity = initial_capacity;
		elems = capacity == 0 ? NULL : ALLOC_MANY( a, T, capacity );
		clear();
	}

	void shutdown() {
		FREE( a, elems );
	}

	T * add() {
		size_t idx = extend( 1 );
		return &elems[ idx ];
	}

	size_t add( const T & x ) {
		size_t idx = extend( 1 );
		elems[ idx ] = x;
		return idx;
	}

	void add_many( Span< const T > xs ) {
		size_t base = extend( xs.n );
		for( size_t i = 0; i < xs.n; i++ ) {
			elems[ base + i ] = xs[ i ];
		}
	}

	void clear() {
		resize( 0 );
	}

	void resize( size_t new_size ) {
		if( new_size <= n ) {
			n = new_size;
			ASAN_POISON_MEMORY_REGION( elems + n, ( capacity - n ) * sizeof( T ) );
			return;
		}

		if( new_size <= capacity ) {
			n = new_size;
			ASAN_UNPOISON_MEMORY_REGION( elems, n * sizeof( T ) );
			return;
		}

		size_t new_capacity = Max2( size_t( 64 ), capacity );
		while( new_capacity < new_size )
			new_capacity *= 2;

		elems = REALLOC_MANY( a, T, elems, capacity, new_capacity );
		capacity = new_capacity;
		n = new_size;

		ASAN_UNPOISON_MEMORY_REGION( elems, n * sizeof( T ) );
		ASAN_POISON_MEMORY_REGION( elems + n, ( capacity - n ) * sizeof( T ) );
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

template< typename T >
class DynamicArray : public NonRAIIDynamicArray< T > {
	// weird c++ syntax that lets you change visibility of inherited members
	using NonRAIIDynamicArray< T >::init;
	using NonRAIIDynamicArray< T >::shutdown;

public:
	NONCOPYABLE( DynamicArray );

	DynamicArray( Allocator * a_, size_t initial_capacity = 0 ) {
		init( a_, initial_capacity );
	}

	~DynamicArray() {
		shutdown();
	}
};
