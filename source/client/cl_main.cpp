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

#include "qcommon/base.h"
#include "client/client.h"
#include "client/assets.h"
#include "client/audio/api.h"
#include "client/clay_init.h"
#include "client/discord.h"
#include "client/downloads.h"
#include "client/gltf.h"
#include "client/keys.h"
#include "client/demo_browser.h"
#include "client/server_browser.h"
#include "client/livepp.h"
#include "client/renderer/renderer.h"
#include "client/renderer/text.h"
#include "server/server.h"
#include "qcommon/compression.h"
#include "qcommon/csprng.h"
#include "qcommon/hash.h"
#include "qcommon/fs.h"
#include "qcommon/string.h"
#include "qcommon/threadpool.h"
#include "qcommon/time.h"
#include "qcommon/version.h"
#include "gameshared/gs_public.h"

Cvar *cl_timeout;
Cvar *cl_maxfps;
Cvar *cl_pps;
Cvar *cl_shownet;

Cvar *cl_extrapolationTime;
Cvar *cl_extrapolate;

static Cvar *cl_hotloadAssets;

// wsw : debug netcode
Cvar *cl_debug_serverCmd;
Cvar *cl_debug_timeDelta;

Cvar *cl_devtools;

Cvar *cg_showFPS;

client_static_t cls;
client_state_t cl;

SyncEntityState cl_baselines[MAX_EDICTS];

static bool cl_initialized = false;

/*
=======================================================================

CLIENT RELIABLE COMMAND COMMUNICATION

=======================================================================
*/

msg_t * CL_AddReliableCommand( ClientCommandType command ) {
	// if we would be losing an old command that hasn't been acknowledged,
	// we must drop the connection
	if( cls.reliableSequence > cls.reliableAcknowledge + MAX_RELIABLE_COMMANDS ) {
		cls.reliableAcknowledge = cls.reliableSequence; // try to avoid loops
		Com_Error( "Client command overflow %zu %zu", cls.reliableAcknowledge, cls.reliableSequence );
	}

	cls.reliableSequence++;

	size_t index = cls.reliableSequence % ARRAY_COUNT( cls.reliableCommands );
	cls.reliableCommands[ index ] = { };
	cls.reliableCommands[ index ].command = command;
	cls.reliableCommands[ index ].args = NewMSGWriter( cls.reliableCommands[ index ].args_buf, sizeof( cls.reliableCommands[ index ].args_buf ) );
	return &cls.reliableCommands[ index ].args;
}

static void SerializeReliableCommands( msg_t * msg ) {
	for( size_t i = cls.reliableAcknowledge + 1; i <= cls.reliableSequence; i++ ) {
		size_t index = i % ARRAY_COUNT( cls.reliableCommands );
		MSG_WriteUint8( msg, clc_clientcommand );
		MSG_WriteIntBase128( msg, i );
		MSG_WriteUint8( msg, cls.reliableCommands[ index ].command );
		MSG_WriteMsg( msg, cls.reliableCommands[ index ].args );
	}

	cls.reliableSent = cls.reliableSequence;
}

void CL_ServerDisconnect_f( const Tokenized & args ) {
	CL_Disconnect( NULL );
	Com_GGPrint( "Connection was closed by server: {}", args.tokens[ 1 ] );
}

/*
* CL_SendConnectPacket
*
* We have gotten a challenge from the server, so try and
* connect.
*/
static void CL_SendConnectPacket() {
	userinfo_modified = false;

	TempAllocator temp = cls.frame_arena.temp();
	Netchan_OutOfBandPrint( cls.socket, cls.serveraddress, "%s",
		temp( "connect {} {} {} \"{}\"\n", APP_PROTOCOL_VERSION, cls.session_id, cls.challenge, Cvar_GetUserInfo() ) );
}

/*
* CL_CheckForResend
*
* Resend a connect message if the last one has timed out
*/
static void CL_CheckForResend() {
	if( CL_DemoPlaying() ) {
		return;
	}

	// if the local server is running and we aren't then connect
	if( cls.state == CA_DISCONNECTED && Com_ServerState() ) {
		CL_Connect( GetLoopbackAddress( sv_port->integer ) );
	}

	// resend if we haven't gotten a reply yet
	if( cls.state == CA_CONNECTING ) {
		if( cls.connect_time.exists && cls.monotonicTime - cls.connect_time.value < Seconds( 3 ) ) {
			return;
		}
		if( cls.connect_count > 3 ) {
			CL_Disconnect( "Connection timed out" );
			return;
		}
		cls.connect_count++;
		cls.connect_time = cls.monotonicTime;

		Com_GGPrint( "Connecting to {}...", cls.serveraddress );

		Netchan_OutOfBandPrint( cls.socket, cls.serveraddress, "getchallenge\n" );
	}
}

void CL_Connect( const NetAddress & address ) {
	CL_Disconnect( NULL );

	cls.serveraddress = address;

	CL_SetClientState( CA_CONNECTING );

	cls.connect_time = NONE; // CL_CheckForResend() will fire immediately
	cls.connect_count = 0;
	cls.rejected = false;
	cls.lastPacketReceivedTime = cls.realtime; // reset the timeout limit
}

