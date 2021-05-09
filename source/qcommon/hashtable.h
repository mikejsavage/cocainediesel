#pragma once

#include "qcommon/types.h"

template< size_t N >
class Hashtable {
	STATIC_ASSERT( IsPowerOf2( N ) );

	static constexpr u64 DeletedBit = U64( 1 ) << U64( 63 );
	static constexpr u64 EmptyKey = 0;

	struct Entry {
		u64 key;
		u64 value;
	};

	Entry entries[ N ];
	size_t n;

public:
	Hashtable() {
		clear();
	}

	bool add( u64 key, u64 value ) {
		assert( key != EmptyKey );

		if( n == N )
			return false;

		key = hash_key( key );

		u64 i = key % N;
		u64 dist = 0;

		while( true ) {
			if( entries[ i ].key == key )
				return false;

			if( entries[ i ].key == EmptyKey ) {
				entries[ i ].key = key;
				entries[ i ].value = value;
				n++;
				return true;
			}

			u64 existing_dist = probe_distance( hash_key( entries[ i ].key ), i );
			if( existing_dist < dist ) {
				if( is_deleted( entries[ i ].key ) ) {
					entries[ i ].key = key;
					entries[ i ].value = value;
					n++;
					return true;
				}

				Swap2( &key, &entries[ i ].key );
				Swap2( &value, &entries[ i ].value );
				dist = existing_dist;
			}

			i = ( i + 1 ) % N;
			dist++;
		}

		return false;
	}

	bool update( u64 key, u64 value ) {
		u64 i;
		if( !find( hash_key( key ), &i ) )
			return false;

		entries[ i ].value = value;
		return true;
	}

	bool get( u64 key, u64 * value ) const {
		u64 i;
		if( !find( hash_key( key ), &i ) )
			return false;

		*value = entries[ i ].value;
		return true;
	}

	bool remove( u64 key ) {
		u64 i;
		if( !find( key, &i ) )
			return false;

		entries[ i ].key |= DeletedBit;
		n--;
		return true;
	}

	size_t size() const {
		return n;
	}

	void clear() {
		for( Entry & e : entries ) {
			e.key = EmptyKey;
			e.value = 0;
		}
		n = 0;
	}

private:
	static u64 probe_distance( u64 hash, u64 pos ) {
		return ( pos - hash ) % N;
	}

	static u64 is_deleted( u64 key ) {
		return ( key & DeletedBit ) != 0;
	}

	u64 hash_key( u64 key ) const {
		return key & ~DeletedBit;
	}

	bool find( u64 key, u64 * idx ) const {
		if( key == EmptyKey )
			return false;

		key = hash_key( key );

		u64 i = key % N;
		u64 dist = 0;

		while( true ) {
			if( entries[ i ].key == key ) {
				*idx = i;
				return true;
			}

			if( entries[ i ].key == EmptyKey )
				break;

			if( probe_distance( hash_key( entries[ i ].key ), i ) < dist )
				break;

			i = ( i + 1 ) % N;
			dist++;
		}

		return false;
	}
};
