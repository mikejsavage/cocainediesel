/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "qcommon/qcommon.h"
#include "qcommon/half_float.h"
#include "qcommon/serialization.h"

#define MAX_MSG_STRING_CHARS    2048

void MSG_Init( msg_t *msg, uint8_t *data, size_t length ) {
	memset( msg, 0, sizeof( *msg ) );
	msg->data = data;
	msg->maxsize = length;
	msg->cursize = 0;
	msg->compressed = false;
}

void MSG_Clear( msg_t *msg ) {
	msg->cursize = 0;
	msg->compressed = false;
}

void *MSG_GetSpace( msg_t *msg, size_t length ) {
	void *ptr;

	assert( msg->cursize + length <= msg->maxsize );
	if( msg->cursize + length > msg->maxsize ) {
		Com_Error( ERR_FATAL, "MSG_GetSpace: overflowed" );
	}

	ptr = msg->data + msg->cursize;
	msg->cursize += length;
	return ptr;
}

struct DeltaBuffer {
	static constexpr u32 MAX_FIELDS = 1024;

	u8 * buf;
	u8 * cursor;
	u8 * end;

	u8 field_mask[ MAX_FIELDS / 8 ];
	u32 num_fields;
	u32 field_mask_read_cursor;

	bool serializing;
	bool error;
};

static void MSG_WriteDeltaBuffer( msg_t * msg, const DeltaBuffer & delta ) {
	MSG_WriteUintBase128( msg, delta.num_fields );
	u8 bytes = ( delta.num_fields + 7 ) / 8;
	MSG_WriteData( msg, delta.field_mask, bytes );
	MSG_WriteData( msg, delta.buf, delta.cursor - delta.buf );
}

static DeltaBuffer MSG_StartReadingDeltaBuffer( msg_t * msg ) {
	DeltaBuffer delta = { };

	delta.num_fields = MSG_ReadUintBase128( msg );
	u8 bytes = ( delta.num_fields + 7 ) / 8;
	MSG_ReadData( msg, delta.field_mask, bytes );

	delta.buf = msg->data + msg->readcount;
	delta.cursor = msg->data + msg->readcount;
	delta.end = msg->data + msg->cursize;

	return delta;
}

static void MSG_FinishReadingDeltaBuffer( msg_t * msg, const DeltaBuffer & delta ) {
	msg->readcount += delta.cursor - delta.buf;
}

static DeltaBuffer DeltaWriter( u8 * buf, size_t n ) {
	DeltaBuffer delta = { };
	delta.buf = buf;
	delta.cursor = buf;
	delta.end = delta.buf + n;
	delta.serializing = true;

	return delta;
}

static void AddBit( DeltaBuffer * buf, bool b ) {
	if( buf->error || buf->num_fields == DeltaBuffer::MAX_FIELDS ) {
		buf->error = true;
		return;
	}

	if( b ) {
		u32 byte = buf->num_fields / 8;
		u32 bit = buf->num_fields % 8;
		buf->field_mask[ byte ] |= u8( 1 ) << u8( bit );
	}

	buf->num_fields++;
}

static bool GetBit( DeltaBuffer * buf ) {
	if( buf->error || buf->field_mask_read_cursor == buf->num_fields ) {
		buf->error = true;
		return false;
	}

	u32 byte = buf->field_mask_read_cursor / 8;
	u32 bit = buf->field_mask_read_cursor % 8;
	bool b = ( buf->field_mask[ byte ] & ( u8( 1 ) << u8( bit ) ) ) != 0;
	buf->field_mask_read_cursor++;

	return b;
}

static void AddBytes( DeltaBuffer * buf, const void * data, size_t n ) {
	if( buf->error || size_t( buf->end - buf->cursor ) < n ) {
		buf->error = true;
		return;
	}

	memcpy( buf->cursor, data, n );
	buf->cursor += n;
}

static void GetBytes( DeltaBuffer * buf, void * data, size_t n ) {
	if( buf->error || size_t( buf->end - buf->cursor ) < n ) {
		buf->error = true;
		memset( data, 0, n );
		return;
	}

	memcpy( data, buf->cursor, n );
	buf->cursor += n;
}

static void AddByte( DeltaBuffer * buf, u8 byte ) {
	AddBytes( buf, &byte, 1 );
}

static u8 GetByte( DeltaBuffer * buf ) {
	u8 b;
	GetBytes( buf, &b, 1 );
	return b;
}

