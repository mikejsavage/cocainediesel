#pragma once

#include "qcommon/types.h"

enum SerializationMode {
	SerializationMode_Serializing,
	SerializationMode_Deserializing,
};

struct SerializationBuffer {
	char * cursor;
	char * end;
	bool serializing;
	bool error;

	SerializationBuffer( SerializationMode mode, char * buf, size_t buf_size ) {
		cursor = buf;
		end = buf + buf_size;
		serializing = mode == SerializationMode_Serializing;
		error = false;
	}
};

template< typename T >
bool Serialize( const T & x, char * buf, size_t buf_size ) {
	SerializationBuffer sb( SerializationMode_Serializing, buf, buf_size );
	Serialize( &sb, const_cast< T & >( x ) );
	return !sb.error;
}

template< typename T >
bool Deserialize( T & x, const char * buf, size_t buf_size ) {
	SerializationBuffer sb( SerializationMode_Deserializing, const_cast< char * >( buf ), buf_size );
	Serialize( &sb, x );
	return !sb.error;
}

void Serialize( SerializationBuffer * buf, s8 & x );
void Serialize( SerializationBuffer * buf, s16 & x );
void Serialize( SerializationBuffer * buf, s32 & x );
void Serialize( SerializationBuffer * buf, s64 & x );
void Serialize( SerializationBuffer * buf, u8 & x );
void Serialize( SerializationBuffer * buf, u16 & x );
void Serialize( SerializationBuffer * buf, u32 & x );
void Serialize( SerializationBuffer * buf, u64 & x );
void Serialize( SerializationBuffer * buf, float & x );
void Serialize( SerializationBuffer * buf, double & x );
void Serialize( SerializationBuffer * buf, bool & b );
void Serialize( SerializationBuffer * buf, Vec2 & v );
void Serialize( SerializationBuffer * buf, Vec3 & v );
void Serialize( SerializationBuffer * buf, Vec4 & v );

void Serialize( SerializationBuffer * buf, Mat2 & m );
void Serialize( SerializationBuffer * buf, Mat3 & m );
void Serialize( SerializationBuffer * buf, Mat4 & m );
void Serialize( SerializationBuffer * buf, Quaternion & q );
void Serialize( SerializationBuffer * buf, MinMax1 & b );
void Serialize( SerializationBuffer * buf, MinMax2 & b );

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
