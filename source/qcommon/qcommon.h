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

#pragma once

#include <stdarg.h>
#include <stdint.h>
#include <inttypes.h>

#include "gameshared/q_math.h"
#include "gameshared/q_shared.h"
#include "gameshared/q_collision.h"
#include "gameshared/gs_public.h"

#include "qcommon/application.h"
#include "qcommon/cmd.h"
#include "qcommon/cvar.h"
#include "qcommon/strtonum.h"

inline Vec3 FromQFAxis( const mat3_t m, int axis ) {
	return Vec3( m[ axis + 0 ], m[ axis + 1 ], m[ axis + 2 ] );
}

inline Mat4 FromAxisAndOrigin( const mat3_t axis, Vec3 origin ) {
	Mat4 rotation = Mat4::Identity();
	rotation.col0.x = axis[ 0 ];
	rotation.col0.y = axis[ 1 ];
	rotation.col0.z = axis[ 2 ];
	rotation.col1.x = axis[ 3 ];
	rotation.col1.y = axis[ 4 ];
	rotation.col1.z = axis[ 5 ];
	rotation.col2.x = axis[ 6 ];
	rotation.col2.y = axis[ 7 ];
	rotation.col2.z = axis[ 8 ];

	Mat4 translation = Mat4::Identity();
	translation.col3 = Vec4( origin, 1.0f );

	return translation * rotation;
}

//============================================================================

struct msg_t {
	uint8_t *data;
	size_t maxsize;
	size_t cursize;
	size_t readcount;
	bool compressed;
};

msg_t NewMSGReader( u8 * data, size_t n, size_t data_size );
msg_t NewMSGWriter( u8 * data, size_t n );
void MSG_Clear( msg_t * msg );
void *MSG_GetSpace( msg_t * msg, size_t length );
void MSG_Write( msg_t * msg, const void * data, size_t length );
void MSG_WriteZeroes( msg_t * msg, size_t n );
int MSG_SkipData( msg_t * msg, size_t length );

//============================================================================

struct UserCommand;

void MSG_WriteInt8( msg_t * msg, s8 x );
void MSG_WriteUint8( msg_t * msg, u8 x );
void MSG_WriteInt16( msg_t * msg, s16 x );
void MSG_WriteUint16( msg_t * msg, u16 x );
void MSG_WriteInt32( msg_t * msg, s32 x );
void MSG_WriteUint32( msg_t * msg, u32 x );
void MSG_WriteInt64( msg_t * msg, s64 x );
void MSG_WriteUint64( msg_t * msg, u64 x );
void MSG_WriteUintBase128( msg_t * msg, uint64_t c );
void MSG_WriteIntBase128( msg_t * msg, int64_t c );
void MSG_WriteString( msg_t * msg, const char *s );
void MSG_WriteDeltaUsercmd( msg_t * msg, const UserCommand * baseline , const UserCommand * cmd );
void MSG_WriteEntityNumber( msg_t * msg, int number, bool remove );
void MSG_WriteDeltaEntity( msg_t * msg, const SyncEntityState * baseline, const SyncEntityState * ent, bool force );
void MSG_WriteDeltaPlayerState( msg_t * msg, const SyncPlayerState * baseline, const SyncPlayerState * player );
void MSG_WriteDeltaGameState( msg_t * msg, const SyncGameState * baseline, const SyncGameState * state );
void MSG_WriteMsg( msg_t * msg, msg_t other );

void MSG_BeginReading( msg_t * msg );
s8 MSG_ReadInt8( msg_t * msg );
u8 MSG_ReadUint8( msg_t * msg );
s16 MSG_ReadInt16( msg_t * msg );
u16 MSG_ReadUint16( msg_t * msg );
s32 MSG_ReadInt32( msg_t * msg );
u32 MSG_ReadUint32( msg_t * msg );
s64 MSG_ReadInt64( msg_t * msg );
u64 MSG_ReadUint64( msg_t * msg );
uint64_t MSG_ReadUintBase128( msg_t * msg );
int64_t MSG_ReadIntBase128( msg_t * msg );
char *MSG_ReadString( msg_t * msg );
char *MSG_ReadStringLine( msg_t * msg );
void MSG_ReadDeltaUsercmd( msg_t * msg, const UserCommand * baseline, UserCommand * cmd );
int MSG_ReadEntityNumber( msg_t * msg, bool * remove );
void MSG_ReadDeltaEntity( msg_t * msg, const SyncEntityState * baseline, SyncEntityState * ent );
void MSG_ReadDeltaPlayerState( msg_t * msg, const SyncPlayerState * baseline, SyncPlayerState * player );
void MSG_ReadDeltaGameState( msg_t * msg, const SyncGameState * baseline, SyncGameState * state );
void MSG_ReadData( msg_t * msg, void *buffer, size_t length );
msg_t MSG_ReadMsg( msg_t * msg );