template< typename T >
static void DeltaFundamental( DeltaBuffer * buf, T & x, const T & baseline ) {
	if( buf->serializing ) {
		AddBit( buf, x != baseline );
		if( x != baseline ) {
			AddBytes( buf, &x, sizeof( x ) );
		}
	}
	else {
		if( GetBit( buf ) ) {
			GetBytes( buf, &x, sizeof( x ) );
		}
		else {
			x = baseline;
		}
	}
}

static void Delta( DeltaBuffer * buf, s8 & x, s8 baseline ) { DeltaFundamental( buf, x, baseline ); }
static void Delta( DeltaBuffer * buf, s16 & x, s16 baseline ) { DeltaFundamental( buf, x, baseline ); }
static void Delta( DeltaBuffer * buf, s32 & x, s32 baseline ) { DeltaFundamental( buf, x, baseline ); }
static void Delta( DeltaBuffer * buf, s64 & x, s64 baseline ) { DeltaFundamental( buf, x, baseline ); }
static void Delta( DeltaBuffer * buf, u8 & x, u8 baseline ) { DeltaFundamental( buf, x, baseline ); }
static void Delta( DeltaBuffer * buf, u16 & x, u16 baseline ) { DeltaFundamental( buf, x, baseline ); }
static void Delta( DeltaBuffer * buf, u32 & x, u32 baseline ) { DeltaFundamental( buf, x, baseline ); }
static void Delta( DeltaBuffer * buf, u64 & x, u64 baseline ) { DeltaFundamental( buf, x, baseline ); }
static void Delta( DeltaBuffer * buf, float & x, float baseline ) { DeltaFundamental( buf, x, baseline ); }
static void Delta( DeltaBuffer * buf, double & x, double baseline ) { DeltaFundamental( buf, x, baseline ); }

static void Delta( DeltaBuffer * buf, bool & b, bool baseline ) {
	if( buf->serializing ) {
		AddBit( buf, b != baseline );
	}
	else {
		b = GetBit( buf ) ? !baseline : baseline;
	}
}

template< typename T, size_t N >
void Delta( DeltaBuffer * buf, T ( &arr )[ N ], const T ( &baseline )[ N ] ) {
	for( size_t i = 0; i < N; i++ ) {
		Delta( buf, arr[ i ], baseline[ i ] );
	}
}

static void Delta( DeltaBuffer * buf, RGBA8 & rgba, const RGBA8 & baseline ) {
	Delta( buf, rgba.r, baseline.r );
	Delta( buf, rgba.g, baseline.b );
	Delta( buf, rgba.b, baseline.g );
	Delta( buf, rgba.a, baseline.a );
}

template< typename T >
static void DeltaUnsignedLEB128( DeltaBuffer * buf, T & x, const T & baseline ) {
	if( buf->serializing ) {
		if( x == baseline ) {
			AddBit( buf, false );
		}
		else {
			AddBit( buf, true );
			do {
				u8 curr = x & 127;
				x >>= 7;
				if( x != 0 ) {
					curr |= 1 << 7;
				}

				AddByte( buf, curr );
			} while( x != 0 );
		}
	}
	else {
		if( GetBit( buf ) ) {
			x = 0;
			u32 n = 0;
			while( !buf->error ) {
				u8 byte = GetByte( buf );
				x |= ( byte & 127 ) << ( n * 7 );
				if( ( byte & ( 1 << 7 ) ) == 0 )
					break;
				n++;
			}
		}
		else {
			x = baseline;
		}
	}
}

static void DeltaHalf( DeltaBuffer * buf, float & x, const float & baseline ) {
	u16 half_x = FloatToHalf( x );
	u16 half_baseline = FloatToHalf( baseline );
	Delta( buf, half_x, half_baseline );
	x = HalfToFloat( half_x );
}

static void DeltaAngle( DeltaBuffer * buf, float & x, const float & baseline ) {
	u16 angle16 = AngleNormalize360( x ) / 360.0f * U16_MAX;
	u16 baseline16 = AngleNormalize360( baseline ) / 360.0f * U16_MAX;
	Delta( buf, angle16, baseline16 );
	x = angle16 / float( U16_MAX ) * 360.0f;
}

static void DeltaAngle( DeltaBuffer * buf, vec3_t & v, const vec3_t & baseline ) {
	DeltaAngle( buf, v[ 0 ], baseline[ 0 ] );
	DeltaAngle( buf, v[ 1 ], baseline[ 1 ] );
	DeltaAngle( buf, v[ 2 ], baseline[ 2 ] );
}

//==================================================
// WRITE FUNCTIONS
//==================================================


void MSG_WriteData( msg_t *msg, const void *data, size_t length ) {
	MSG_CopyData( msg, data, length );
}

