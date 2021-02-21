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

#include "gameshared/q_arch.h"
#include "gameshared/q_math.h"
#include "gameshared/q_shared.h"
#include "gameshared/q_cvar.h"
#include "gameshared/q_collision.h"
#include "gameshared/gs_public.h"

#include "qcommon/application.h"
#include "qcommon/qfiles.h"
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

struct usercmd_t;

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
void MSG_WriteDeltaUsercmd( msg_t * msg, const usercmd_t * baseline , const usercmd_t * cmd );
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
void MSG_ReadDeltaUsercmd( msg_t * msg, const usercmd_t * baseline, usercmd_t * cmd );
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
void SNAP_BeginDemoRecording( int demofile, unsigned int spawncount, unsigned int snapFrameTime,
	unsigned int sv_bitflags, char *configstrings, SyncEntityState *baselines );
void SNAP_StopDemoRecording( int demofile );
void SNAP_WriteDemoMetaData( const char *filename, const char *meta_data, size_t meta_data_realsize );
size_t SNAP_ClearDemoMeta( char *meta_data, size_t meta_data_max_size );
size_t SNAP_SetDemoMetaKeyValue( char *meta_data, size_t meta_data_max_size, size_t meta_data_realsize,
								 const char *key, const char *value );
size_t SNAP_ReadDemoMetaData( int demofile, char *meta_data, size_t meta_data_size );

//============================================================================

int COM_Argc();
const char *COM_Argv( int arg );  // range and null checked
void COM_ClearArgv( int arg );
int COM_CheckParm( char *parm );
void COM_AddParm( char *parm );

void COM_Init();
void COM_InitArgv( int argc, char **argv );

// some hax, because we want to save the file and line where the copy was called
// from, not the file and line from ZoneCopyString function
char *_ZoneCopyString( const char *str, const char *filename, int fileline );
#define ZoneCopyString( str ) _ZoneCopyString( str, __FILE__, __LINE__ )

char *_TempCopyString( const char *str, const char *filename, int fileline );
#define TempCopyString( str ) _TempCopyString( str, __FILE__, __LINE__ )

int Com_GlobMatch( const char *pattern, const char *text, const bool casecmp );

void Info_Print( char *s );

/*
==============================================================

PROTOCOL

==============================================================
*/

// protocol.h -- communications protocols

//=========================================

#define PORT_MASTER         27950
#define PORT_SERVER         44400
#define NUM_BROADCAST_PORTS 5

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
	clc_move,               // [[usercmd_t]
	clc_svcack,
	clc_clientcommand,      // [string] message
};

//==============================================

// serverdata flags
#define SV_BITFLAGS_RELIABLE        ( 1 << 0 )
#define SV_BITFLAGS_HTTP            ( 1 << 1 )
#define SV_BITFLAGS_HTTP_BASEURL    ( 1 << 2 )

// framesnap flags
#define FRAMESNAP_FLAG_DELTA        ( 1 << 0 )
#define FRAMESNAP_FLAG_ALLENTITIES  ( 1 << 1 )
#define FRAMESNAP_FLAG_MULTIPOV     ( 1 << 2 )

/*
==============================================================

CMD

Command text buffering and command execution

==============================================================
*/

/*

Any number of commands can be added in a frame, from several different sources.
Most commands come from either keybindings or console line input, but remote
servers can also send across commands and entire text files can be execed.

The + command line options are also added to the command buffer.
*/

void Cbuf_Init();
void Cbuf_Shutdown();
void Cbuf_AddText( const char *text );
void Cbuf_ExecuteText( int exec_when, const char *text );
void Cbuf_AddEarlyCommands( bool clear );
bool Cbuf_AddLateCommands();
void Cbuf_Execute();


//===========================================================================

/*

Command execution takes a null terminated string, breaks it into tokens,
then searches for a command or variable that matches the first token.

*/

typedef void ( *xcommand_t )();
typedef const char ** ( *xcompletionf_t )( const char *partial );