static void CL_Connect_f( const Tokenized & args ) {
	if( args.tokens.n < 2 ) {
		Com_Printf( "Usage: connect <address>\n" );
		return;
	}

	TempAllocator temp = cls.frame_arena.temp();

	Span< const char > arg = StripPrefix( args.tokens[ 1 ], APP_URI_SCHEME );

	const char * at = StrChr( arg, '@' );
	if( at != NULL ) {
		Span< const char > password = arg.slice( 0, at - arg.ptr );
		Cvar_Set( "password", temp.sv( "{}", password ) );
		arg += password.n + 1;
	}

	u16 port;
	Span< const char > hostname = SplitIntoHostnameAndPort( arg, &port );

	NetAddress address;
	if( !DNS( temp( "{}", hostname ), &address ) ) {
		Com_Printf( "Bad server address\n" );
		return;
	}
	address.port = port == 0 ? PORT_SERVER : port;

	CL_Connect( address );
}

void CL_ClearState() {
	TracyZoneScoped;

	// wipe the entire cl structure
	memset( &cl, 0, sizeof( client_state_t ) );
	memset( cl_baselines, 0, sizeof( cl_baselines ) );

	//userinfo_modified = true;
	cls.lastExecutedServerCommand = 0;
	cls.reliableAcknowledge = 0;
	cls.reliableSequence = 0;
	cls.reliableSent = 0;
	memset( cls.reliableCommands, 0, sizeof( cls.reliableCommands ) );
	// reset ucmds buffer
	cls.ucmdHead = 0;
	cls.ucmdSent = 0;
	cls.ucmdAcknowledged = 0;

	//restart realtime and lastPacket times
	cls.game_time = { };
	cls.realtime = 0;
	cls.gametime = 0;
	cls.lastPacketSentTime = 0;
	cls.lastPacketReceivedTime = 0;
}

/*
* CL_Disconnect_SendCommand
*
* Sends a disconnect message to the server
*/
static void CL_Disconnect_SendCommand() {
	// wsw : jal : send the packet 3 times to make sure isn't lost
	CL_AddReliableCommand( ClientCommand_Disconnect );
	CL_SendMessagesToServer( true );
	CL_AddReliableCommand( ClientCommand_Disconnect );
	CL_SendMessagesToServer( true );
	CL_AddReliableCommand( ClientCommand_Disconnect );
	CL_SendMessagesToServer( true );
}

/*
* CL_Disconnect
*
* Goes from a connected state to full screen console state
* Sends a disconnect message to the server
* This is also called on Com_Error, so it shouldn't cause any errors
*/
void CL_Disconnect( const char *message ) {
	CancelDownload();

	if( cls.state == CA_UNINITIALIZED ) {
		return;
	}
	if( cls.state == CA_DISCONNECTED ) {
		return;
	}

	SV_ShutdownGame( "Owner left the listen server", false );

	cls.connect_time = NONE;
	cls.connect_count = 0;
	cls.rejected = false;

	CL_StopRecording( true );

	if( CL_DemoPlaying() ) {
		CL_DemoCompleted();
	} else {
		CL_Disconnect_SendCommand(); // send a disconnect message to the server
	}

	Free( sys_allocator, cls.server_name.ptr );
	cls.server_name = Span< char >();

	Free( sys_allocator, cls.download_url.ptr );
	cls.download_url = Span< char >();

	StopAllSounds( false );

	CL_GameModule_Shutdown();

	CL_ClearState();
	CL_SetClientState( CA_DISCONNECTED );

	if( message != NULL ) {
		TempAllocator temp = cls.frame_arena.temp();
		Cmd_Execute( &temp, "menu_open connfailed rejectmessage \"{}\"", message );
	}
}

/*
* CL_Changing_f
*
* Just sent as a hint to the client that they should
* drop to full console
*/
void CL_Changing_f() {
	CL_StopRecording( true );

	// ignore snapshots from previous connection
	cl.pendingSnapNum = cl.currentSnapNum = cl.receivedSnapNum = 0;

	CL_SetClientState( CA_CONNECTED ); // not active anymore, but not disconnected
}

/*
* CL_ServerReconnect_f
*
* The server is changing levels
*/
void CL_ServerReconnect_f() {
	if( CL_DemoPlaying() ) {
		return;
	}

	if( cls.state < CA_CONNECTED ) {
		Com_Printf( "Error: CL_ServerReconnect_f while not connected\n" );
		return;
	}

	CancelDownload();

	CL_StopRecording( true );

	cls.connect_count = 0;
	cls.rejected = false;

	CL_GameModule_Shutdown();
	StopAllSounds( true );

	Com_Printf( "Reconnecting...\n" );

	cls.connect_time = cls.monotonicTime;

	CL_SetClientState( CA_HANDSHAKE );
	CL_AddReliableCommand( ClientCommand_New );
}