void MSG_CopyData( msg_t *buf, const void *data, size_t length ) {
	memcpy( MSG_GetSpace( buf, length ), data, length );
}

void MSG_WriteInt8( msg_t *msg, int c ) {
	uint8_t *buf = ( uint8_t* )MSG_GetSpace( msg, 1 );
	buf[0] = ( char )c;
}

void MSG_WriteUint8( msg_t *msg, int c ) {
	uint8_t *buf = ( uint8_t* )MSG_GetSpace( msg, 1 );
	buf[0] = ( uint8_t )( c & 0xff );
}

void MSG_WriteInt16( msg_t *msg, int c ) {
	uint8_t *buf = ( uint8_t* )MSG_GetSpace( msg, 2 );
	buf[0] = ( uint8_t )( c & 0xff );
	buf[1] = ( uint8_t )( ( c >> 8 ) & 0xff );
}

void MSG_WriteUint16( msg_t *msg, unsigned c ) {
	uint8_t *buf = ( uint8_t* )MSG_GetSpace( msg, 2 );
	buf[0] = ( uint8_t )( c & 0xff );
	buf[1] = ( uint8_t )( ( c >> 8 ) & 0xff );
}

void MSG_WriteInt32( msg_t *msg, int c ) {
	uint8_t *buf = ( uint8_t* )MSG_GetSpace( msg, 4 );
	buf[0] = ( uint8_t )( c & 0xff );
	buf[1] = ( uint8_t )( ( c >> 8 ) & 0xff );
	buf[2] = ( uint8_t )( ( c >> 16 ) & 0xff );
	buf[3] = ( uint8_t )( c >> 24 );
}

void MSG_WriteInt64( msg_t *msg, int64_t c ) {
	uint8_t *buf = ( uint8_t* )MSG_GetSpace( msg, 8 );
	buf[0] = ( uint8_t )( c & 0xffL );
	buf[1] = ( uint8_t )( ( c >> 8L ) & 0xffL );
	buf[2] = ( uint8_t )( ( c >> 16L ) & 0xffL );
	buf[3] = ( uint8_t )( ( c >> 24L ) & 0xffL );
	buf[4] = ( uint8_t )( ( c >> 32L ) & 0xffL );
	buf[5] = ( uint8_t )( ( c >> 40L ) & 0xffL );
	buf[6] = ( uint8_t )( ( c >> 48L ) & 0xffL );
	buf[7] = ( uint8_t )( c >> 56L );
}

void MSG_WriteUintBase128( msg_t *msg, uint64_t c ) {
	uint8_t buf[10];
	size_t len = 0;

	do {
		buf[len] = c & 0x7fU;
		if ( c >>= 7 ) {
			buf[len] |= 0x80U;
		}
		len++;
	} while( c );

	MSG_WriteData( msg, buf, len );
}

void MSG_WriteIntBase128( msg_t *msg, int64_t c ) {
	// use Zig-zag encoding for signed integers for more efficient storage
	uint64_t cc = (uint64_t)(c << 1) ^ (uint64_t)(c >> 63);
	MSG_WriteUintBase128( msg, cc );
}

void MSG_WriteString( msg_t *msg, const char *s ) {
	if( !s ) {
		MSG_WriteData( msg, "", 1 );
	} else {
		int l = strlen( s );
		if( l >= MAX_MSG_STRING_CHARS ) {
			Com_Printf( "MSG_WriteString: MAX_MSG_STRING_CHARS overflow" );
			MSG_WriteData( msg, "", 1 );
			return;
		}
		MSG_WriteData( msg, s, l + 1 );
	}
}

//==================================================
// READ FUNCTIONS
//==================================================

void MSG_BeginReading( msg_t *msg ) {
	msg->readcount = 0;
}

int MSG_ReadInt8( msg_t *msg ) {
	int i = (signed char)msg->data[msg->readcount++];
	if( msg->readcount > msg->cursize ) {
		i = -1;
	}
	return i;
}


int MSG_ReadUint8( msg_t *msg ) {
	msg->readcount++;
	if( msg->readcount > msg->cursize ) {
		return 0;
	}

	return ( unsigned char )( msg->data[msg->readcount - 1] );
}

int16_t MSG_ReadInt16( msg_t *msg ) {
	msg->readcount += 2;
	if( msg->readcount > msg->cursize ) {
		return -1;
	}
	return ( int16_t )( msg->data[msg->readcount - 2] | ( msg->data[msg->readcount - 1] << 8 ) );
}

uint16_t MSG_ReadUint16( msg_t *msg ) {
	msg->readcount += 2;
	if( msg->readcount > msg->cursize ) {
		return 0;
	}
	return ( uint16_t )( msg->data[msg->readcount - 2] | ( msg->data[msg->readcount - 1] << 8 ) );
}

