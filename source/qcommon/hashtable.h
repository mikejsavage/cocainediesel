#pragma once

#include "qcommon/base.h"

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

	bool add( u64 key, u64 value ) {
		key >>= 2;

		size_t i = find( key );
		if( i == N || entry_status( i ) == STATUS_USED )
			return false;

		entries[ i ].key = key | STATUS_USED;
		entries[ i ].value = value;

		return true;
	}

	bool get( u64 key, u64 * value ) const {
		key >>= 2;

		size_t i = find( key );
		if( i == N || entry_status( i ) != STATUS_USED )
			return false;

		*value = entries[ i ].value;
		return true;
	}

	bool remove( u64 key ) {
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
		u64 key;
		u64 value;
	};

	static constexpr u64 STATUS_EMPTY = U64( 0 ) << U64( 62 );
	static constexpr u64 STATUS_REMOVED = U64( 1 ) << U64( 62 );
	static constexpr u64 STATUS_USED = U64( 2 ) << U64( 62 );
	static constexpr u64 STATUS_MASK = U64( 3 ) << U64( 62 );

	Entry entries[ N ];

	u64 entry_key( size_t i ) const {
		return entries[ i ].key & ~STATUS_MASK;
	}

	u64 entry_status( size_t i ) const {
		return entries[ i ].key & STATUS_MASK;
	}

	size_t find( u64 key ) const {
		size_t i = key % N;
		size_t step = 1;
		while( step <= N ) {
			u64 status = entry_status( i );
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