void        Cmd_PreInit();
void        Cmd_Init();
void        Cmd_Shutdown();
void        Cmd_AddCommand( const char *cmd_name, xcommand_t function );
void        Cmd_RemoveCommand( const char *cmd_name );
bool    Cmd_Exists( const char *cmd_name );
bool    Cmd_CheckForCommand( char *text );
int         Cmd_CompleteAliasCountPossible( const char *partial );
const char  **Cmd_CompleteAliasBuildList( const char *partial );
int         Cmd_CompleteCountPossible( const char *partial );
const char  **Cmd_CompleteBuildList( const char *partial );
const char  **Cmd_CompleteBuildArgList( const char *partial );
const char  **Cmd_CompleteBuildArgListExt( const char *command, const char *arguments );
const char  **Cmd_CompleteFileList( const char *partial, const char *basedir, const char *extension, bool subdirectories );
int         Cmd_Argc();
const char  *Cmd_Argv( int arg );
char        *Cmd_Args();
void        Cmd_TokenizeString( const char *text );
void        Cmd_ExecuteString( const char *text );
void        Cmd_SetCompletionFunc( const char *cmd_name, xcompletionf_t completion_func );

/*
==============================================================

CVAR

==============================================================
*/

#include "cvar.h"

/*
==============================================================

NET

==============================================================
*/

// net.h -- quake's interface to the networking layer

#define PACKET_HEADER           10          // two ints, and a short

#define MAX_RELIABLE_COMMANDS   64          // max string commands buffered for restransmit
#define MAX_PACKETLEN           1400        // max size of a network packet
#define MAX_MSGLEN              32768       // max length of a message, which may be fragmented into multiple packets

// wsw: Medar: doubled the MSGLEN as a temporary solution for multiview on bigger servers
#define FRAGMENT_SIZE           ( MAX_PACKETLEN - 96 )
#define FRAGMENT_LAST       (    1 << 14 )
#define FRAGMENT_BIT            ( 1 << 31 )

enum netadrtype_t {
	NA_NOTRANSMIT,      // wsw : jal : fakeclients
	NA_LOOPBACK,
	NA_IP,
	NA_IP6,
};

struct netadr_ipv4_t {
	uint8_t ip [4];
	unsigned short port;
};

struct netadr_ipv6_t {
	uint8_t ip [16];
	unsigned short port;
	unsigned long scope_id;
};

struct netadr_t {
	netadrtype_t type;
	union {
		netadr_ipv4_t ipv4;
		netadr_ipv6_t ipv6;
	} address;
};

enum socket_type_t {
	SOCKET_LOOPBACK,
	SOCKET_UDP,
	SOCKET_TCP,
};

struct socket_t {
	bool open;

	socket_type_t type;
	netadr_t address;
	bool server;

	bool connected;
	netadr_t remoteAddress;

	socket_handle_t handle;
};

enum connection_status_t {
	CONNECTION_FAILED = -1,
	CONNECTION_INPROGRESS = 0,
	CONNECTION_SUCCEEDED = 1
};

enum net_error_t {
	NET_ERR_UNKNOWN = -1,
	NET_ERR_NONE = 0,

	NET_ERR_CONNRESET,
	NET_ERR_INPROGRESS,
	NET_ERR_MSGSIZE,
	NET_ERR_WOULDBLOCK,
	NET_ERR_UNSUPPORTED,
};

void        NET_Init();
void        NET_Shutdown();

bool        NET_OpenSocket( socket_t *socket, socket_type_t type, const netadr_t *address, bool server );
void        NET_CloseSocket( socket_t *socket );

bool        NET_Listen( const socket_t *socket );
int         NET_Accept( const socket_t *socket, socket_t *newsocket, netadr_t *address );

int         NET_GetPacket( const socket_t *socket, netadr_t *address, msg_t *message );
bool        NET_SendPacket( const socket_t *socket, const void *data, size_t length, const netadr_t *address );

int         NET_Get( const socket_t *socket, netadr_t *address, void *data, size_t length );
int         NET_Send( const socket_t *socket, const void *data, size_t length, const netadr_t *address );
int64_t     NET_SendFile( const socket_t *socket, int file, size_t offset, size_t count, const netadr_t *address );

void        NET_Sleep( int msec, socket_t *sockets[] );
int         NET_Monitor( int msec, socket_t *sockets[],
						 void ( *read_cb )( socket_t *socket, void* ),
						 void ( *write_cb )( socket_t *socket, void* ),
						 void ( *exception_cb )( socket_t *socket, void* ), void *privatep[] );
const char *NET_ErrorString();

#ifndef _MSC_VER
void NET_SetErrorString( const char *format, ... ) __attribute__( ( format( printf, 1, 2 ) ) );
#else
void NET_SetErrorString( _Printf_format_string_ const char *format, ... );
#endif

void        NET_SetErrorStringFromLastError( const char *function );