/*
==============================================================

PROTOCOL

==============================================================
*/

constexpr u16 PORT_MASTER = 27950;
constexpr u16 PORT_SERVER = 44400;

//=========================================

#define UPDATE_BACKUP   32  // copies of SyncEntityState to keep buffered
// must be power of two

#define UPDATE_MASK ( UPDATE_BACKUP - 1 )

//==================
// the svc_strings[] array in snapshot.c should mirror this
//==================
extern const char * const svc_strings[256];
void _SHOWNET( msg_t *msg, const char *s, int shownet );

//
// server to client
//
enum svc_ops_e {
	svc_servercmd,          // [string] string
	svc_serverdata,         // [int] protocol ...
	svc_spawnbaseline,
	svc_playerinfo,         // variable
	svc_packetentities,     // [...]
	svc_gamecommands,
	svc_match,
	svc_clcack,
	svc_servercs,           //tmp jalfixme : send reliable commands as unreliable
	svc_frame,
};

//==============================================

//
// client to server
//
enum clc_ops_e {
	clc_move,               // [[UserCommand]
	clc_svcack,
	clc_clientcommand,      // [string] message
};

//==============================================

// framesnap flags
#define FRAMESNAP_FLAG_DELTA        ( 1 << 0 )
#define FRAMESNAP_FLAG_ALLENTITIES  ( 1 << 1 )
#define FRAMESNAP_FLAG_MULTIPOV     ( 1 << 2 )

/*
==============================================================

NET

==============================================================
*/

#define MAX_RELIABLE_COMMANDS   64          // max string commands buffered for restransmit
#define MAX_PACKETLEN           1400        // max size of a network packet
#define MAX_MSGLEN              32768       // max length of a message, which may be fragmented into multiple packets

#include "qcommon/net.h"
#include "qcommon/net_chan.h"

// wsw: Medar: doubled the MSGLEN as a temporary solution for multiview on bigger servers
#define FRAGMENT_SIZE           ( MAX_PACKETLEN - 96 )
#define FRAGMENT_LAST       (    1 << 14 )
#define FRAGMENT_BIT            ( 1 << 31 )

/*
==============================================================

MISC

==============================================================
*/

#define MAX_PRINTMSG    3072

void Com_BeginRedirect( int target, char *buffer, int buffersize,
							   void ( *flush )( int, const char*, const void* ), const void *extra );
void Com_EndRedirect();

template< typename... Rest >
void Com_GGError( const char * fmt, const Rest & ... rest ) {
	char buf[ 4096 ];
	ggformat( buf, sizeof( buf ), fmt, rest... );
	Com_Error( "%s", buf );
}

void Com_DeferQuit();

connstate_t Com_ClientState();
void Com_SetClientState( connstate_t state );

bool Com_DemoPlaying();
void Com_SetDemoPlaying( bool state );

server_state_t Com_ServerState();
void Com_SetServerState( server_state_t state );

extern const bool is_dedicated_server;

void Qcommon_Init( int argc, char **argv );
bool Qcommon_Frame( unsigned int realMsec );
void Qcommon_Shutdown();

/*
==============================================================

NON-PORTABLE SYSTEM SERVICES

==============================================================
*/

void Sys_Init();
void ShowErrorMessage( const char * msg, const char * file, int line );

int64_t Sys_Milliseconds();
void Sys_Sleep( unsigned int millis );

const char * Sys_ConsoleInput();
void Sys_ConsoleOutput( const char * string );

bool Sys_OpenInWebBrowser( const char * url );

bool Sys_BeingDebugged();

/*
==============================================================

CLIENT / SERVER SYSTEMS

==============================================================
*/

void CL_Init();
void CL_Shutdown();
void CL_Frame( int realMsec, int gameMsec );
void CL_Disconnect( const char *message );
bool CL_DemoPlaying();

void Con_Print( const char *text );

void SV_Init();
void SV_Shutdown( const char *finalmsg );
void SV_ShutdownGame( const char *finalmsg, bool reconnect );
void SV_Frame( unsigned realMsec, unsigned gameMsec );
