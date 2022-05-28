#include "qcommon/base.h"
#include "qcommon/array.h"
#include "qcommon/serialization.h"

SerializationBuffer NewDeserializationBuffer( Allocator * a, const void * buf, size_t buf_size ) {
	SerializationBuffer sb = { };
	sb.serializing = false;
	sb.a = a;
	sb.input_cursor = ( const char * ) buf;
	sb.input_end = sb.input_cursor + buf_size;
	return sb;
}

SerializationBuffer NewSerializationBuffer( DynamicArray< u8 > * buf ) {
	SerializationBuffer sb = { };
	sb.serializing = true;
	sb.output_buf = buf;
	return sb;
}

template< typename T >
static void SerializeFundamental( SerializationBuffer * buf, T & x ) {
	if( buf->error ) {
		return;
	}

	if( buf->serializing ) {
		size_t old_size = buf->output_buf->extend( sizeof( T ) );
		memcpy( buf->output_buf->ptr() + old_size, &x, sizeof( T ) );
	}
	else {
		if( buf->input_cursor < buf->input_end && size_t( buf->input_end - buf->input_cursor ) >= sizeof( T ) ) {
			memcpy( &x, buf->input_cursor, sizeof( T ) );
		}
		else {
			buf->error = true;
			x = { };
		}

		buf->input_cursor += sizeof( T );
	}
}

void Serialize( SerializationBuffer * buf, char & x ) { SerializeFundamental( buf, x ); }
void Serialize( SerializationBuffer * buf, s8 & x ) { SerializeFundamental( buf, x ); }
void Serialize( SerializationBuffer * buf, s16 & x ) { SerializeFundamental( buf, x ); }
void Serialize( SerializationBuffer * buf, s32 & x ) { SerializeFundamental( buf, x ); }
void Serialize( SerializationBuffer * buf, s64 & x ) { SerializeFundamental( buf, x ); }
void Serialize( SerializationBuffer * buf, u8 & x ) { SerializeFundamental( buf, x ); }
void Serialize( SerializationBuffer * buf, u16 & x ) { SerializeFundamental( buf, x ); }
void Serialize( SerializationBuffer * buf, u32 & x ) { SerializeFundamental( buf, x ); }
void Serialize( SerializationBuffer * buf, u64 & x ) { SerializeFundamental( buf, x ); }
void Serialize( SerializationBuffer * buf, float & x ) { SerializeFundamental( buf, x ); }
void Serialize( SerializationBuffer * buf, double & x ) { SerializeFundamental( buf, x ); }

void Serialize( SerializationBuffer * buf, bool & b ) {
	u8 x = b ? 1 : 0;
	SerializeFundamental( buf, x );
	b = x == 1;
}

void Serialize( SerializationBuffer * buf, Vec2 & v ) { *buf & v.x & v.y; }
void Serialize( SerializationBuffer * buf, Vec3 & v ) { *buf & v.x & v.y & v.z; }
void Serialize( SerializationBuffer * buf, Vec4 & v ) { *buf & v.x & v.y & v.z & v.w; }

void Serialize( SerializationBuffer * buf, Mat3 & m ) { *buf & m.col0 & m.col1 & m.col2; }
void Serialize( SerializationBuffer * buf, Mat4 & m ) { *buf & m.col0 & m.col1 & m.col2 & m.col3; }

void Serialize( SerializationBuffer * buf, Quaternion & q ) { *buf & q.x & q.y & q.z & q.w; }

void Serialize( SerializationBuffer * buf, MinMax1 & b ) { *buf & b.lo & b.hi; }
void Serialize( SerializationBuffer * buf, MinMax2 & b ) { *buf & b.mins & b.maxs; }
void Serialize( SerializationBuffer * buf, MinMax3 & b ) { *buf & b.mins & b.maxs; }