void CL_Reconnect_f() {
	if( cls.serveraddress == NULL_ADDRESS ) {
		Com_Printf( "Can't reconnect, never connected\n" );
		return;
	}

	CL_Disconnect( NULL );
	CL_Connect( cls.serveraddress );
}

/*
* CL_ConnectionlessPacket
*
* Responses to broadcasts, etc
*/
static void CL_ConnectionlessPacket( const NetAddress & address, msg_t * msg ) {
	MSG_BeginReading( msg );
	MSG_ReadInt32( msg ); // skip the -1

	const char * s = MSG_ReadStringLine( msg );

	if( StartsWith( s, "getserversResponse" ) ) {
		ParseMasterServerResponse( msg, false );
		return;
	}

	if( StartsWith( s, "getserversExtResponse" ) ) {
		ParseMasterServerResponse( msg, true );
		return;
	}

	TempAllocator temp = cls.frame_arena.temp();
	Tokenized args = Tokenize( &temp, MakeSpan( s ) );

	if( args.tokens.n == 0 ) {
		Assert( is_public_build );
		return;
	}

	if( args.tokens[ 0 ] == "info" ) {
		ParseGameServerResponse( msg, address );
		return;
	}

	if( CL_DemoPlaying() ) {
		return;
	}

	// server connection
	if( args.tokens[ 0 ] == "client_connect" ) {
		if( cls.state == CA_CONNECTED ) {
			Com_Printf( "Dup connect received. Ignored.\n" );
			return;
		}
		// these two are from Q3
		if( cls.state != CA_CONNECTING ) {
			Com_Printf( "client_connect packet while not connecting. Ignored.\n" );
			return;
		}
		if( address != cls.serveraddress ) {
			Assert( is_public_build );
			return;
		}

		cls.rejected = false;

		Netchan_Setup( &cls.netchan, address, cls.session_id );
		CL_SetClientState( CA_HANDSHAKE );
		CL_AddReliableCommand( ClientCommand_New );
		return;
	}

	// reject packet, used to inform the client that connection attemp didn't succeed
	if( args.tokens[ 0 ] == "reject" ) {
		int rejectflag;

		if( cls.state != CA_CONNECTING ) {
			Com_Printf( "reject packet while not connecting, ignored\n" );
			return;
		}
		if( address != cls.serveraddress ) {
			Assert( is_public_build );
			return;
		}

		cls.rejected = true;

		rejectflag = atoi( MSG_ReadStringLine( msg ) );

		SafeStrCpy( cls.rejectmessage, MSG_ReadStringLine( msg ), sizeof( cls.rejectmessage ) );
		if( strlen( cls.rejectmessage ) > sizeof( cls.rejectmessage ) - 2 ) {
			cls.rejectmessage[strlen( cls.rejectmessage ) - 2] = '.';
			cls.rejectmessage[strlen( cls.rejectmessage ) - 1] = '.';
			cls.rejectmessage[strlen( cls.rejectmessage )] = '.';
		}

		Com_Printf( "Connection refused: %s\n", cls.rejectmessage );
		if( rejectflag & DROP_FLAG_AUTORECONNECT ) {
			Com_Printf( "Automatic reconnecting allowed.\n" );
		}
		else {
			Com_Printf( "Automatic reconnecting not allowed.\n" );

			CL_Disconnect( NULL );
			Cmd_Execute( &temp, "menu_open connfailed rejectmessage \"{}\"", cls.rejectmessage );
		}

		return;
	}

	// challenge from the server we are connecting to
	if( args.tokens[ 0 ] == "challenge" ) {
		// these two are from Q3
		if( cls.state != CA_CONNECTING ) {
			Com_Printf( "challenge packet while not connecting, ignored\n" );
			Assert( is_public_build );
			return;
		}
		if( args.tokens.n != 2 || address != cls.serveraddress ) {
			Assert( is_public_build );
			return;
		}

		cls.challenge = SpanToInt( args.tokens[ 1 ], 0 );
		//wsw : r1q2[start]
		//r1: reset the timer so we don't send dup. getchallenges
		cls.connect_time = cls.monotonicTime;
		//wsw : r1q2[end]
		CL_SendConnectPacket();
		return;
	}

	Assert( is_public_build );
}

static bool CL_ProcessPacket( netchan_t * netchan, msg_t * msg ) {
	if( !Netchan_Process( netchan, msg ) ) {
		return false; // wasn't accepted for some reason
	}

	// now if compressed, expand it
	MSG_BeginReading( msg );
	MSG_ReadInt32( msg ); // sequence
	MSG_ReadInt32( msg ); // sequence_ack
	MSG_ReadUint64( msg ); // session_id

	if( msg->compressed ) {
		return Netchan_DecompressMessage( msg );
	}

	return true;
}

