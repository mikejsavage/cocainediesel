#pragma once

#include "qcommon/types.h"
#include "qcommon/hashtable.h"

template< typename T >
using HashMapGetKey = u64 ( * )( const T & x );

template< typename T, size_t N, HashMapGetKey< T > GetKey = HashMapGetKey< T >( NULL ) >
class HashMap {
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
		if( slot == NULL )
			return false;
		*slot = x;
		return true;
	}

	Optional< PoolHandle< T > > add_handle( u64 key ) {
		T * slot = add( key );
		if( slot == NULL )
			return NONE;
		return PoolHandle< T > { typename PoolHandleType< T >::T( slot - values ) };
	}

	T * get( u64 key ) {
		u64 idx;
		return hashtable.get( key, &idx ) ? &values[ idx ] : NULL;
	}

	Optional< PoolHandle< T > > get_handle( u64 key ) {
		u64 idx;
		if( !hashtable.get( key, &idx ) )
			return NONE;
		return PoolHandle< T > { typename PoolHandleType< T >::T( idx ) };
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

	void clear() { hashtable.clear(); }

	size_t size() const { return hashtable.size(); }
	bool full() const { return hashtable.size() == N; }

	Span< T > span() { return Span< T >( values, hashtable.size() ); }
	Span< const T > span() const { return Span< const T >( values, hashtable.size() ); }

	T & operator[]( PoolHandle< T > handle ) { return span()[ handle.x ]; }
	const T & operator[]( PoolHandle< T > handle ) const { return span()[ handle.x ]; }
};

template< typename T, size_t N >
class HashPool {
	HashMap< T, N > map;

	Optional< PoolHandle< T > > handle( T * slot ) const {
		if( slot == NULL )
			return NONE;
		return PoolHandle< T > { typename PoolHandleType< T >::T( slot - map.span().ptr ) };
	}

public:
	Optional< PoolHandle< T > > add( u64 key ) {
		return handle( map.add( key ) );
	}

	Optional< PoolHandle< T > > add( u64 key, const T & x ) {
		T * slot = map.add( key );
		if( slot == NULL )
			return NONE;
		*slot = x;
		return handle( slot );
	}

	Optional< PoolHandle< T > > get( u64 key ) {
		return handle( map.get( key ) );
	}

	Optional< PoolHandle< T > > upsert( Optional< PoolHandle< T > > old_handle, u64 key, const T & x ) {
		if( old_handle.exists ) {
			( *this )[ old_handle.value ] = x;
			return old_handle;
		}
		return add( key, x );
	}

	void clear() { map.clear(); }

	Span< T > span() { return map.span(); }
	Span< const T > span() const { return map.span(); }

	T & operator[]( PoolHandle< T > handle ) { return span()[ handle.x ]; }
	const T & operator[]( PoolHandle< T > handle ) const { return span()[ handle.x ]; }
};
