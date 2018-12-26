#pragma once

#include <assert.h>
#include <stddef.h>

template< typename T >
class Span {
	T * elems;
	size_t n;

public:
	Span() {
		n = 0;
	}

	Span( T * memory, size_t count ) {
		assert( count == 0 || memory != NULL );
		n = count;
		elems = memory;
	}

	T & operator[]( size_t i ) {
		assert( i < n );
		return elems[ i ];
	}

	const T & operator[]( size_t i ) const {
		assert( i < n );
		return elems[ i ];
	}

	Span< T > operator+( size_t i ) {
		assert( i <= n );
		return Span< T >( elems + i, n - i );
	}

	const Span< T > operator+( size_t i ) const {
		assert( i <= n );
		return Span< T >( elems + i, n - i );
	}

	bool in_range( size_t i ) const {
		return i < n;
	}

	T * ptr() {
		return elems;
	}

	const T * ptr() const {
		return elems;
	}

	size_t num_bytes() const {
		return sizeof( T ) * n;
	}

	T * begin() {
		return elems;
	}

	T * end() {
		return elems + n;
	}

	const T * cbegin() const {
		return elems;
	}

	const T * cend() const {
		return elems + n;
	}

	Span< T > slice( size_t start, size_t one_past_end ) {
		assert( start <= one_past_end );
		assert( one_past_end <= n );
		return Span< T >( elems + start, one_past_end - start );
	}

	const Span< T > slice( size_t start, size_t one_past_end ) const {
		assert( start <= one_past_end );
		assert( one_past_end <= n );
		return Span< T >( elems + start, one_past_end - start );
	}

	template< typename S >
	Span< S > cast() {
		assert( num_bytes() % sizeof( S ) == 0 );
		return Span< S >( ( S * ) ptr(), num_bytes() / sizeof( S ) );
	}

	template< typename S >
	const Span< S > cast() const {
		assert( num_bytes() % sizeof( S ) == 0 );
		return Span< S >( ( S * ) ptr(), num_bytes() / sizeof( S ) );
	}
};