void CL_ReadPackets() {
	uint8_t data[ MAX_MSGLEN ];
	NetAddress source;
	size_t bytes_received = UDPReceive( cls.socket, &source, data, sizeof( data ) );
	if( bytes_received == 0 ) {
		return;
	}

	msg_t msg = NewMSGReader( data, bytes_received, sizeof( data ) );

	// remote command packet
	if( MSG_ReadInt32( &msg ) == -1 ) {
		CL_ConnectionlessPacket( source, &msg );
		return;
	}

	if( CL_DemoPlaying() ) {
		// only allow connectionless packets during demo playback
		return;
	}

	if( cls.state == CA_DISCONNECTED || cls.state == CA_CONNECTING ) {
		return;
	}

	if( msg.cursize < 8 ) {
		return;
	}

	if( source != cls.netchan.remoteAddress ) {
		Assert( is_public_build );
		return;
	}

	if( !CL_ProcessPacket( &cls.netchan, &msg ) ) {
		return; // wasn't accepted for some reason, like only one fragment of bigger message
	}

	CL_ParseServerMessage( &msg );
	cls.lastPacketReceivedTime = cls.realtime;

	// not expected, but could happen if cls.realtime is cleared and lastPacketReceivedTime is not
	if( cls.lastPacketReceivedTime > cls.realtime ) {
		cls.lastPacketReceivedTime = cls.realtime;
	}

	// check timeout
	if( cls.state >= CA_HANDSHAKE && cls.lastPacketReceivedTime ) {
		if( cls.lastPacketReceivedTime + cl_timeout->number * 1000 < cls.realtime ) {
			if( ++cl.timeoutcount > 5 ) { // timeoutcount saves debugger
				Com_Printf( "\nServer connection timed out.\n" );
				CL_Disconnect( "Connection timed out" );
				return;
			}
		}
	} else {
		cl.timeoutcount = 0;
	}
}

//=============================================================================

static int precache_spawncount;

void CL_FinishConnect() {
	TempAllocator temp = cls.frame_arena.temp();

	CL_GameModule_Init();
	msg_t * args = CL_AddReliableCommand( ClientCommand_Begin );
	MSG_WriteInt32( args, precache_spawncount );
}

static bool AddDownloadedMap( Span< const char > filename, Span< const u8 > compressed ) {
	if( compressed.ptr == NULL )
		return false;

	Span< u8 > data;
	defer { Free( sys_allocator, data.ptr ); };

	if( !Decompress( filename, sys_allocator, compressed, &data ) ) {
		Com_Printf( "Downloaded map is corrupt.\n" );
		return false;
	}

	TempAllocator temp = cls.frame_arena.temp();
	Span< const char > clean_filename = StripPrefix( StripExtension( filename ), "base/" );
	if( !AddMap( data, clean_filename ) ) {
		Com_Printf( "Downloaded map is corrupt.\n" );
		return false;
	}

	return true;
}

/*
* CL_Precache_f
*
* The server will send this command right
* before allowing the client into the server
*/
void CL_Precache_f( const Tokenized & args ) {
	if( CL_DemoPlaying() ) {
		if( !CL_DemoSeeking() ) {
			CL_GameModule_Init();
		}
		else {
			CL_GameModule_Reset();
			StopAllSounds( false );
		}

		return;
	}

	precache_spawncount = SpanToInt( args.tokens[ 1 ], 0 );
	cl.map = FindMap( StringHash( Hash64( args.tokens[ 2 ] ) ) );

	if( cl.map == NULL ) {
		TempAllocator temp = cls.frame_arena.temp();
		CL_DownloadFile( temp.sv( "base/maps/{}.cdmap.zst", args.tokens[ 2 ] ), []( Span< const char > filename, Span< const u8 > data ) {
			if( AddDownloadedMap( filename, data ) ) {
				CL_FinishConnect();
			}
			else {
				CL_Disconnect( NULL );
			}
		} );

		return;
	}

	CL_FinishConnect();
}

static void CL_WriteConfiguration() {
	TempAllocator temp = cls.frame_arena.temp();

	DynamicString config( &temp );

	WriteKeyBindingsConfig( &config );
	config += "\r\n";
	config += Cvar_MakeConfig( &temp );

	const char * fmt = is_public_build ? "{}/config.cfg" : "{}/base/config.cfg";
	const char * path = temp( fmt, HomeDirPath() );
	if( !WriteFile( &temp, path, config.c_str(), config.length() ) ) {
		Com_Printf( "Couldn't write %s.\n", path );
	}
}

void CL_SetClientState( connstate_t state ) {
	cls.state = state;
	Com_SetClientState( state );

	switch( state ) {
		case CA_DISCONNECTED:
			Con_Close();
			UI_ShowMainMenu();
			break;
		case CA_CONNECTING:
			cls.cgameActive = false;
			Con_Close();
			UI_ShowConnectingScreen();
			break;
		case CA_CONNECTED:
			cls.cgameActive = false;
			Con_Close();
			ResetCheatCvars();
			break;
		case CA_ACTIVE:
			Con_Close();
			UI_HideMenu();
			StopMenuMusic();
			break;
		default:
			break;
	}
}

static Span< Span< const char > > TabCompleteDemo( TempAllocator * a, Span< const char > partial ) {
	return TabCompleteFilenameHomeDir( a, partial, "demos", APP_DEMO_EXTENSION_STR );
}

