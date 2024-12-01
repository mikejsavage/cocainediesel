#pragma once

#include "qcommon/types.h"

template< typename T, typename PoolHandleType< T >::T N >
struct Pool {
	T data[ N ];
	typename PoolHandleType< T >::T n = 0;

	static constexpr typename PoolHandleType< T >::T capacity = N;

	PoolHandle< T > allocate() {
		assert( n < N );
		return { n++ };
	}

	PoolHandle< T > allocate( const T & x ) {
		assert( n < N );
		data[ n ] = x;
		return { n++ };
	}

	PoolHandle< T > upsert( Optional< PoolHandle< T > > old_handle, const T & x ) {
		if( !old_handle.exists ) {
			return allocate( x );
		}

		assert( old_handle.value.x < N );
		data[ old_handle.value.x ] = x;
		return old_handle.value;
	}

	void clear() { n = 0; }

	T & operator[]( PoolHandle< T > handle ) { return data[ handle.x ]; }
	const T & operator[]( PoolHandle< T > handle ) const { return data[ handle.x ]; }

	T * begin() { return data; }
	T * end() { return data + n; }
};
