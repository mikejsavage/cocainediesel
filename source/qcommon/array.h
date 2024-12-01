#pragma once

#include "qcommon/types.h"
#include "qcommon/asan.h"

template< typename T, size_t N >
class BoundedDynamicArray {
	T elems[ N ];
	size_t n;

public:
	BoundedDynamicArray() = default;

	// TODO would be nice if this worked
	// constexpr BoundedDynamicArray( std::initializer_list< T > init ) : elems( init ), n( init.size() ) { }
	BoundedDynamicArray( std::initializer_list< T > init ) {
		clear();
		for( const T & x : init ) {
			[[maybe_unused]] bool ok = add( x );
			Assert( ok );
		}
	}

	void clear() {
		n = 0;
	}

	T * add() {
		return n == N ? NULL : &elems[ n++ ];
	}

	[[nodiscard]] bool add( const T & x ) {
		T * slot = add();
		if( slot == NULL )
			return false;
		*slot = x;
		return true;
	}

	void must_add( const T & x ) {
		[[maybe_unused]] bool ok = add( x );
	}

	void remove_swap( T * x ) {
		n--;
		Swap2( x, &elems[ n ] );
	}

	void remove_swap( size_t idx ) {
		remove_swap( &elems[ idx ] );
	}

	T pop() {
		Assert( n > 0 );
		n--;
		return elems[ n ];
	}

	T & operator[]( size_t i ) {
		Assert( i < n );
		return elems[ i ];
	}

	const T & operator[]( size_t i ) const {
		Assert( i < n );
		return elems[ i ];
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

template< typename T, size_t N >
constexpr size_t ARRAY_COUNT( const BoundedDynamicArray< T, N > & arr ) {
	return N;
}

template< typename T, typename M, size_t N >
constexpr size_t ARRAY_COUNT( BoundedDynamicArray< M, N > ( T::* ) ) {
	return N;
}

template< typename T >
class NonRAIIDynamicArray {
	Allocator * a;
	size_t n;
	size_t capacity;
	T * elems;

public:
	NonRAIIDynamicArray() = default;

	NonRAIIDynamicArray( Allocator * a_, size_t initial_capacity = 0, SourceLocation src_loc = CurrentSourceLocation() ) {
		init( a_, initial_capacity, src_loc );
	}

	void init( Allocator * a_, size_t initial_capacity = 0, SourceLocation src_loc = CurrentSourceLocation() ) {
		a = a_;
		capacity = initial_capacity;
		elems = capacity == 0 ? NULL : AllocMany< T >( a, capacity, src_loc );
		clear();
	}

	void shutdown( SourceLocation src_loc = CurrentSourceLocation() ) {
		Free( a, elems, src_loc );
	}

	T * add( SourceLocation src_loc = CurrentSourceLocation() ) {
		size_t idx = extend( 1, src_loc );
		return &elems[ idx ];
	}

	size_t add( const T & x, SourceLocation src_loc = CurrentSourceLocation() ) {
		size_t idx = extend( 1, src_loc );
		elems[ idx ] = x;
		return idx;
	}

	void add_many( Span< const T > xs, SourceLocation src_loc = CurrentSourceLocation() ) {
		size_t base = extend( xs.n, src_loc );
		for( size_t i = 0; i < xs.n; i++ ) {
			elems[ base + i ] = xs[ i ];
		}
	}

	void clear() {
		resize( 0 );
	}

	void resize( size_t new_size, SourceLocation src_loc = CurrentSourceLocation() ) {
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

		elems = ReallocMany< T >( a, elems, capacity, new_capacity, src_loc );
		capacity = new_capacity;
		n = new_size;

		ASAN_UNPOISON_MEMORY_REGION( elems, n * sizeof( T ) );
		ASAN_POISON_MEMORY_REGION( elems + n, ( capacity - n ) * sizeof( T ) );
	}

	size_t extend( size_t by, SourceLocation src_loc = CurrentSourceLocation() ) {
		size_t old_size = n;
		resize( n + by, src_loc );
		return old_size;
	}

	T & operator[]( size_t i ) {
		Assert( i < n );
		return elems[ i ];
	}

	const T & operator[]( size_t i ) const {
		Assert( i < n );
		return elems[ i ];
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

	DynamicArray( Allocator * a_, size_t initial_capacity = 0, SourceLocation src_loc = CurrentSourceLocation() ) {
		init( a_, initial_capacity, src_loc );
	}

	~DynamicArray() {
		shutdown();
	}
};
