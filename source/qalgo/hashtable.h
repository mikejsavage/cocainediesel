#pragma once

#include <stdint.h>

#define STATIC_ASSERT( p ) static_assert( p, #p )
template< typename T >
constexpr bool IsPowerOf2( T x ) {
	return ( x & ( x - 1 ) ) == 0;
}

template< size_t N >
class Hashtable {
public:
	STATIC_ASSERT( IsPowerOf2( N ) );

	Hashtable() {
		clear();
	}

	bool add( uint64_t key, uint64_t value ) {
		key >>= 2;

		size_t i = find( key );
		if( i == N || entry_status( i ) == STATUS_USED )
			return false;

		entries[ i ].key = key | STATUS_USED;
		entries[ i ].value = value;

		return true;
	}

	bool get( uint64_t key, uint64_t * value ) const {
		key >>= 2;

		size_t i = find( key );
		if( i == N || entry_status( i ) != STATUS_USED )
			return false;

		*value = entries[ i ].value;
		return true;
	}

	bool remove( uint64_t key ) {
		key >>= 2;

		size_t i = find( key );
		if( i == N || entry_status( i ) != STATUS_USED )
			return false;

		entries[ i ].key = key | STATUS_REMOVED;
	}

	void clear() {
		for( Entry & e : entries )
			e.key = STATUS_EMPTY;
	}

private:
	struct Entry {
		// top 2 bits of key are used for status
		uint64_t key;
		uint64_t value;
	};

	static constexpr uint64_t STATUS_EMPTY = UINT64_C( 0 ) << UINT64_C( 62 );
	static constexpr uint64_t STATUS_REMOVED = UINT64_C( 1 ) << UINT64_C( 62 );
	static constexpr uint64_t STATUS_USED = UINT64_C( 2 ) << UINT64_C( 62 );
	static constexpr uint64_t STATUS_MASK = UINT64_C( 3 ) << UINT64_C( 62 );

	Entry entries[ N ];

	uint64_t entry_key( size_t i ) const {
		return entries[ i ].key & ~STATUS_MASK;
	}

	uint64_t entry_status( size_t i ) const {
		return entries[ i ].key & STATUS_MASK;
	}

	size_t find( uint64_t key ) const {
		size_t i = key % N;
		size_t step = 1;
		while( step <= N ) {
			uint64_t status = entry_status( i );
			if( status == STATUS_EMPTY )
				return i;

			if( entry_key( i ) == key )
				return i;

			i = ( i + step ) % N;
			step++;
		}
		return N;
	}
};
