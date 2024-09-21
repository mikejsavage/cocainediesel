#pragma once

#include "qcommon/types.h"
#include "qcommon/hashtable.h"

template< typename T >
using HashmapGetKey = u64 ( * )( const T & x );

template< typename T, size_t N, HashmapGetKey< T > GetKey = HashmapGetKey< T >( NULL ) >
class Hashmap {
	Hashtable< N * 2 > hashtable;
	T values[ N ];

public:
	T * add( u64 key ) {
		if( full() )
			return NULL;
		[[maybe_unused]] bool ok = hashtable.add( key, hashtable.size() );
		Assert( ok );
		return &values[ hashtable.size() - 1 ];
	}

	bool add( u64 key, const T & x ) {
		T * slot = add( key );
		if( !slot )
			return false;
		*slot = x;
		return true;
	}

	T * get( u64 key ) {
		u64 idx;
		return hashtable.get( key, &idx ) ? &values[ idx ] : NULL;
	}

	bool upsert( u64 key, const T & x ) {
		T * slot = get( key );
		if( slot == NULL )
			slot = add( key );
		if( slot == NULL )
			return false;
		*slot = x;
		return true;
	}

	bool remove( u64 key ) requires( GetKey != NULL ) {
		u64 idx;
		if( !hashtable.get( key, &idx ) )
			return false;

		Swap2( &values[ idx ], &values[ hashtable.size() - 1 ] );

		[[maybe_unused]] bool ok1 = hashtable.update( GetKey( values[ idx ] ), idx );
		[[maybe_unused]] bool ok2 = hashtable.remove( key );

		Assert( ok1 );
		Assert( ok2 );

		return true;
	}

	size_t size() const { return hashtable.size(); }
	bool full() const { return size() == N; }
	T & operator[]( size_t idx ) { return values[ idx ]; }

	void clear() { hashtable.clear(); }
};
