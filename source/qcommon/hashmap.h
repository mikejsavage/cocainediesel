#pragma once

#include "qcommon/types.h"
#include "qcommon/hashtable.h"

template< typename T, size_t N >
class Hashmap {
	Hashtable< N * 2 > ht;
	u64 keys[ N ];
	T values[ N ];

public:
	T * add( u64 key ) {
		if( !ht.add( key, ht.size() ) )
			return NULL;

		keys[ ht.size() - 1 ] = key;
		return &values[ ht.size() - 1 ];
	}

	T * get( u64 key ) {
		u64 idx;
		return ht.get( key, &idx ) ? &values[ idx ] : NULL;
	}

	bool remove( u64 key ) {
		u64 idx;
		if( !ht.get( key, &idx ) )
			return false;

		Swap2( &keys[ idx ], &keys[ ht.size() - 1 ] );
		Swap2( &values[ idx ], &values[ ht.size() - 1 ] );

		ht.update( keys[ idx ], idx );
		ht.remove( key );

		return true;
	}
};
