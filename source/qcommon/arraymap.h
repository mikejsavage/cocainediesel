#pragma once

#include "qcommon/types.h"

template< typename K, typename V, size_t N >
class ArrayMap {
	struct Slot {
		K key;
		V value;
	};

	Slot slots[ N ];
	size_t n = 0;

public:
	V * add( const K & key ) {
		if( n == N || get2( key ).exists )
			return NULL;
		slots[ n ].key = key;
		return &slots[ n++ ].value;
	}

	[[nodiscard]] bool add( const K & key, const V & value ) {
		V * v = add( key );
		if( v == NULL )
			return false;
		*v = value;
		return true;
	}

	void must_add( const K & key, const V & value ) {
		if( !add( key, value ) ) {
			abort();
		}
	}

	const V * get( const K & key ) const {
		for( size_t i = 0; i < n; i++ ) {
			if( slots[ i ].key == key ) {
				return &slots[ i ].value;
			}
		}

		return NULL;
	}

	Optional< V > get2( const K & key ) const {
		for( size_t i = 0; i < n; i++ ) {
			if( slots[ i ].key == key ) {
				return slots[ i ].value;
			}
		}

		return NONE;
	}

	const Slot * begin() const { return slots; }
	const Slot * end() const { return slots + n; }
	size_t size() const { return n; }
};
