#pragma once

#include "qcommon/types.h"
#include "qcommon/hashtable.h"

template< typename T, size_t N >
class Hashmap {
	Hashtable< N * 2 > ht;
	u64 keys[ N ];

public:
	T values[ N ];
	size_t n;

	Hashmap() {
		n = 0;
	}

	T * add( u64 key ) {
		if( !ht.add( key, n ) )
			return NULL;

		keys[ n ] = key;
		n++;

		return &values[ n - 1 ];
	}

	T * get( u64 key ) {
		u64 idx;
		return ht.get( key, &idx ) ? &values[ idx ] : NULL;
	}

	bool remove( u64 key ) {
		u64 idx;
		if( !ht.get( key, &idx ) )
			return false;

		n--;
		Swap2( &keys[ idx ], &keys[ n ] );
		Swap2( &values[ idx ], &values[ n ] );

		ht.update( keys[ idx ], idx );
		ht.remove( key );

		return true;
	}
};