const char *NET_SocketTypeToString( socket_type_t type );
const char *NET_SocketToString( const socket_t *socket );
char       *NET_AddressToString( const netadr_t *address );
bool        NET_StringToAddress( const char *s, netadr_t *address );

unsigned short  NET_GetAddressPort( const netadr_t *address );
void            NET_SetAddressPort( netadr_t *address, unsigned short port );

bool    NET_CompareAddress( const netadr_t *a, const netadr_t *b );
bool    NET_CompareBaseAddress( const netadr_t *a, const netadr_t *b );
bool    NET_IsLANAddress( const netadr_t *address );
bool    NET_IsLocalAddress( const netadr_t *address );
void    NET_InitAddress( netadr_t *address, netadrtype_t type );
void    NET_BroadcastAddress( netadr_t *address, int port );

//============================================================================

struct netchan_t {
	const socket_t *socket;

	int dropped;                // between last packet and previous

	netadr_t remoteAddress;
	u64 session_id;

	// sequencing variables
	int incomingSequence;
	int incoming_acknowledged;
	int outgoingSequence;

	// incoming fragment assembly buffer
	int fragmentSequence;
	size_t fragmentLength;
	uint8_t fragmentBuffer[MAX_MSGLEN];

	// outgoing fragment buffer
	// we need to space out the sending of large fragmented messages
	bool unsentFragments;
	size_t unsentFragmentStart;
	size_t unsentLength;
	uint8_t unsentBuffer[MAX_MSGLEN];
	bool unsentIsCompressed;
};

extern netadr_t net_from;


void Netchan_Init();
void Netchan_Shutdown();
void Netchan_Setup( netchan_t *chan, const socket_t *socket, const netadr_t *address, u64 session_id );
bool Netchan_Process( netchan_t *chan, msg_t *msg );
bool Netchan_Transmit( netchan_t *chan, msg_t *msg );
bool Netchan_PushAllFragments( netchan_t *chan );
bool Netchan_TransmitNextFragment( netchan_t *chan );
int Netchan_CompressMessage( msg_t *msg );
int Netchan_DecompressMessage( msg_t *msg );
void Netchan_OutOfBand( const socket_t *socket, const netadr_t *address, size_t length, const uint8_t *data );

#ifndef _MSC_VER
void Netchan_OutOfBandPrint( const socket_t *socket, const netadr_t *address, const char *format, ... ) __attribute__( ( format( printf, 3, 4 ) ) );
#else
void Netchan_OutOfBandPrint( const socket_t *socket, const netadr_t *address, _Printf_format_string_ const char *format, ... );
#endif

u64 Netchan_ClientSessionID();

/*
==============================================================

FILESYSTEM

==============================================================
*/

void        FS_Init();
void        FS_Frame();
void        FS_Shutdown();

const char *FS_GameDirectory();
const char *FS_BaseGameDirectory();

// handling of absolute filenames
// only to be used if necessary (library not supporting custom file handling functions etc.)
const char *FS_WriteDirectory();
const char *FS_DownloadsDirectory();
void        FS_CreateAbsolutePath( const char *path );
const char *FS_AbsoluteNameForFile( const char *filename );
const char *FS_AbsoluteNameForBaseFile( const char *filename );

// // game and base files
// file streaming
int     FS_FOpenFile( const char *filename, int *filenum, int mode );
int     FS_FOpenBaseFile( const char *filename, int *filenum, int mode );
int     FS_FOpenAbsoluteFile( const char *filename, int *filenum, int mode );
void    FS_FCloseFile( int file );

int     FS_Read( void *buffer, size_t len, int file );
int     FS_Print( int file, const char *msg );

#ifndef _MSC_VER
int FS_Printf( int file, const char *format, ... ) __attribute__( ( format( printf, 2, 3 ) ) );
#else
int FS_Printf( int file, _Printf_format_string_ const char *format, ... );
#endif

int     FS_Write( const void *buffer, size_t len, int file );
int     FS_Tell( int file );
int     FS_Seek( int file, int offset, int whence );
int     FS_Flush( int file );
int     FS_FileNo( int file );

void    FS_SetCompressionLevel( int file, int level );
int     FS_GetCompressionLevel( int file );

