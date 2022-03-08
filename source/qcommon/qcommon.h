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

struct CollisionModel;

//============================================================================

struct msg_t {
	uint8_t *data;
	size_t maxsize;
	size_t cursize;
	size_t readcount;
	bool compressed;
};

// msg.c
void MSG_Init( msg_t *buf, uint8_t *data, size_t length );
void MSG_Clear( msg_t *buf );
void *MSG_GetSpace( msg_t *buf, size_t length );
void MSG_WriteData( msg_t *msg, const void *data, size_t length );
void MSG_CopyData( msg_t *buf, const void *data, size_t length );
int MSG_SkipData( msg_t *sb, size_t length );

//============================================================================

struct UserCommand;

void MSG_WriteInt8( msg_t *sb, int c );
void MSG_WriteUint8( msg_t *sb, int c );
void MSG_WriteInt16( msg_t *sb, int c );
void MSG_WriteUint16( msg_t *sb, unsigned c );
void MSG_WriteInt32( msg_t *sb, int c );
void MSG_WriteInt64( msg_t *sb, int64_t c );
void MSG_WriteUint64( msg_t *sb, uint64_t c );
void MSG_WriteUintBase128( msg_t *msg, uint64_t c );
void MSG_WriteIntBase128( msg_t *msg, int64_t c );
void MSG_WriteString( msg_t *sb, const char *s );
void MSG_WriteDeltaUsercmd( msg_t * msg, const UserCommand * baseline , const UserCommand * cmd );
void MSG_WriteEntityNumber( msg_t * msg, int number, bool remove );
void MSG_WriteDeltaEntity( msg_t * msg, const SyncEntityState * baseline, const SyncEntityState * ent, bool force );
void MSG_WriteDeltaPlayerState( msg_t * msg, const SyncPlayerState * baseline, const SyncPlayerState * player );
void MSG_WriteDeltaGameState( msg_t * msg, const SyncGameState * baseline, const SyncGameState * state );

void MSG_BeginReading( msg_t *sb );
int MSG_ReadInt8( msg_t *msg );
int MSG_ReadUint8( msg_t *msg );
int16_t MSG_ReadInt16( msg_t *sb );
uint16_t MSG_ReadUint16( msg_t *sb );
int MSG_ReadInt32( msg_t *sb );
int64_t MSG_ReadInt64( msg_t *sb );
uint64_t MSG_ReadUint64( msg_t *sb );
uint64_t MSG_ReadUintBase128( msg_t *msg );
int64_t MSG_ReadIntBase128( msg_t *msg );
char *MSG_ReadString( msg_t *sb );
char *MSG_ReadStringLine( msg_t *sb );
void MSG_ReadDeltaUsercmd( msg_t * msg, const UserCommand * baseline, UserCommand * cmd );
int MSG_ReadEntityNumber( msg_t * msg, bool * remove );
void MSG_ReadDeltaEntity( msg_t * msg, const SyncEntityState * baseline, SyncEntityState * ent );
void MSG_ReadDeltaPlayerState( msg_t * msg, const SyncPlayerState * baseline, SyncPlayerState * player );
void MSG_ReadDeltaGameState( msg_t * msg, const SyncGameState * baseline, SyncGameState * state );
void MSG_ReadData( msg_t *sb, void *buffer, size_t length );

//============================================================================

#define SNAP_MAX_DEMO_META_DATA_SIZE    16 * 1024

// define this 0 to disable compression of demo files
#define SNAP_DEMO_GZ                    FS_GZ

void SNAP_RecordDemoMessage( int demofile, msg_t *msg, int offset );
int SNAP_ReadDemoMessage( int demofile, msg_t *msg );
void SNAP_BeginDemoRecording( TempAllocator * temp, int demofile, unsigned int spawncount, unsigned int snapFrameTime,
	const char *configstrings, SyncEntityState *baselines );
void SNAP_StopDemoRecording( int demofile );
void SNAP_WriteDemoMetaData( const char *filename, const char *meta_data, size_t meta_data_realsize );
size_t SNAP_SetDemoMetaKeyValue( char *meta_data, size_t meta_data_max_size, size_t meta_data_realsize,
								 const char *key, const char *value );

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
	svc_demoinfo,
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

FILESYSTEM

==============================================================
*/

void        FS_Init();
void        FS_Shutdown();

// // game and base files
// file streaming
int     FS_FOpenAbsoluteFile( const char *filename, int *filenum, int mode );
void    FS_FCloseFile( int file );

int     FS_Read( void *buffer, size_t len, int file );

int     FS_Write( const void *buffer, size_t len, int file );
int     FS_Seek( int file, int offset, int whence );
int     FS_Flush( int file );

void    FS_SetCompressionLevel( int file, int level );
int     FS_GetCompressionLevel( int file );

/*
==============================================================

MISC

==============================================================
*/

#define MAX_PRINTMSG    3072

void Com_BeginRedirect( int target, char *buffer, int buffersize,
							   void ( *flush )( int, const char*, const void* ), const void *extra );
void Com_EndRedirect();

#ifndef _MSC_VER
void Com_Printf( const char *format, ... ) __attribute__( ( format( printf, 1, 2 ) ) );
void Com_Error( const char *format, ... ) __attribute__( ( format( printf, 1, 2 ) ) );
#else
void Com_Printf( _Printf_format_string_ const char *format, ... );
void Com_Error( _Printf_format_string_ const char *format, ... );
#endif

template< typename... Rest >
void Com_GGPrintNL( const char * fmt, const Rest & ... rest ) {
	char buf[ 4096 ];
	ggformat( buf, sizeof( buf ), fmt, rest... );
	Com_Printf( "%s", buf );
}

#define Com_GGPrint( fmt, ... ) Com_GGPrintNL( fmt "\n", ##__VA_ARGS__ )

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
uint64_t Sys_Microseconds();
void Sys_Sleep( unsigned int millis );
bool Sys_FormatTimestamp( char * buf, size_t buf_size, const char * fmt, s64 time );
bool Sys_FormatCurrentTime( char * buf, size_t buf_size, const char * fmt );

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
void CL_Disconnect( const char *message );
void CL_Shutdown();
void CL_Frame( int realMsec, int gameMsec );
void Con_Print( const char *text );

void SV_Init();
void SV_Shutdown( const char *finalmsg );
void SV_ShutdownGame( const char *finalmsg, bool reconnect );
void SV_Frame( unsigned realMsec, unsigned gameMsec );