int MSG_ReadInt32( msg_t *msg ) {
	msg->readcount += 4;
	if( msg->readcount > msg->cursize ) {
		return -1;
	}

	return msg->data[msg->readcount - 4]
		   | ( msg->data[msg->readcount - 3] << 8 )
		   | ( msg->data[msg->readcount - 2] << 16 )
		   | ( msg->data[msg->readcount - 1] << 24 );
}

int64_t MSG_ReadInt64( msg_t *msg ) {
	msg->readcount += 8;
	if( msg->readcount > msg->cursize ) {
		return -1;
	}

	return ( int64_t )msg->data[msg->readcount - 8]
		| ( ( int64_t )msg->data[msg->readcount - 7] << 8L )
		| ( ( int64_t )msg->data[msg->readcount - 6] << 16L )
		| ( ( int64_t )msg->data[msg->readcount - 5] << 24L )
		| ( ( int64_t )msg->data[msg->readcount - 4] << 32L )
		| ( ( int64_t )msg->data[msg->readcount - 3] << 40L )
		| ( ( int64_t )msg->data[msg->readcount - 2] << 48L )
		| ( ( int64_t )msg->data[msg->readcount - 1] << 56L );
}

uint64_t MSG_ReadUintBase128( msg_t *msg ) {
	size_t len = 0;
	uint64_t i = 0;

	while( len < 10 ) {
		uint8_t c = MSG_ReadUint8( msg );
		i |= (c & 0x7fLL) << (7 * len);
		len++;
		if( !(c & 0x80) ) {
			break;
		}
	}

	return i;
}

int64_t MSG_ReadIntBase128( msg_t *msg ) {
	// un-Zig-Zag our value back to a signed integer
	uint64_t c = MSG_ReadUintBase128( msg );
	return (int64_t)(c >> 1) ^ (-(int64_t)(c & 1));
}

void MSG_ReadData( msg_t *msg, void *data, size_t length ) {
	unsigned int i;

	for( i = 0; i < length; i++ )
		( (uint8_t *)data )[i] = MSG_ReadUint8( msg );

}

int MSG_SkipData( msg_t *msg, size_t length ) {
	if( msg->readcount + length <= msg->cursize ) {
		msg->readcount += length;
		return 1;
	}
	return 0;
}

static char *MSG_ReadString2( msg_t *msg, bool linebreak ) {
	int l, c;
	static char string[MAX_MSG_STRING_CHARS];

	l = 0;
	do {
		c = MSG_ReadUint8( msg );
		if( c == -1 || c == 0 || ( linebreak && c == '\n' ) ) {
			break;
		}

		string[l] = c;
		l++;
	} while( (unsigned int)l < sizeof( string ) - 1 );

	string[l] = 0;

	return string;
}

char *MSG_ReadString( msg_t *msg ) {
	return MSG_ReadString2( msg, false );
}

char *MSG_ReadStringLine( msg_t *msg ) {
	return MSG_ReadString2( msg, true );
}

//==================================================
// DELTA ENTITIES
//==================================================

static void Delta( DeltaBuffer * buf, SyncEntityState & ent, const SyncEntityState & baseline ) {
	Delta( buf, ent.events[ 0 ], baseline.events[ 0 ] );
	Delta( buf, ent.eventParms[ 0 ], baseline.eventParms[ 0 ] );

	Delta( buf, ent.origin, baseline.origin );
	DeltaAngle( buf, ent.angles, baseline.angles );

	Delta( buf, ent.teleported, baseline.teleported );

	Delta( buf, ent.type, baseline.type );
	Delta( buf, ent.solid, baseline.solid );
	Delta( buf, ent.modelindex, baseline.modelindex );
	Delta( buf, ent.svflags, baseline.svflags );
	Delta( buf, ent.effects, baseline.effects );
	Delta( buf, ent.ownerNum, baseline.ownerNum );
	Delta( buf, ent.targetNum, baseline.targetNum );
	Delta( buf, ent.sound, baseline.sound );
	Delta( buf, ent.modelindex2, baseline.modelindex2 );
	DeltaHalf( buf, ent.attenuation, baseline.attenuation );
	Delta( buf, ent.counterNum, baseline.counterNum );
	Delta( buf, ent.bodyOwner, baseline.bodyOwner );
	Delta( buf, ent.channel, baseline.channel );
	Delta( buf, ent.events[ 1 ], baseline.events[ 1 ] );
	Delta( buf, ent.eventParms[ 1 ], baseline.eventParms[ 1 ] );
	Delta( buf, ent.weapon, baseline.weapon );
	Delta( buf, ent.damage, baseline.damage );
	Delta( buf, ent.radius, baseline.radius );
	Delta( buf, ent.team, baseline.team );

	Delta( buf, ent.origin2, baseline.origin2 );

	Delta( buf, ent.linearMovementTimeStamp, baseline.linearMovementTimeStamp );
	Delta( buf, ent.linearMovement, baseline.linearMovement );
	Delta( buf, ent.linearMovementDuration, baseline.linearMovementDuration );
	Delta( buf, ent.linearMovementVelocity, baseline.linearMovementVelocity );
	Delta( buf, ent.linearMovementBegin, baseline.linearMovementBegin );
	Delta( buf, ent.linearMovementEnd, baseline.linearMovementEnd );

	Delta( buf, ent.colorRGBA, baseline.colorRGBA );
	Delta( buf, ent.silhouetteColor, baseline.silhouetteColor );

	Delta( buf, ent.light, baseline.light );
}