static void CL_InitLocal() {
	TempAllocator temp = cls.frame_arena.temp();

	cls.state = CA_DISCONNECTED;
	Com_SetClientState( CA_DISCONNECTED );

	cl_maxfps = NewCvar( "cl_maxfps", "250", CvarFlag_Archive );
	cl_pps = NewCvar( "cl_pps", "40", CvarFlag_Archive );

	cl_extrapolationTime = NewCvar( "cl_extrapolationTime", "0", CvarFlag_Developer );
	cl_extrapolate = NewCvar( "cl_extrapolate", "1", CvarFlag_Archive );

	cl_hotloadAssets = NewCvar( "cl_hotloadAssets", is_public_build ? "0" : "1", CvarFlag_Archive );

	cl_shownet = NewCvar( "cl_shownet", "0" );
	cl_timeout = NewCvar( "cl_timeout", "120" );

	// wsw : debug netcode
	cl_debug_serverCmd = NewCvar( "cl_debug_serverCmd", "0", CvarFlag_Cheat );
	cl_debug_timeDelta = NewCvar( "cl_debug_timeDelta", "0", CvarFlags( 0 ) /*CvarFlag_Cheat*/ );

	cl_devtools = NewCvar( "cl_devtools", "0", CvarFlag_Archive );

	NewCvar( "password", "", CvarFlag_UserInfo );

	Cvar * name = NewCvar( "name", "", CvarFlag_UserInfo | CvarFlag_Archive );
	if( StrEqual( name->value, "" ) ) {
		Cvar_Set( "name", temp.sv( "user{06}", RandomUniform( &cls.rng, 0, 1000000 ) ) );
	}

	//avoid the game from crashing in the menu
	NewCvar( "cg_loadout", "", CvarFlag_Archive | CvarFlag_UserInfo );
	NewCvar( "cg_mask", "", CvarFlag_Archive | CvarFlag_UserInfo );
	NewCvar( "cg_showHotkeys", "1", CvarFlag_Archive );
	NewCvar( "cg_colorBlind", "0", CvarFlag_Archive );
	NewCvar( "cg_crosshair_size", "3", CvarFlag_Archive );
	NewCvar( "cg_crosshair_gap", "0", CvarFlag_Archive );
	NewCvar( "cg_crosshair_dynamic", "1", CvarFlag_Archive );
	NewCvar( "cg_showSpeed", "0", CvarFlag_Archive );
	NewCvar( "cg_chat", "1", CvarFlag_Archive );
	cg_showFPS = NewCvar( "cg_showFPS", "0", CvarFlag_Archive );

	NewCvar( "sensitivity", "3", CvarFlag_Archive );
	NewCvar( "horizontalsensscale", "1", CvarFlag_Archive );
	NewCvar( "m_invertY", "0", CvarFlag_Archive );

	AddCommand( "connect", CL_Connect_f );
	AddCommand( "reconnect", []( const Tokenized & args ) { CL_Reconnect_f(); } );
	AddCommand( "disconnect", []( const Tokenized & args ) { CL_Disconnect( NULL ); } );
	AddCommand( "demo", CL_PlayDemo_f );
	AddCommand( "record", CL_Record_f );
	AddCommand( "stop", []( const Tokenized & args ) { CL_StopRecording( false ); } );
	AddCommand( "yolodemo", CL_YoloDemo_f );
	AddCommand( "demopause", []( const Tokenized & args ) { CL_PauseDemo_f(); } );
	AddCommand( "demojump", CL_DemoJump_f );

	SetTabCompletionCallback( "demo", TabCompleteDemo );
	SetTabCompletionCallback( "yolodemo", TabCompleteDemo );
}

static void CL_ShutdownLocal() {
	cls.state = CA_UNINITIALIZED;
	Com_SetClientState( CA_UNINITIALIZED );

	RemoveCommand( "connect" );
	RemoveCommand( "reconnect" );
	RemoveCommand( "disconnect" );
	RemoveCommand( "demo" );
	RemoveCommand( "record" );
	RemoveCommand( "stop" );
	RemoveCommand( "yolodemo" );
	RemoveCommand( "demopause" );
	RemoveCommand( "demojump" );
}

//============================================================================

/*
* CL_AdjustServerTime - adjust delta to new frame snap timestamp
*/
void CL_AdjustServerTime( unsigned int gameMsec ) {
	TracyZoneScoped;

	// hurry up if coming late (unless in demos)
	if( !CL_DemoPlaying() ) {
		if( ( cl.newServerTimeDelta < cl.serverTimeDelta ) && gameMsec > 0 ) {
			cl.serverTimeDelta--;
		}
		if( cl.newServerTimeDelta > cl.serverTimeDelta ) {
			cl.serverTimeDelta++;
		}
	}

	cl.serverTime = ( cls.game_time.flicks / ( GGTIME_FLICKS_PER_SECOND / 1000 ) ) + cl.serverTimeDelta;

	// it launches a new snapshot when the timestamp of the CURRENT snap is reached.
	if( cl.pendingSnapNum && ( cl.serverTime >= cl.snapShots[ cl.currentSnapNum % ARRAY_COUNT( cl.snapShots ) ].serverTime ) ) {
		// fire next snapshot
		if( CL_GameModule_NewSnapshot( cl.pendingSnapNum ) ) {
			cl.previousSnapNum = cl.currentSnapNum;
			cl.currentSnapNum = cl.pendingSnapNum;
			cl.pendingSnapNum = 0;

			// getting a valid snapshot ends the connection process
			if( cls.state == CA_CONNECTED ) {
				CL_SetClientState( CA_ACTIVE );
			}
		}
	}
}