// file loading
int     FS_LoadFileExt( const char *path, int flags, void **buffer, void *stack, size_t stackSize, const char *filename, int fileline );
int     FS_LoadBaseFileExt( const char *path, int flags, void **buffer, void *stack, size_t stackSize, const char *filename, int fileline );
void    FS_FreeFile( void *buffer );
void    FS_FreeBaseFile( void *buffer );
#define FS_LoadFile( path,buffer,stack,stacksize ) FS_LoadFileExt( path,0,buffer,stack,stacksize,__FILE__,__LINE__ )
#define FS_LoadBaseFile( path,buffer,stack,stacksize ) FS_LoadBaseFileExt( path,0,buffer,stack,stacksize,__FILE__,__LINE__ )

// util functions
bool    FS_MoveFile( const char *src, const char *dst );
bool    FS_MoveBaseFile( const char *src, const char *dst );
bool    FS_RemoveFile( const char *filename );
bool    FS_RemoveBaseFile( const char *filename );
bool    FS_RemoveAbsoluteFile( const char *filename );
unsigned    FS_ChecksumAbsoluteFile( const char *filename );
unsigned    FS_ChecksumBaseFile( const char *filename );

// // only for game files
const char *FS_BaseNameForFile( const char *filename );

int         FS_GetFileList( const char *dir, const char *extension, char *buf, size_t bufsize, int start, int end );
int         FS_GetFileListExt( const char *dir, const char *extension, char *buf, size_t *bufsize, int start, int end );

/*
==============================================================

MISC

==============================================================
*/

#define MAX_PRINTMSG    3072

void        Com_BeginRedirect( int target, char *buffer, int buffersize,
							   void ( *flush )( int, const char*, const void* ), const void *extra );
void        Com_EndRedirect();
void        Com_DeferConsoleLogReopen();

#ifndef _MSC_VER
void Com_Printf( const char *format, ... ) __attribute__( ( format( printf, 1, 2 ) ) );
void Com_DPrintf( const char *format, ... ) __attribute__( ( format( printf, 1, 2 ) ) );
void Com_Error( com_error_code_t code, const char *format, ... ) __attribute__( ( format( printf, 2, 3 ) ) ) __attribute__( ( noreturn ) );
void Com_Quit() __attribute__( ( noreturn ) );
#else
void Com_Printf( _Printf_format_string_ const char *format, ... );
void Com_DPrintf( _Printf_format_string_ const char *format, ... );
__declspec( noreturn ) void Com_Error( com_error_code_t code, _Printf_format_string_ const char *format, ... );
__declspec( noreturn ) void Com_Quit();
#endif

template< typename... Rest >
void Com_GGPrintNL( const char * fmt, const Rest & ... rest ) {
	char buf[ 4096 ];
	ggformat( buf, sizeof( buf ), fmt, rest... );
	Com_Printf( "%s", buf );
}

#define Com_GGPrint( fmt, ... ) Com_GGPrintNL( fmt "\n", ##__VA_ARGS__ )

template< typename... Rest >
void Com_GGError( com_error_code_t code, const char * fmt, const Rest & ... rest ) {
	char buf[ 4096 ];
	ggformat( buf, sizeof( buf ), fmt, rest... );
	Com_Error( code, "%s", buf );
}

void        Com_DeferQuit();

int         Com_ClientState();        // this should have just been a cvar...
void        Com_SetClientState( int state );

bool		Com_DemoPlaying();
void        Com_SetDemoPlaying( bool state );

int         Com_ServerState();        // this should have just been a cvar...
void        Com_SetServerState( int state );

extern cvar_t *developer;
extern const bool is_dedicated_server;
extern cvar_t *versioncvar;

/*
==============================================================

MEMORY MANAGEMENT

==============================================================
*/

struct mempool_t;

#define MEMPOOL_TEMPORARY           1
#define MEMPOOL_GAME                2
#define MEMPOOL_CLIENTGAME          4

void Memory_Init();
void Memory_InitCommands();
void Memory_Shutdown();
void Memory_ShutdownCommands();

ATTRIBUTE_MALLOC void *_Mem_AllocExt( mempool_t *pool, size_t size, size_t aligment, int z, int musthave, int canthave, const char *filename, int fileline );
ATTRIBUTE_MALLOC void *_Mem_Alloc( mempool_t *pool, size_t size, int musthave, int canthave, const char *filename, int fileline );
void *_Mem_Realloc( void *data, size_t size, const char *filename, int fileline );
void _Mem_Free( void *data, int musthave, int canthave, const char *filename, int fileline );
mempool_t *_Mem_AllocPool( mempool_t *parent, const char *name, int flags, const char *filename, int fileline );
mempool_t *_Mem_AllocTempPool( const char *name, const char *filename, int fileline );
void _Mem_FreePool( mempool_t **pool, int musthave, int canthave, const char *filename, int fileline );
void _Mem_EmptyPool( mempool_t *pool, int musthave, int canthave, const char *filename, int fileline );
char *_Mem_CopyString( mempool_t *pool, const char *in, const char *filename, int fileline );