// static const msg_field_t ent_state_fields[] = {
// 	{ ESOFS( events[0] ), 32, 1, WIRE_UBASE128 },
// 	{ ESOFS( eventParms[0] ), 32, 1, WIRE_BASE128 },
//
// 	{ ESOFS( origin[0] ), 0, 1, WIRE_FLOAT },
// 	{ ESOFS( origin[1] ), 0, 1, WIRE_FLOAT },
// 	{ ESOFS( origin[2] ), 0, 1, WIRE_FLOAT },
//
// 	{ ESOFS( angles[0] ), 0, 1, WIRE_ANGLE },
// 	{ ESOFS( angles[1] ), 0, 1, WIRE_ANGLE },
//
// 	{ ESOFS( teleported ), 1, 1, WIRE_BOOL },
//
// 	{ ESOFS( type ), 32, 1, WIRE_UBASE128 },
// 	{ ESOFS( solid ), 32, 1, WIRE_UBASE128 },
// 	{ ESOFS( modelindex ), 32, 1, WIRE_FIXED_INT8 },
// 	{ ESOFS( svflags ), 32, 1, WIRE_UBASE128 },
// 	{ ESOFS( effects ), 32, 1, WIRE_UBASE128 },
// 	{ ESOFS( ownerNum ), 32, 1, WIRE_BASE128 },
// 	{ ESOFS( targetNum ), 32, 1, WIRE_BASE128 },
// 	{ ESOFS( sound ), 32, 1, WIRE_FIXED_INT8 },
// 	{ ESOFS( modelindex2 ), 32, 1, WIRE_FIXED_INT8 },
// 	{ ESOFS( attenuation ), 0, 1, WIRE_HALF_FLOAT },
// 	{ ESOFS( counterNum ), 32, 1, WIRE_BASE128 },
// 	{ ESOFS( bodyOwner ), 32, 1, WIRE_UBASE128 },
// 	{ ESOFS( channel ), 32, 1, WIRE_FIXED_INT8 },
// 	{ ESOFS( events[1] ), 32, 1, WIRE_UBASE128 },
// 	{ ESOFS( eventParms[1] ), 32, 1, WIRE_BASE128 },
// 	{ ESOFS( weapon ), 32, 1, WIRE_UBASE128 },
// 	{ ESOFS( damage ), 32, 1, WIRE_UBASE128 },
// 	{ ESOFS( radius ), 32, 1, WIRE_UBASE128 },
// 	{ ESOFS( team ), 32, 1, WIRE_FIXED_INT8 },
//
// 	{ ESOFS( origin2[0] ), 0, 1, WIRE_FLOAT },
// 	{ ESOFS( origin2[1] ), 0, 1, WIRE_FLOAT },
// 	{ ESOFS( origin2[2] ), 0, 1, WIRE_FLOAT },
//
// 	{ ESOFS( linearMovementTimeStamp ), 32, 1, WIRE_UBASE128 },
// 	{ ESOFS( linearMovement ), 1, 1, WIRE_BOOL },
// 	{ ESOFS( linearMovementDuration ), 32, 1, WIRE_UBASE128 },
// 	{ ESOFS( linearMovementVelocity[0] ), 0, 1, WIRE_FLOAT },
// 	{ ESOFS( linearMovementVelocity[1] ), 0, 1, WIRE_FLOAT },
// 	{ ESOFS( linearMovementVelocity[2] ), 0, 1, WIRE_FLOAT },
// 	{ ESOFS( linearMovementBegin[0] ), 0, 1, WIRE_FLOAT },
// 	{ ESOFS( linearMovementBegin[1] ), 0, 1, WIRE_FLOAT },
// 	{ ESOFS( linearMovementBegin[2] ), 0, 1, WIRE_FLOAT },
// 	{ ESOFS( linearMovementEnd[0] ), 0, 1, WIRE_FLOAT },
// 	{ ESOFS( linearMovementEnd[1] ), 0, 1, WIRE_FLOAT },
// 	{ ESOFS( linearMovementEnd[2] ), 0, 1, WIRE_FLOAT },
//
// 	{ ESOFS( angles[2] ), 0, 1, WIRE_ANGLE },
//
// 	{ ESOFS( colorRGBA ), 32, 1, WIRE_FIXED_INT32 },
// 	{ ESOFS( silhouetteColor ), 32, 1, WIRE_FIXED_INT32 },
//
// 	{ ESOFS( light ), 32, 1, WIRE_FIXED_INT32 },
// };