void CL_RestartTimeDeltas( int newTimeDelta ) {
	int i;

	cl.serverTimeDelta = cl.newServerTimeDelta = newTimeDelta;
	for( i = 0; i < MAX_TIMEDELTAS_BACKUP; i++ )
		cl.serverTimeDeltas[i] = newTimeDelta;

	if( cl_debug_timeDelta->integer ) {
		Com_Printf( S_COLOR_CYAN "***** timeDelta restarted\n" );
	}
}

int CL_SmoothTimeDeltas() {
	if( CL_DemoPlaying() ) {
		if( cl.currentSnapNum <= 0 ) { // if first snap
			return cl.serverTimeDeltas[ cl.pendingSnapNum % ARRAY_COUNT( cl.serverTimeDeltas ) ];
		}

		return cl.serverTimeDeltas[ cl.currentSnapNum % ARRAY_COUNT( cl.serverTimeDeltas ) ];
	}

	int i = cl.receivedSnapNum - Min2( MAX_TIMEDELTAS_BACKUP, 8 );
	if( i < 0 ) {
		i = 0;
	}

	double delta = 0.0;
	int count = 0;
	for( ; i <= cl.receivedSnapNum; i++ ) {
		const snapshot_t * snap = &cl.snapShots[ i % ARRAY_COUNT( cl.snapShots ) ];
		if( snap->valid && snap->serverFrame == i ) {
			delta += (double)cl.serverTimeDeltas[ i % ARRAY_COUNT( cl.serverTimeDeltas ) ];
			count++;
		}
	}

	if( !count ) {
		return 0;
	}

	return (int)( delta / (double)count );
}

/*
* CL_UpdateSnapshot - Check for pending snapshots, and fire if needed
*/
static void CL_UpdateSnapshot() {
	TracyZoneScoped;

	// see if there is any pending snap to be fired
	if( !cl.pendingSnapNum && ( cl.currentSnapNum != cl.receivedSnapNum ) ) {
		snapshot_t * snap = NULL;
		for( int i = cl.currentSnapNum + 1; i <= cl.receivedSnapNum; i++ ) {
			if( cl.snapShots[ i % ARRAY_COUNT( cl.snapShots ) ].valid && ( cl.snapShots[ i % ARRAY_COUNT( cl.snapShots ) ].serverFrame > cl.currentSnapNum ) ) {
				snap = &cl.snapShots[ i % ARRAY_COUNT( cl.snapShots ) ];
				//torbenh: this break was the source of the lag bug at cl_fps < sv_pps
				//break;
			}
		}

		if( snap ) { // valid pending snap found
			cl.pendingSnapNum = snap->serverFrame;

			cl.newServerTimeDelta = CL_SmoothTimeDeltas();

			if( cl_extrapolationTime->modified ) {
				if( cl_extrapolationTime->integer > (int)cl.snapFrameTime - 1 ) {
					Cvar_SetInteger( "cl_extrapolationTime", (int)cl.snapFrameTime - 1 );
				} else if( cl_extrapolationTime->integer < 0 ) {
					Cvar_ForceSet( "cl_extrapolationTime", "0" );
				}

				cl_extrapolationTime->modified = false;
			}

			if( !CL_DemoPlaying() && cl_extrapolate->integer ) {
				cl.newServerTimeDelta += cl_extrapolationTime->integer;
			}

			// if we don't have current snap (or delay is too big) don't wait to fire the pending one
			if( ( !CL_DemoSeeking() && cl.currentSnapNum <= 0 ) ||
				( !CL_DemoPlaying() && Abs( cl.newServerTimeDelta - cl.serverTimeDelta ) > 200 ) ) {
				cl.serverTimeDelta = cl.newServerTimeDelta;
			}
		}
	}
}

void CL_Netchan_Transmit( msg_t * msg ) {
	if( msg->cursize == 0 ) {
		return;
	}

	// if we got here with unsent fragments, fire them all now
	Netchan_PushAllFragments( cls.socket, &cls.netchan );

	if( msg->cursize > 60 ) {
		Netchan_CompressMessage( msg );
	}

	Netchan_Transmit( cls.socket, &cls.netchan, msg );
	cls.lastPacketSentTime = cls.realtime;
}