void _Mem_CheckSentinels( void *data, const char *filename, int fileline );
void _Mem_CheckSentinelsGlobal( const char *filename, int fileline );

size_t Mem_PoolTotalSize( mempool_t *pool );

#define Mem_AllocExt( pool, size, z ) _Mem_AllocExt( pool, size, 0, z, 0, 0, __FILE__, __LINE__ )
#define Mem_Alloc( pool, size ) _Mem_Alloc( pool, size, 0, 0, __FILE__, __LINE__ )
#define Mem_Realloc( data, size ) _Mem_Realloc( data, size, __FILE__, __LINE__ )
#define Mem_Free( mem ) _Mem_Free( mem, 0, 0, __FILE__, __LINE__ )
#define Mem_AllocPool( parent, name ) _Mem_AllocPool( parent, name, 0, __FILE__, __LINE__ )
#define Mem_AllocTempPool( name ) _Mem_AllocTempPool( name, __FILE__, __LINE__ )
#define Mem_FreePool( pool ) _Mem_FreePool( pool, 0, 0, __FILE__, __LINE__ )
#define Mem_EmptyPool( pool ) _Mem_EmptyPool( pool, 0, 0, __FILE__, __LINE__ )
#define Mem_CopyString( pool, str ) _Mem_CopyString( pool, str, __FILE__, __LINE__ )

#define Mem_CheckSentinels( data ) _Mem_CheckSentinels( data, __FILE__, __LINE__ )
#define Mem_CheckSentinelsGlobal() _Mem_CheckSentinelsGlobal( __FILE__, __LINE__ )
#ifdef NDEBUG
#define Mem_DebugCheckSentinelsGlobal()
#else
#define Mem_DebugCheckSentinelsGlobal() _Mem_CheckSentinelsGlobal( __FILE__, __LINE__ )
#endif

// used for temporary allocations
extern mempool_t *tempMemPool;
extern mempool_t *zoneMemPool;

#define Mem_ZoneMallocExt( size, z ) Mem_AllocExt( zoneMemPool, size, z )
#define Mem_ZoneMalloc( size ) Mem_Alloc( zoneMemPool, size )
#define Mem_ZoneFree( data ) Mem_Free( data )

#define Mem_TempMallocExt( size, z ) Mem_AllocExt( tempMemPool, size, z )
#define Mem_TempMalloc( size ) Mem_Alloc( tempMemPool, size )
#define Mem_TempFree( data ) Mem_Free( data )

void *Q_malloc( size_t size );
void *Q_realloc( void *buf, size_t newsize );
void Q_free( void *buf );

void Qcommon_Init( int argc, char **argv );
void Qcommon_Frame( unsigned int realMsec );
void Qcommon_Shutdown();

/*
==============================================================

NON-PORTABLE SYSTEM SERVICES

==============================================================
*/

// directory searching
#define SFF_ARCH    0x01
#define SFF_HIDDEN  0x02
#define SFF_RDONLY  0x04
#define SFF_SUBDIR  0x08
#define SFF_SYSTEM  0x10

void Sys_Init();
void Sys_ShowErrorMessage( const char * msg );

int64_t Sys_Milliseconds();
uint64_t Sys_Microseconds();
void Sys_Sleep( unsigned int millis );
bool Sys_FormatTime( char * buf, size_t buf_size, const char * fmt );

const char * Sys_ConsoleInput();
void Sys_ConsoleOutput( const char * string );

bool Sys_OpenInWebBrowser( const char * url );

bool Sys_BeingDebugged();

#ifndef _MSC_VER
void Sys_Error( const char *error, ... ) __attribute__( ( format( printf, 1, 2 ) ) ) __attribute__( ( noreturn ) );
void Sys_Quit() __attribute__( ( noreturn ) );
#else
__declspec( noreturn ) void Sys_Error( _Printf_format_string_ const char *error, ... );
__declspec( noreturn ) void Sys_Quit();
#endif

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

/*
==============================================================

MAPLIST SUBSYSTEM

==============================================================
*/

void InitMapList();
void ShutdownMapList();

void RefreshMapList();
Span< const char * > GetMapList();
bool MapExists( const char * name );

const char ** CompleteMapName( const char * prefix );