void MSG_WriteEntityNumber( msg_t *msg, int number, bool remove ) {
	MSG_WriteIntBase128( msg, (remove ? 1 : 0) | number << 1 );
}

/*
* MSG_ReadEntityNumber
*
* Returns the entity number and the remove bit
*/
int MSG_ReadEntityNumber( msg_t *msg, bool *remove ) {
	int number = (int)MSG_ReadIntBase128( msg );
	*remove = ( number & 1 ) != 0;
	return number >> 1;
}

void MSG_WriteDeltaEntity( msg_t * msg, const SyncEntityState * baseline, const SyncEntityState * ent, bool force ) {
	u8 buf[ MAX_MSGLEN ];
	DeltaBuffer delta = DeltaWriter( buf, sizeof( buf ) );

	Delta( &delta, *const_cast< SyncEntityState * >( ent ), * baseline );

	bool changed = false;
	for( u8 x : delta.field_mask ) {
		if( x != 0 ) {
			changed = true;
			break;
		}
	}

	if( !changed && !force ) {
		return;
	}

	MSG_WriteEntityNumber( msg, ent->number, false );
	MSG_WriteDeltaBuffer( msg, delta );
}

void MSG_ReadDeltaEntity( msg_t * msg, const SyncEntityState * baseline, SyncEntityState * ent ) {
	DeltaBuffer delta = MSG_StartReadingDeltaBuffer( msg );
	Delta( &delta, *ent, *baseline );
	MSG_FinishReadingDeltaBuffer( msg, delta );
}

//==================================================
// DELTA USER CMDS
//==================================================

// static const msg_field_t usercmd_fields[] = {
// 	{ UCOFS( angles[0] ), 16, 1, WIRE_FIXED_INT16 },
// 	{ UCOFS( angles[1] ), 16, 1, WIRE_FIXED_INT16 },
// 	{ UCOFS( angles[2] ), 16, 1, WIRE_FIXED_INT16 },
//
// 	{ UCOFS( forwardmove ), 8, 1, WIRE_FIXED_INT8 },
// 	{ UCOFS( sidemove ), 8, 1, WIRE_FIXED_INT8 },
// 	{ UCOFS( upmove ), 8, 1, WIRE_FIXED_INT8 },
//
// 	{ UCOFS( buttons ), 32, 1, WIRE_UBASE128 },
// };

static void Delta( DeltaBuffer * buf, usercmd_t & cmd, const usercmd_t & baseline ) {
	Delta( buf, cmd.angles, baseline.angles );
	Delta( buf, cmd.forwardmove, baseline.forwardmove );
	Delta( buf, cmd.sidemove, baseline.sidemove );
	Delta( buf, cmd.upmove, baseline.upmove );
	Delta( buf, cmd.buttons, baseline.buttons );
}

void MSG_WriteDeltaUsercmd( msg_t * msg, const usercmd_t * baseline, const usercmd_t * cmd ) {
	u8 buf[ MAX_MSGLEN ];
	DeltaBuffer delta = DeltaWriter( buf, sizeof( buf ) );

	Delta( &delta, *const_cast< usercmd_t * >( cmd ), *baseline );

	MSG_WriteDeltaBuffer( msg, delta );
	MSG_WriteIntBase128( msg, cmd->serverTimeStamp );
}

void MSG_ReadDeltaUsercmd( msg_t * msg, const usercmd_t * baseline, usercmd_t * cmd ) {
	DeltaBuffer delta = MSG_StartReadingDeltaBuffer( msg );
	Delta( &delta, *cmd, *baseline );
	MSG_FinishReadingDeltaBuffer( msg, delta );
	cmd->serverTimeStamp = MSG_ReadIntBase128( msg );
}