static bool CL_MaxPacketsReached() {
	static int64_t lastPacketTime = 0;
	static float roundingMsec = 0.0f;
	int minpackettime;
	int elapsedTime;

	if( lastPacketTime > cls.realtime ) {
		lastPacketTime = cls.realtime;
	}

	if( cl_pps->integer > 62 || cl_pps->integer < 40 ) {
		Com_Printf( "'cl_pps' value is out of valid range, resetting to default\n" );
		Cvar_ForceSet( "cl_pps", cl_pps->default_value );
	}

	elapsedTime = cls.realtime - lastPacketTime;
	float minTime = ( 1000.0f / cl_pps->number );

	// don't let cl_pps be smaller than sv_pps
	if( cls.state == CA_ACTIVE && !CL_DemoPlaying() && cl.snapFrameTime ) {
		if( (unsigned int)minTime > cl.snapFrameTime ) {
			minTime = cl.snapFrameTime;
		}
	}

	minpackettime = (int)minTime;
	roundingMsec += minTime - (int)minTime;
	if( roundingMsec >= 1.0f ) {
		minpackettime += (int)roundingMsec;
		roundingMsec -= (int)roundingMsec;
	}

	if( elapsedTime < minpackettime ) {
		return false;
	}

	lastPacketTime = cls.realtime;
	return true;
}

void CL_SendMessagesToServer( bool sendNow ) {
	if( cls.state == CA_DISCONNECTED || cls.state == CA_CONNECTING ) {
		return;
	}

	if( CL_DemoPlaying() ) {
		return;
	}

	uint8_t messageData[MAX_MSGLEN];
	msg_t message = NewMSGWriter( messageData, sizeof( messageData ) );

	// send only reliable commands during connecting time
	if( cls.state < CA_ACTIVE ) {
		if( sendNow || cls.realtime > 100 + cls.lastPacketSentTime ) {
			// write the command ack
			MSG_WriteUint8( &message, clc_svcack );
			MSG_WriteIntBase128( &message, cls.lastExecutedServerCommand );
			//write up the clc commands
			SerializeReliableCommands( &message );
			CL_Netchan_Transmit( &message );
		}
	} else if( sendNow || CL_MaxPacketsReached() ) {
		// write the command ack
		MSG_WriteUint8( &message, clc_svcack );
		MSG_WriteIntBase128( &message, cls.lastExecutedServerCommand );
		// send a userinfo update if needed
		if( userinfo_modified ) {
			TempAllocator temp = cls.frame_arena.temp();
			msg_t * args = CL_AddReliableCommand( ClientCommand_UserInfo );
			MSG_WriteString( args, Cvar_GetUserInfo() );
			userinfo_modified = false;
		}
		SerializeReliableCommands( &message );
		CL_WriteUcmdsToMessage( &message );
		CL_Netchan_Transmit( &message );
	}
}

static void CL_NetFrame( int realMsec, int gameMsec ) {
	TracyZoneScoped;

	// read packets from server
	if( realMsec > 5000 ) { // if in the debugger last frame, don't timeout
		cls.lastPacketReceivedTime = cls.realtime;
	}

	if( CL_DemoPlaying() ) {
		CL_ReadDemoPackets(); // fetch results from demo file
	}
	CL_ReadPackets(); // fetch results from server

	// send packets to server
	if( cls.netchan.unsentFragments ) {
		Netchan_TransmitNextFragment( cls.socket, &cls.netchan );
	} else {
		CL_SendMessagesToServer( false );
	}

	// resend a connection request if necessary
	CL_CheckForResend();

	ServerBrowserFrame();
}

