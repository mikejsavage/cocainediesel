#include "qcommon/base.h"
#include "serialization.h"

template< typename T >
static void SerializeFundamental( SerializationBuffer * buf, T & x ) {
	if( buf->error || size_t( buf->end - buf->cursor ) < sizeof( T ) ) {
		buf->error = true;
		if( !buf->serializing )
			memset( &x, 0, sizeof( T ) );
		return;
	}

	if( buf->serializing ) {
		memcpy( buf->cursor, &x, sizeof( T ) );
	}
	else {
		memcpy( &x, buf->cursor, sizeof( T ) );
	}

	buf->cursor += sizeof( T );
}

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

void Serialize( SerializationBuffer * buf, Mat2 & m ) { *buf & m.col0 & m.col1; }
void Serialize( SerializationBuffer * buf, Mat3 & m ) { *buf & m.col0 & m.col1 & m.col2; }
void Serialize( SerializationBuffer * buf, Mat4 & m ) { *buf & m.col0 & m.col1 & m.col2 & m.col3; }

void Serialize( SerializationBuffer * buf, Quaternion & q ) { *buf & q.x & q.y & q.z & q.w; }

void Serialize( SerializationBuffer * buf, MinMax1 & b ) { *buf & b.lo & b.hi; }
void Serialize( SerializationBuffer * buf, MinMax2 & b ) { *buf & b.mins & b.maxs; }
void Serialize( SerializationBuffer * buf, MinMax3 & b ) { *buf & b.mins & b.maxs; }