//==================================================
// DELTA PLAYER STATES
//==================================================

// static const msg_field_t player_state_msg_fields[] = {
// 	{ PSOFS( pmove.pm_type ), 32, 1, WIRE_UBASE128 },
//
// 	{ PSOFS( pmove.origin[0] ), 0, 1, WIRE_FLOAT },
// 	{ PSOFS( pmove.origin[1] ), 0, 1, WIRE_FLOAT },
// 	{ PSOFS( pmove.origin[2] ), 0, 1, WIRE_FLOAT },
//
// 	{ PSOFS( pmove.velocity[0] ), 0, 1, WIRE_FLOAT },
// 	{ PSOFS( pmove.velocity[1] ), 0, 1, WIRE_FLOAT },
// 	{ PSOFS( pmove.velocity[2] ), 0, 1, WIRE_FLOAT },
//
// 	{ PSOFS( pmove.pm_time ), 32, 1, WIRE_UBASE128 },
//
// 	{ PSOFS( pmove.pm_flags ), 32, 1, WIRE_UBASE128 },
//
// 	{ PSOFS( pmove.delta_angles[0] ), 16, 1, WIRE_FIXED_INT16 },
// 	{ PSOFS( pmove.delta_angles[1] ), 16, 1, WIRE_FIXED_INT16 },
// 	{ PSOFS( pmove.delta_angles[2] ), 16, 1, WIRE_FIXED_INT16 },
//
// 	{ PSOFS( event[0] ), 32, 1, WIRE_UBASE128 },
// 	{ PSOFS( eventParm[0] ), 32, 1, WIRE_UBASE128 },
//
// 	{ PSOFS( event[1] ), 32, 1, WIRE_UBASE128 },
// 	{ PSOFS( eventParm[1] ), 32, 1, WIRE_UBASE128 },
//
// 	{ PSOFS( viewangles[0] ), 0, 1, WIRE_ANGLE },
// 	{ PSOFS( viewangles[1] ), 0, 1, WIRE_ANGLE },
// 	{ PSOFS( viewangles[2] ), 0, 1, WIRE_ANGLE },
//
// 	{ PSOFS( pmove.gravity ), 32, 1, WIRE_UBASE128 },
//
// 	{ PSOFS( weapon_state ), 8, 1, WIRE_FIXED_INT8 },
//
// 	{ PSOFS( fov ), 0, 1, WIRE_HALF_FLOAT },
//
// 	{ PSOFS( POVnum ), 32, 1, WIRE_UBASE128 },
// 	{ PSOFS( playerNum ), 32, 1, WIRE_UBASE128 },
//
// 	{ PSOFS( viewheight ), 32, 1, WIRE_HALF_FLOAT },
//
// 	{ PSOFS( plrkeys ), 32, 1, WIRE_UBASE128 },
//
// 	{ PSOFS( stats ), 16, PS_MAX_STATS, WIRE_BASE128 },
//
// 	{ PSOFS( pmove.stats ), 16, PM_STAT_SIZE, WIRE_BASE128 },
// 	{ PSOFS( inventory ), 32, MAX_ITEMS, WIRE_UBASE128 },
// };

static void Delta( DeltaBuffer * buf, pmove_state_t & pmove, const pmove_state_t & baseline ) {
	Delta( buf, pmove.pm_type, baseline.pm_type );

	Delta( buf, pmove.origin, baseline.origin );
	Delta( buf, pmove.velocity, baseline.velocity );

	Delta( buf, pmove.pm_time, baseline.pm_time );

	Delta( buf, pmove.pm_flags, baseline.pm_flags );

	Delta( buf, pmove.delta_angles, baseline.delta_angles );

	Delta( buf, pmove.gravity, baseline.gravity );

	Delta( buf, pmove.stats, baseline.stats );
}

static void Delta( DeltaBuffer * buf, SyncPlayerState::WeaponInfo & weapon, const SyncPlayerState::WeaponInfo & baseline ) {
	Delta( buf, weapon.owned, baseline.owned );
	Delta( buf, weapon.ammo, baseline.ammo );
}