void CL_Frame( int realMsec, int gameMsec ) {
	TracyZoneScoped;

	LivePPFrame();
	DiscordFrame();

	TracyCPlot( "Client frame arena max utilisation", cls.frame_arena.max_utilisation() );
	cls.frame_arena.clear();

	u64 entropy[ 2 ];
	CSPRNG( entropy, sizeof( entropy ) );
	cls.rng = NewRNG( entropy[ 0 ], entropy[ 1 ] );

	static int allRealMsec = 0, allGameMsec = 0, extraMsec = 0;
	static float roundingMsec = 0.0f;

	cls.monotonicTime += Milliseconds( realMsec );
	cls.game_time += Milliseconds( gameMsec );
	cls.shadertoy_time += Milliseconds( gameMsec );
	cls.realtime += realMsec;

	if( CL_DemoPlaying() ) {
		if( CL_DemoPaused() ) {
			gameMsec = 0;
		}
		CL_LatchedDemoJump();
	}

	cls.gametime += gameMsec;

	allRealMsec += realMsec;
	allGameMsec += gameMsec;

	CL_UpdateSnapshot();
	CL_AdjustServerTime( gameMsec );
	CL_UserInputFrame( realMsec );
	CL_NetFrame( realMsec, gameMsec );
	PumpDownloads();

	constexpr int absMinFps = 24;

	// do not allow setting cl_maxfps to very low values to prevent cheating
	if( cl_maxfps->integer < absMinFps ) {
		Cvar_SetInteger( "cl_maxfps", absMinFps );
	}
	float maxFps = IsWindowFocused() ? cl_maxfps->number : absMinFps;
	int minMsec = Max2( 1000.0f / maxFps, 1.0f );
	roundingMsec += Max2( 1000.0f / maxFps, 1.0f ) - minMsec;

	if( roundingMsec >= 1.0f ) {
		minMsec += (int)roundingMsec;
		roundingMsec -= (int)roundingMsec;
	}

	if( allRealMsec + extraMsec < minMsec ) {
		// let CPU sleep while minimized
		bool sleep = cls.state == CA_DISCONNECTED || !IsWindowFocused();

		if( sleep && minMsec - extraMsec > 1 ) {
			TracyZoneScopedNC( "Sys_Sleep", 0xff0000 );
			Sys_Sleep( minMsec - extraMsec - 1 );
		}
		return;
	}

	TracyCFrameMark;

	DoneHotloadingAssets();
	if( cl_hotloadAssets->integer != 0 ) {
		TempAllocator temp = cls.frame_arena.temp();
		HotloadAssets( &temp );
	}

	cls.frametime = allGameMsec;
	cls.realFrameTime = allRealMsec;
	if( allRealMsec < minMsec ) { // is compensating for a too slow frame
		extraMsec = Clamp( 0, extraMsec - ( minMsec - allRealMsec ), 100 );
	} else {   // too slow, or exact frame
		extraMsec = Clamp( 0, allRealMsec - minMsec, 100 );
	}

	VID_CheckChanges();

	// update the screen
	int viewport_width, viewport_height;
	GetFramebufferSize( &viewport_width, &viewport_height );
	RendererBeginFrame( viewport_width, viewport_height );
	ClayBeginFrame();

	SCR_UpdateScreen();

	// update audio
	if( cls.state != CA_ACTIVE ) {
		SoundFrame( Vec3( 0.0f ), Vec3( 0.0f ), Vec3( 1.0f, 0.0f, 0.0f ), Vec3( 0.0f, 0.0f, 1.0f ) );
	}

	RendererEndFrame();

	allRealMsec = 0;
	allGameMsec = 0;

	cl.prevviewangles = cl.viewangles;

	cls.framecount++;

	SwapBuffers();
}

void CL_Init() {
	TracyZoneScoped;

	InitLivePP();

	constexpr size_t frame_arena_size = Megabytes( 1 );
	void * frame_arena_memory = sys_allocator->allocate( frame_arena_size, 16 );
	cls.frame_arena = ArenaAllocator( frame_arena_memory, frame_arena_size );

	u64 entropy[ 2 ];
	CSPRNG( entropy, sizeof( entropy ) );
	cls.rng = NewRNG( entropy[ 0 ], entropy[ 1 ] );
	cls.per_launch_entropy = Random64( &cls.rng );

	CSPRNG( &cls.session_id, sizeof( cls.session_id ) );

	cls.monotonicTime = { };
	cls.shadertoy_time = { };

	Assert( !cl_initialized );

	cl_initialized = true;

	InitThreadPool();

	{
#if PLATFORM_WINDOWS
		// both VID_Init and InitAssets need to run on the main thread on Windows
		VID_Init();
		TempAllocator temp = cls.frame_arena.temp();
		InitAssets( &temp );
#else
		// overlap loading assets and creating a window
		ThreadPoolDo( []( TempAllocator * temp, void * data ) {
			InitAssets( temp );
		} );
		VID_Init();

		ThreadPoolFinish();
#endif
	}

	InitRenderer();
	InitGLTFModels();
	InitMaps();
	InitSound();

	cls.white_material = FindMaterial( "white" );

	CL_ClearState();

	cls.socket = NewUDPClient( NonBlocking_Yes );

	SCR_InitScreen();

	CL_InitLocal();
	CL_InitInput();

	InitDiscord();
	InitDownloads();
	InitServerBrowser();
	InitDemoBrowser();

	CL_InitImGui();
	InitClay();
	UI_Init();

	cls.fontNormal = RegisterFont( "fonts/Decalotype-Bold" );
	cls.fontNormalBold = RegisterFont( "fonts/Decalotype-Black" );
	cls.fontItalic = RegisterFont( "fonts/Decalotype-BoldItalic" );
	cls.fontBoldItalic = RegisterFont( "fonts/Decalotype-BlackItalic" );
}

void CL_Shutdown() {
	TracyZoneScoped;

	if( !cl_initialized ) {
		return;
	}

	StopAllSounds( true );

	CL_WriteConfiguration();

	CL_Disconnect( NULL );
	CloseSocket( cls.socket );

	ShutdownDiscord();

	UI_Shutdown();
	ShutdownClay();
	CL_ShutdownImGui();

	CL_GameModule_Shutdown();
	ShutdownSound();
	ShutdownMaps();
	ShutdownGLTFModels();
	ShutdownRenderer();
	DestroyWindow();

	ShutdownDemoBrowser();
	ShutdownServerBrowser();
	ShutdownDownloads();

	CL_ShutdownLocal();

	ShutdownThreadPool();

	Con_Shutdown();

	ShutdownAssets();

	cls.state = CA_UNINITIALIZED;
	cl_initialized = false;

	Free( sys_allocator, cls.frame_arena.get_memory() );
}
