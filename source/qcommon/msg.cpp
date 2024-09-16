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
#include "qcommon/rng.h"

#define MAX_MSG_STRING_CHARS    2048

msg_t NewMSGWriter( u8 * data, size_t n ) {
	msg_t msg = { };
	msg.data = data;
	msg.maxsize = n;
	return msg;
}

msg_t NewMSGReader( u8 * data, size_t n, size_t data_size ) {
	msg_t msg = { };
	msg.data = data;
	msg.maxsize = data_size;
	msg.cursize = n;
	return msg;
}

void MSG_Clear( msg_t * msg ) {
	msg->cursize = 0;
	msg->compressed = false;
}

void *MSG_GetSpace( msg_t * msg, size_t length ) {
	void *ptr;

	Assert( msg->cursize + length <= msg->maxsize );
	if( msg->cursize + length > msg->maxsize ) {
		Fatal( "MSG_GetSpace: overflowed" );
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
	MSG_Write( msg, delta.field_mask, bytes );
	MSG_Write( msg, delta.buf, delta.cursor - delta.buf );
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
	return DeltaBuffer {
		.buf = buf,
		.cursor = buf,
		.end = buf + n,
		.serializing = true,
	};
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

static void Delta( DeltaBuffer * buf, char & x, char baseline ) { DeltaFundamental( buf, x, baseline ); }
static void Delta( DeltaBuffer * buf, s8 & x, s8 baseline ) { DeltaFundamental( buf, x, baseline ); }
static void Delta( DeltaBuffer * buf, s16 & x, s16 baseline ) { DeltaFundamental( buf, x, baseline ); }
static void Delta( DeltaBuffer * buf, s32 & x, s32 baseline ) { DeltaFundamental( buf, x, baseline ); }
static void Delta( DeltaBuffer * buf, s64 & x, s64 baseline ) { DeltaFundamental( buf, x, baseline ); }
static void Delta( DeltaBuffer * buf, u8 & x, u8 baseline ) { DeltaFundamental( buf, x, baseline ); }
static void Delta( DeltaBuffer * buf, u16 & x, u16 baseline ) { DeltaFundamental( buf, x, baseline ); }
static void Delta( DeltaBuffer * buf, u32 & x, u32 baseline ) { DeltaFundamental( buf, x, baseline ); }
static void Delta( DeltaBuffer * buf, u64 & x, u64 baseline ) { DeltaFundamental( buf, x, baseline ); }
static void Delta( DeltaBuffer * buf, float & x, float baseline ) { DeltaFundamental( buf, x, baseline ); }

static void Delta( DeltaBuffer * buf, bool & b, bool baseline ) {
	if( buf->serializing ) {
		AddBit( buf, b != baseline );
	}
	else {
		b = GetBit( buf ) ? !baseline : baseline;
	}
}

template< typename T >
void Delta( DeltaBuffer * buf, Optional< T > & x, const Optional< T > & baseline ) {
	constexpr T null_baseline = T();

	Delta( buf, x.exists, baseline.exists );

	const T & baseline_to_delta_against = baseline.exists ? baseline.value : null_baseline;
	if( x.exists ) {
		Delta( buf, x.value, baseline_to_delta_against );
	}
}

static void Delta( DeltaBuffer * buf, StringHash & hash, StringHash baseline ) {
	DeltaFundamental( buf, hash.hash, baseline.hash );
}

template< typename T, size_t N >
void Delta( DeltaBuffer * buf, T ( &arr )[ N ], const T ( &baseline )[ N ] ) {
	for( size_t i = 0; i < N; i++ ) {
		Delta( buf, arr[ i ], baseline[ i ] );
	}
}

static void Delta( DeltaBuffer * buf, Vec3 & v, const Vec3 & baseline ) {
	for( int i = 0; i < 3; i++ ) {
		Delta( buf, v[ i ], baseline[ i ] );
	}
}

static void Delta( DeltaBuffer * buf, MinMax3 & b, const MinMax3 & baseline ) {
	Delta( buf, b.mins, baseline.mins );
	Delta( buf, b.maxs, baseline.maxs );
}

static void Delta( DeltaBuffer * buf, RGBA8 & rgba, const RGBA8 & baseline ) {
	Delta( buf, rgba.r, baseline.r );
	Delta( buf, rgba.g, baseline.b );
	Delta( buf, rgba.b, baseline.g );
	Delta( buf, rgba.a, baseline.a );
}

static void Delta( DeltaBuffer * buf, Sphere & s, const Sphere & baseline ) {
	Delta( buf, s.center, baseline.center );
	Delta( buf, s.radius, baseline.radius );
}

static void Delta( DeltaBuffer * buf, Capsule & c, const Capsule & baseline ) {
	Delta( buf, c.a, baseline.a );
	Delta( buf, c.b, baseline.b );
	Delta( buf, c.radius, baseline.radius );
}

template< typename E >
void DeltaEnum( DeltaBuffer * buf, E & x, E baseline, E count ) {
	using T = UnderlyingType< E >;
	Delta( buf, ( T & ) x, ( const T & ) baseline );
	if( x < 0 || x >= count ) {
		buf->error = true;
	}
}

template< typename E >
void DeltaEnum( DeltaBuffer * buf, Optional< E > & x, Optional< E > baseline, E count ) {
	constexpr E null_baseline = E();

	Delta( buf, x.exists, baseline.exists );

	using T = UnderlyingType< E >;
	const T & baseline_to_delta_against = baseline.exists ? baseline.value : null_baseline;
	if( x.exists ) {
		Delta( buf, ( T & ) x.value, baseline_to_delta_against );
		if( x.value < 0 || x.value >= count ) {
			buf->error = true;
		}
	}
}

template< typename E >
void DeltaBitfieldEnum( DeltaBuffer * buf, E & x, E baseline, E mask ) {
	using T = UnderlyingType< E >;
	Delta( buf, ( T & ) x, ( const T & ) baseline );
	if( ( x & ~mask ) != 0 ) {
		buf->error = true;
	}
}

template< typename E >
void DeltaBitfieldEnum( DeltaBuffer * buf, Optional< E > & x, Optional< E > baseline, E mask ) {
	constexpr E null_baseline = E();

	Delta( buf, x.exists, baseline.exists );

	using T = UnderlyingType< E >;
	const T & baseline_to_delta_against = baseline.exists ? baseline.value : null_baseline;
	if( x.exists ) {
		Delta( buf, ( T & ) x.value, baseline_to_delta_against );
		if( ( x.value & ~mask ) != 0 ) {
			buf->error = true;
		}
	}
}

static void Delta( DeltaBuffer * buf, CollisionModel & cm, const CollisionModel & baseline ) {
	constexpr CollisionModel null_baseline = { };

	DeltaEnum( buf, cm.type, baseline.type, CollisionModelType_Count );

	const CollisionModel * baseline_to_delta_against = cm.type == baseline.type ? &baseline : &null_baseline;

	switch( cm.type ) {
		case CollisionModelType_Point:
			break;

		case CollisionModelType_AABB:
			Delta( buf, cm.aabb, baseline_to_delta_against->aabb );
			break;

		case CollisionModelType_Sphere:
			Delta( buf, cm.sphere, baseline_to_delta_against->sphere );
			break;

		case CollisionModelType_Capsule:
			Delta( buf, cm.capsule, baseline_to_delta_against->capsule );
			break;

		case CollisionModelType_MapModel:
			Delta( buf, cm.map_model, baseline_to_delta_against->map_model );
			break;

		case CollisionModelType_GLTF:
			Delta( buf, cm.gltf_model, baseline_to_delta_against->gltf_model );
			break;
	}
}

template< size_t N >
constexpr auto SmallestSufficientIntType() {
	if constexpr ( N < ( 1_u64 << 8 ) ) { return u8(); }
	else if constexpr ( N < ( 1_u64 << 16 ) ) { return u16(); }
	else if constexpr ( N < ( 1_u64 << 32 ) ) { return u32(); }
	else { return u64(); }
}

template< size_t N >
static void DeltaString( DeltaBuffer * buf, char ( &str )[ N ], const char ( &baseline )[ N ] ) {
	if( buf->serializing ) {
		bool diff = !StrEqual( str, baseline );
		AddBit( buf, diff );
		if( diff ) {
			decltype( SmallestSufficientIntType< N >() ) n = strlen( str );
			AddBytes( buf, &n, sizeof( n ) );
			AddBytes( buf, str, n );
		}
	}
	else {
		if( GetBit( buf ) ) {
			decltype( SmallestSufficientIntType< N >() ) n;
			GetBytes( buf, &n, sizeof( n ) );
			GetBytes( buf, str, n );
			str[ n ] = '\0';
		}
		else {
			SafeStrCpy( str, baseline, N );
		}
	}
}

static void DeltaAngle( DeltaBuffer * buf, float & x, const float & baseline, float ( *normalize )( float ) ) {
	u16 angle16 = Quantize01< u16 >( AngleNormalize360( x ) / 360.0f );
	u16 baseline16 = Quantize01< u16 >( AngleNormalize360( baseline ) / 360.0f );
	Delta( buf, angle16, baseline16 );
	x = normalize( Dequantize01< u16 >( angle16 ) * 360.0f );
}

static void Delta( DeltaBuffer * buf, EulerDegrees2 & a, const EulerDegrees2 & baseline ) {
	DeltaAngle( buf, a.pitch, baseline.pitch, AngleNormalize180 );
	DeltaAngle( buf, a.yaw, baseline.yaw, AngleNormalize360 );
}

static void Delta( DeltaBuffer * buf, EulerDegrees3 & a, const EulerDegrees3 & baseline ) {
	DeltaAngle( buf, a.pitch, baseline.pitch, AngleNormalize180 );
	DeltaAngle( buf, a.yaw, baseline.yaw, AngleNormalize360 );
	DeltaAngle( buf, a.roll, baseline.roll, AngleNormalize360 );
}

//==================================================
// WRITE FUNCTIONS
//==================================================

void MSG_Write( msg_t * msg, const void * data, size_t length ) {
	memcpy( MSG_GetSpace( msg, length ), data, length );
}

void MSG_WriteZeroes( msg_t * msg, size_t n ) {
	memset( MSG_GetSpace( msg, n ), 0, n );
}

template< typename T >
void MSG_WriteFundamental( msg_t * msg, T x ) {
	memcpy( MSG_GetSpace( msg, sizeof( T ) ), &x, sizeof( T ) );
}

void MSG_WriteInt8( msg_t * msg, s8 x ) { MSG_WriteFundamental( msg, x ); }
void MSG_WriteUint8( msg_t * msg, u8 x ) { MSG_WriteFundamental( msg, x ); }
void MSG_WriteInt16( msg_t * msg, s16 x ) { MSG_WriteFundamental( msg, x ); }
void MSG_WriteUint16( msg_t * msg, u16 x ) { MSG_WriteFundamental( msg, x ); }
void MSG_WriteInt32( msg_t * msg, s32 x ) { MSG_WriteFundamental( msg, x ); }
void MSG_WriteUint32( msg_t * msg, u32 x ) { MSG_WriteFundamental( msg, x ); }
void MSG_WriteInt64( msg_t * msg, s64 x ) { MSG_WriteFundamental( msg, x ); }
void MSG_WriteUint64( msg_t * msg, u64 x ) { MSG_WriteFundamental( msg, x ); }

void MSG_WriteUintBase128( msg_t * msg, uint64_t c ) {
	uint8_t buf[10];
	size_t len = 0;

	do {
		buf[len] = c & 0x7fU;
		if ( c >>= 7 ) {
			buf[len] |= 0x80U;
		}
		len++;
	} while( c );

	MSG_Write( msg, buf, len );
}

void MSG_WriteIntBase128( msg_t * msg, int64_t c ) {
	// use Zig-zag encoding for signed integers for more efficient storage
	uint64_t cc = (uint64_t)(c << 1) ^ (uint64_t)(c >> 63);
	MSG_WriteUintBase128( msg, cc );
}

void MSG_WriteString( msg_t * msg, Span< const char > str ) {
	Assert( str.n < MAX_MSG_STRING_CHARS );
	MSG_Write( msg, str.ptr, str.n );
	MSG_Write( msg, "", 1 );
}

void MSG_WriteString( msg_t * msg, const char * str ) {
	MSG_WriteString( msg, MakeSpan( str ) );
}

void MSG_WriteMsg( msg_t * msg, msg_t other ) {
	MSG_WriteUint16( msg, checked_cast< u16 >( other.cursize ) );
	MSG_Write( msg, other.data, other.cursize );
}

//==================================================
// READ FUNCTIONS
//==================================================

void MSG_BeginReading( msg_t * msg ) {
	msg->readcount = 0;
}

template< typename T >
T MSG_ReadFundamental( msg_t * msg, T def ) {
	if( msg->readcount + sizeof( T ) > msg->cursize ) {
		msg->readcount += sizeof( T );
		return def;
	}

	T x;
	memcpy( &x, &msg->data[ msg->readcount ], sizeof( T ) );
	msg->readcount += sizeof( T );
	return x;
}

s8 MSG_ReadInt8( msg_t * msg ) { return MSG_ReadFundamental< s8 >( msg, -1 ); }
u8 MSG_ReadUint8( msg_t * msg ) { return MSG_ReadFundamental< u8 >( msg, 0 ); }
s16 MSG_ReadInt16( msg_t * msg ) { return MSG_ReadFundamental< s16 >( msg, -1 ); }
u16 MSG_ReadUint16( msg_t * msg ) { return MSG_ReadFundamental< u16 >( msg, 0 ); }
s32 MSG_ReadInt32( msg_t * msg ) { return MSG_ReadFundamental< s32 >( msg, -1 ); }
u32 MSG_ReadUint32( msg_t * msg ) { return MSG_ReadFundamental< u32 >( msg, 0 ); }
s64 MSG_ReadInt64( msg_t * msg ) { return MSG_ReadFundamental< s64 >( msg, -1 ); }
u64 MSG_ReadUint64( msg_t * msg ) { return MSG_ReadFundamental< u64 >( msg, 0 ); }

uint64_t MSG_ReadUintBase128( msg_t * msg ) {
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

int64_t MSG_ReadIntBase128( msg_t * msg ) {
	// un-Zig-Zag our value back to a signed integer
	uint64_t c = MSG_ReadUintBase128( msg );
	return (int64_t)(c >> 1) ^ (-(int64_t)(c & 1));
}

void MSG_ReadData( msg_t * msg, void *data, size_t length ) {
	for( size_t i = 0; i < length; i++ ) {
		( (uint8_t *)data )[i] = MSG_ReadUint8( msg );
	}
}

int MSG_SkipData( msg_t * msg, size_t length ) {
	if( msg->readcount + length <= msg->cursize ) {
		msg->readcount += length;
		return 1;
	}
	return 0;
}

static const char * MSG_ReadString2( msg_t * msg, bool linebreak ) {
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

const char * MSG_ReadString( msg_t * msg ) {
	return MSG_ReadString2( msg, false );
}

const char * MSG_ReadStringLine( msg_t * msg ) {
	return MSG_ReadString2( msg, true );
}

msg_t MSG_ReadMsg( msg_t * msg ) {
	u16 len = MSG_ReadUint16( msg );
	if( msg->readcount + len > msg->cursize ) {
		return { };
	}

	msg_t result = NewMSGReader( &msg->data[ msg->readcount ], len, len );
	MSG_SkipData( msg, len );
	return result;
}

//==================================================
// DELTA ENTITIES
//==================================================

static void Delta( DeltaBuffer * buf, SyncEvent & event, const SyncEvent & baseline ) {
	Delta( buf, event.parm, baseline.parm );
	Delta( buf, event.type, baseline.type );
}

static void Delta( DeltaBuffer * buf, SyncEntityState & ent, const SyncEntityState & baseline ) {
	Delta( buf, ent.events, baseline.events );

	Delta( buf, ent.origin, baseline.origin );
	Delta( buf, ent.angles, baseline.angles );

	Delta( buf, ent.override_collision_model, baseline.override_collision_model );
	DeltaBitfieldEnum( buf, ent.solidity, baseline.solidity, SolidMask_Everything );

	Delta( buf, ent.teleported, baseline.teleported );

	DeltaEnum( buf, ent.type, baseline.type, EntityType_Count );
	Delta( buf, ent.model, baseline.model );
	Delta( buf, ent.material, baseline.material );
	Delta( buf, ent.color, baseline.color );
	DeltaBitfieldEnum( buf, ent.svflags, baseline.svflags, EntityFlags( U16_MAX ) );
	Delta( buf, ent.effects, baseline.effects );
	Delta( buf, ent.ownerNum, baseline.ownerNum );
	Delta( buf, ent.sound, baseline.sound );
	Delta( buf, ent.model2, baseline.model2 );
	Delta( buf, ent.mask, baseline.mask );
	Delta( buf, ent.animating, baseline.animating );
	Delta( buf, ent.animation_time, baseline.animation_time );
	Delta( buf, ent.site_letter, baseline.site_letter );
	Delta( buf, ent.positioned_sound, baseline.positioned_sound );
	DeltaEnum( buf, ent.perk, baseline.perk, Perk_Count );
	DeltaEnum( buf, ent.weapon, baseline.weapon, Weapon_Count );
	DeltaEnum( buf, ent.gadget, baseline.gadget, Gadget_Count );
	Delta( buf, ent.radius, baseline.radius );
	DeltaEnum( buf, ent.team, baseline.team, Team_Count );
	Delta( buf, ent.scale, baseline.scale );

	Delta( buf, ent.origin2, baseline.origin2 );

	Delta( buf, ent.linearMovementTimeStamp, baseline.linearMovementTimeStamp );
	Delta( buf, ent.linearMovement, baseline.linearMovement );
	Delta( buf, ent.linearMovementDuration, baseline.linearMovementDuration );
	Delta( buf, ent.linearMovementVelocity, baseline.linearMovementVelocity );
	Delta( buf, ent.linearMovementBegin, baseline.linearMovementBegin );
	Delta( buf, ent.linearMovementEnd, baseline.linearMovementEnd );
	Delta( buf, ent.linearMovementTimeDelta, baseline.linearMovementTimeDelta );

	Delta( buf, ent.silhouetteColor, baseline.silhouetteColor );

	Delta( buf, ent.id.id, baseline.id.id );
}

void MSG_WriteEntityNumber( msg_t * msg, int number, bool remove ) {
	MSG_WriteIntBase128( msg, (remove ? 1 : 0) | number << 1 );
}

/*
* MSG_ReadEntityNumber
*
* Returns the entity number and the remove bit
*/
int MSG_ReadEntityNumber( msg_t * msg, bool *remove ) {
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

static void Delta( DeltaBuffer * buf, UserCommand & cmd, const UserCommand & baseline ) {
	Delta( buf, cmd.angles, baseline.angles );
	Delta( buf, cmd.forwardmove, baseline.forwardmove );
	Delta( buf, cmd.sidemove, baseline.sidemove );
	DeltaEnum( buf, cmd.buttons, baseline.buttons, UserCommandButton( U8_MAX ) ); // TODO: dunno how to represent bitfields here
	DeltaEnum( buf, cmd.down_edges, baseline.down_edges, UserCommandButton( U8_MAX ) );
	Delta( buf, cmd.entropy, baseline.entropy );
	DeltaEnum( buf, cmd.weaponSwitch, baseline.weaponSwitch, Weapon_Count );
}

void MSG_WriteDeltaUsercmd( msg_t * msg, const UserCommand * baseline, const UserCommand * cmd ) {
	u8 buf[ MAX_MSGLEN ];
	DeltaBuffer delta = DeltaWriter( buf, sizeof( buf ) );

	Delta( &delta, *const_cast< UserCommand * >( cmd ), *baseline );

	MSG_WriteDeltaBuffer( msg, delta );
	MSG_WriteIntBase128( msg, cmd->serverTimeStamp );
}

void MSG_ReadDeltaUsercmd( msg_t * msg, const UserCommand * baseline, UserCommand * cmd ) {
	DeltaBuffer delta = MSG_StartReadingDeltaBuffer( msg );
	Delta( &delta, *cmd, *baseline );
	MSG_FinishReadingDeltaBuffer( msg, delta );
	cmd->serverTimeStamp = MSG_ReadIntBase128( msg );
}

//==================================================
// DELTA PLAYER STATES
//==================================================

static void Delta( DeltaBuffer * buf, pmove_state_t & pmove, const pmove_state_t & baseline ) {
	Delta( buf, pmove.pm_type, baseline.pm_type );

	Delta( buf, pmove.origin, baseline.origin );
	Delta( buf, pmove.velocity, baseline.velocity );
	Delta( buf, pmove.angles, baseline.angles );

	Delta( buf, pmove.pm_flags, baseline.pm_flags );

	Delta( buf, pmove.features, baseline.features );

	Delta( buf, pmove.no_shooting_time, baseline.no_shooting_time );
	Delta( buf, pmove.no_friction_time, baseline.no_friction_time );
	Delta( buf, pmove.stamina, baseline.stamina );
	Delta( buf, pmove.stamina_stored, baseline.stamina_stored );
	Delta( buf, pmove.jump_buffering, baseline.jump_buffering );
	DeltaEnum( buf, pmove.stamina_state, baseline.stamina_state, Stamina_Count );

	Delta( buf, pmove.max_speed, baseline.max_speed );
}

static void Delta( DeltaBuffer * buf, WeaponSlot & weapon, const WeaponSlot & baseline ) {
	DeltaEnum( buf, weapon.weapon, baseline.weapon, Weapon_Count );
	Delta( buf, weapon.ammo, baseline.ammo );
}

static void Delta( DeltaBuffer * buf, SyncPlayerState & player, const SyncPlayerState & baseline ) {
	Delta( buf, player.pmove, baseline.pmove );

	Delta( buf, player.events, baseline.events );

	Delta( buf, player.viewangles, baseline.viewangles );

	Delta( buf, player.POVnum, baseline.POVnum );
	Delta( buf, player.playerNum, baseline.playerNum );

	Delta( buf, player.viewheight, baseline.viewheight );

	Delta( buf, player.weapons, baseline.weapons );
	DeltaEnum( buf, player.gadget, baseline.gadget, Gadget_Count );
	DeltaEnum( buf, player.perk, baseline.perk, Perk_Count );

	Delta( buf, player.gadget_ammo, baseline.gadget_ammo );

	Delta( buf, player.ready, baseline.ready );
	Delta( buf, player.voted, baseline.voted );
	Delta( buf, player.can_change_loadout, baseline.can_change_loadout );
	Delta( buf, player.can_plant, baseline.can_plant );
	Delta( buf, player.carrying_bomb, baseline.carrying_bomb );

	Delta( buf, player.health, baseline.health );
	Delta( buf, player.max_health, baseline.max_health );
	Delta( buf, player.flashed, baseline.flashed );

	DeltaEnum( buf, player.weapon_state, baseline.weapon_state, WeaponState_Count );
	Delta( buf, player.weapon_state_time, baseline.weapon_state_time );

	DeltaEnum( buf, player.weapon, baseline.weapon, Weapon_Count );
	DeltaEnum( buf, player.pending_weapon, baseline.pending_weapon, Weapon_Count );
	Delta( buf, player.using_gadget, baseline.using_gadget );
	Delta( buf, player.pending_gadget, baseline.pending_gadget );
	DeltaEnum( buf, player.last_weapon, baseline.last_weapon, Weapon_Count );
	Delta( buf, player.zoom_time, baseline.zoom_time );

	DeltaEnum( buf, player.team, baseline.team, Team_Count );
	DeltaEnum( buf, player.real_team, baseline.real_team, Team_Count );

	DeltaEnum( buf, player.progress_type, baseline.progress_type, BombProgress_Count );
	Delta( buf, player.progress, baseline.progress );
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

static void Delta( DeltaBuffer * buf, SyncScoreboardPlayer & player, const SyncScoreboardPlayer & baseline ) {
	DeltaString( buf, player.name, baseline.name );
	Delta( buf, player.ping, baseline.ping );
	Delta( buf, player.score, baseline.score );
	Delta( buf, player.kills, baseline.kills );
	Delta( buf, player.ready, baseline.ready );
	Delta( buf, player.carrier, baseline.carrier );
	Delta( buf, player.alive, baseline.alive );
}

static void Delta( DeltaBuffer * buf, SyncTeamState & team, const SyncTeamState & baseline ) {
	Delta( buf, team.player_indices, baseline.player_indices );
	Delta( buf, team.score, baseline.score );
	Delta( buf, team.num_players, baseline.num_players );
}

static void Delta( DeltaBuffer * buf, SyncBombGameState & bomb, const SyncBombGameState & baseline ) {
	DeltaEnum( buf, bomb.attacking_team, baseline.attacking_team, Team_Count );
	Delta( buf, bomb.alpha_players_alive, baseline.alpha_players_alive );
	Delta( buf, bomb.alpha_players_total, baseline.alpha_players_total );
	Delta( buf, bomb.beta_players_alive, baseline.beta_players_alive );
	Delta( buf, bomb.beta_players_total, baseline.beta_players_total );
}

static void Delta( DeltaBuffer * buf, SyncGameState & state, const SyncGameState & baseline ) {
	DeltaEnum( buf, state.gametype, baseline.gametype, Gametype_Count );
	DeltaEnum( buf, state.match_state, baseline.match_state, MatchState_Count );
	Delta( buf, state.paused, baseline.paused );
	Delta( buf, state.match_state_start_time, baseline.match_state_start_time );
	Delta( buf, state.match_duration, baseline.match_duration );
	Delta( buf, state.clock_override, baseline.clock_override );
	Delta( buf, state.scorelimit, baseline.scorelimit );

	DeltaString( buf, state.callvote, baseline.callvote );
	Delta( buf, state.callvote_required_votes, baseline.callvote_required_votes );
	Delta( buf, state.callvote_yes_votes, baseline.callvote_yes_votes );

	Delta( buf, state.round_num, baseline.round_num );
	DeltaEnum( buf, state.round_state, baseline.round_state, RoundState_Count );
	DeltaEnum( buf, state.round_type, baseline.round_type, RoundType_Count );

	Delta( buf, state.teams, baseline.teams );
	Delta( buf, state.players, baseline.players );

	Delta( buf, state.map, baseline.map );

	Delta( buf, state.bomb, baseline.bomb );
	Delta( buf, state.exploding, baseline.exploding );
	Delta( buf, state.exploded_at, baseline.exploded_at );

	Delta( buf, state.sun_angles_from, baseline.sun_angles_from );
	Delta( buf, state.sun_angles_to, baseline.sun_angles_to );
	Delta( buf, state.sun_moved_from, baseline.sun_moved_from );
	Delta( buf, state.sun_moved_to, baseline.sun_moved_to );
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

[[maybe_unused]] DeltaBuffer ReaderFromWriter( const DeltaBuffer & writer ) {
	DeltaBuffer reader = {
		.buf = writer.buf,
		.cursor = writer.buf,
		.end = writer.cursor,
		.num_fields = writer.num_fields,
		.serializing = false,
	};

	memcpy( reader.field_mask, writer.field_mask, sizeof( writer.field_mask ) );

	return reader;
}

TEST( "Delta encoding" ) {
	struct DeltaTester {
		Optional< u32 > x;
		u32 y;
	};

	auto D = []( DeltaBuffer * buf, DeltaTester & x, const DeltaTester & baseline ) {
		Delta( buf, x.x, baseline.x );
		Delta( buf, x.y, baseline.y );
	};

	RNG rng = NewRNG();

	bool all_ok = true;

	for( int i = 0; i < 100; i++ ) {
		u8 buf[ 128 ];
		DeltaTester baseline = { };
		DeltaTester src = { .x = Probability( &rng, 0.5f ) ? NONE : MakeOptional( Random32( &rng ) ), .y = Random32( &rng ) };

		DeltaBuffer writer = DeltaWriter( buf, sizeof( buf ) );
		D( &writer, src, baseline );

		DeltaTester dst = baseline;
		DeltaBuffer reader = ReaderFromWriter( writer );
		D( &reader, dst, baseline );

		all_ok = all_ok && !writer.error && !reader.error && dst.x == src.x && dst.y == src.y;
	}

	return all_ok;
}