static void Delta( DeltaBuffer * buf, SyncPlayerState & player, const SyncPlayerState & baseline ) {
	Delta( buf, player.pmove, baseline.pmove );

	Delta( buf, player.event, baseline.event );
	Delta( buf, player.eventParm, baseline.eventParm );

	DeltaAngle( buf, player.viewangles, baseline.viewangles );

	Delta( buf, player.weapon_state, baseline.weapon_state );

	DeltaHalf( buf, player.fov, baseline.fov );

	Delta( buf, player.POVnum, baseline.POVnum );
	Delta( buf, player.playerNum, baseline.playerNum );

	DeltaHalf( buf, player.viewheight, baseline.viewheight );

	Delta( buf, player.plrkeys, baseline.plrkeys );

	Delta( buf, player.weapons, baseline.weapons );
	Delta( buf, player.items, baseline.items );

	Delta( buf, player.show_scoreboard, baseline.show_scoreboard );
	Delta( buf, player.ready, baseline.ready );
	Delta( buf, player.voted, baseline.voted );
	Delta( buf, player.can_change_loadout, baseline.can_change_loadout );
	Delta( buf, player.can_plant, baseline.can_plant );
	Delta( buf, player.carrying_bomb, baseline.carrying_bomb );

	Delta( buf, player.health, baseline.health );

	Delta( buf, player.weapon, baseline.weapon );
	Delta( buf, player.pending_weapon, baseline.pending_weapon );
	Delta( buf, player.weapon_time, baseline.weapon_time );

	Delta( buf, player.team, baseline.team );
	Delta( buf, player.real_team, baseline.real_team );

	Delta( buf, player.progress_type, baseline.progress_type );
	Delta( buf, player.progress, baseline.progress );

	Delta( buf, player.last_killer, baseline.last_killer );
	Delta( buf, player.pointed_player, baseline.pointed_player );
	Delta( buf, player.pointed_health, baseline.pointed_health );
}

void MSG_WriteDeltaPlayerState( msg_t * msg, const SyncPlayerState * baseline, const SyncPlayerState * player ) {
	static SyncPlayerState dummy;
	if( baseline == NULL ) {
		baseline = &dummy;
	}

	u8 buf[ MAX_MSGLEN ];
	DeltaBuffer delta = DeltaWriter( buf, sizeof( buf ) );

	Delta( &delta, *const_cast< SyncPlayerState * >( player ), *baseline );

	MSG_WriteDeltaBuffer( msg, delta );
}

void MSG_ReadDeltaPlayerState( msg_t * msg, const SyncPlayerState * baseline, SyncPlayerState * player ) {
	static SyncPlayerState dummy;
	if( baseline == NULL ) {
		baseline = &dummy;
	}

	DeltaBuffer delta = MSG_StartReadingDeltaBuffer( msg );
	Delta( &delta, *player, *baseline );
	MSG_FinishReadingDeltaBuffer( msg, delta );
}

//==================================================
// DELTA GAME STATES
//==================================================

static void Delta( DeltaBuffer * buf, SyncBombGameState & bomb, const SyncBombGameState & baseline ) {
	Delta( buf, bomb.alpha_score, baseline.alpha_score );
	Delta( buf, bomb.beta_score, baseline.beta_score );
	Delta( buf, bomb.alpha_players_alive, baseline.alpha_players_alive );
	Delta( buf, bomb.alpha_players_total, baseline.alpha_players_total );
	Delta( buf, bomb.beta_players_alive, baseline.beta_players_alive );
	Delta( buf, bomb.beta_players_total, baseline.beta_players_total );
}

static void Delta( DeltaBuffer * buf, SyncGameState & state, const SyncGameState & baseline ) {
	Delta( buf, state.flags, baseline.flags );
	Delta( buf, state.match_state, baseline.match_state );
	Delta( buf, state.match_start, baseline.match_start );
	Delta( buf, state.match_duration, baseline.match_duration );
	Delta( buf, state.clock_override, baseline.clock_override );
	Delta( buf, state.round_type, baseline.round_type );
	Delta( buf, state.max_team_players, baseline.max_team_players );
	Delta( buf, state.bomb, baseline.bomb );
}

void MSG_WriteDeltaGameState( msg_t * msg, const SyncGameState * baseline, const SyncGameState * state ) {
	static SyncGameState dummy;
	if( baseline == NULL ) {
		baseline = &dummy;
	}

	u8 buf[ MAX_MSGLEN ];
	DeltaBuffer delta = DeltaWriter( buf, sizeof( buf ) );

	Delta( &delta, *const_cast< SyncGameState * >( state ), *baseline );

	MSG_WriteDeltaBuffer( msg, delta );
}

void MSG_ReadDeltaGameState( msg_t * msg, const SyncGameState * baseline, SyncGameState * state ) {
	static SyncGameState dummy;
	if( baseline == NULL ) {
		baseline = &dummy;
	}

	DeltaBuffer delta = MSG_StartReadingDeltaBuffer( msg );
	Delta( &delta, *state, *baseline );
	MSG_FinishReadingDeltaBuffer( msg, delta );
}
