#pragma once

#include "qcommon/types.h"

template< typename T > class DynamicArray;

struct SerializationBuffer {
	bool serializing;
	bool error;

	Allocator * a;
	const char * input_cursor;
	const char * input_end;

	DynamicArray< u8 > * output_buf;
};

SerializationBuffer NewDeserializationBuffer( Allocator * a, const void * buf, size_t buf_size );
SerializationBuffer NewSerializationBuffer( DynamicArray< u8 > * buf );

template< typename T >
bool Deserialize( Allocator * a, T * x, const void * buf, size_t buf_size ) {
	SerializationBuffer sb = NewDeserializationBuffer( a, buf, buf_size );
	Serialize( &sb, *x );
	return !sb.error && sb.input_cursor == sb.input_end;
}

template< typename T >
void Serialize( const T & x, DynamicArray< u8 > * buf ) {
	SerializationBuffer sb = NewSerializationBuffer( buf );
	Serialize( &sb, const_cast< T & >( x ) );
}

void Serialize( SerializationBuffer * buf, char & x );
void Serialize( SerializationBuffer * buf, signed char & x );
void Serialize( SerializationBuffer * buf, short & x );
void Serialize( SerializationBuffer * buf, int & x );
void Serialize( SerializationBuffer * buf, long & x );
void Serialize( SerializationBuffer * buf, long long & x );
void Serialize( SerializationBuffer * buf, unsigned char & x );
void Serialize( SerializationBuffer * buf, unsigned short & x );
void Serialize( SerializationBuffer * buf, unsigned int & x );
void Serialize( SerializationBuffer * buf, unsigned long & x );
void Serialize( SerializationBuffer * buf, unsigned long long & x );
void Serialize( SerializationBuffer * buf, float & x );
void Serialize( SerializationBuffer * buf, double & x );
void Serialize( SerializationBuffer * buf, bool & b );
void Serialize( SerializationBuffer * buf, Vec2 & v );
void Serialize( SerializationBuffer * buf, Vec3 & v );
void Serialize( SerializationBuffer * buf, Vec4 & v );

void Serialize( SerializationBuffer * buf, Mat3 & m );
void Serialize( SerializationBuffer * buf, Mat4 & m );
void Serialize( SerializationBuffer * buf, Quaternion & q );
void Serialize( SerializationBuffer * buf, MinMax1 & b );
void Serialize( SerializationBuffer * buf, MinMax2 & b );
void Serialize( SerializationBuffer * buf, MinMax3 & b );

template< typename T >
void Serialize( SerializationBuffer * buf, Span< T > & v ) {
	Serialize( buf, v.n );

	if( !buf->serializing ) {
		v = AllocSpan< T >( buf->a, v.n );
	}

	for( size_t i = 0; i < v.n; i++ ) {
		Serialize( buf, v[ i ] );
	}
}

template< typename T, size_t N >
void Serialize( SerializationBuffer * buf, T ( &arr )[ N ] ) {
	for( size_t i = 0; i < N; i++ ) {
		Serialize( buf, arr[ i ] );
	}
}

template< typename T >
SerializationBuffer & operator&( SerializationBuffer & buf, T & v ) {
	Serialize( &buf, v );
	return buf;
}
