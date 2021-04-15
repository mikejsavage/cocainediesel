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
#include "client/downloads.h"
#include "client/threadpool.h"
#include "client/renderer/renderer.h"
#include "qcommon/csprng.h"
#include "qcommon/hash.h"
#include "qcommon/fs.h"
#include "qcommon/string.h"
#include "qcommon/version.h"
#include "gameshared/gs_public.h"

cvar_t *rcon_client_password;
cvar_t *rcon_address;

cvar_t *cl_timeout;
cvar_t *cl_maxfps;
cvar_t *cl_pps;
cvar_t *cl_shownet;

cvar_t *cl_extrapolationTime;
cvar_t *cl_extrapolate;

static cvar_t *cl_hotloadAssets;

//
// userinfo
//
cvar_t *info_password;

// wsw : debug netcode
cvar_t *cl_debug_serverCmd;
cvar_t *cl_debug_timeDelta;

cvar_t *cl_devtools;

client_static_t cls;
client_state_t cl;

SyncEntityState cl_baselines[MAX_EDICTS];

static bool cl_initialized = false;

/*
=======================================================================

CLIENT RELIABLE COMMAND COMMUNICATION

=======================================================================
*/

/*
* CL_AddReliableCommand
*
* The given command will be transmitted to the server, and is gauranteed to
* not have future usercmd_t executed before it is executed
*/
void CL_AddReliableCommand( const char *cmd ) {
	int index;

	if( !cmd || !strlen( cmd ) ) {
		return;
	}

	// if we would be losing an old command that hasn't been acknowledged,
	// we must drop the connection
	if( cls.reliableSequence > cls.reliableAcknowledge + MAX_RELIABLE_COMMANDS ) {
		cls.reliableAcknowledge = cls.reliableSequence; // try to avoid loops
		Com_Error( ERR_DROP, "Client command overflow %" PRIi64 " %" PRIi64, cls.reliableAcknowledge, cls.reliableSequence );
	}
	cls.reliableSequence++;
	index = cls.reliableSequence & ( MAX_RELIABLE_COMMANDS - 1 );
	Q_strncpyz( cls.reliableCommands[index], cmd, sizeof( cls.reliableCommands[index] ) );
}

/*
* CL_UpdateClientCommandsToServer
*
* Add the pending commands to the message
*/
void CL_UpdateClientCommandsToServer( msg_t *msg ) {
	int64_t i;

	// write any unacknowledged clientCommands
	for( i = cls.reliableAcknowledge + 1; i <= cls.reliableSequence; i++ ) {
		if( !strlen( cls.reliableCommands[i & ( MAX_RELIABLE_COMMANDS - 1 )] ) ) {
			continue;
		}

		MSG_WriteUint8( msg, clc_clientcommand );
		if( !cls.reliable ) {
			MSG_WriteIntBase128( msg, i );
		}
		MSG_WriteString( msg, cls.reliableCommands[i & ( MAX_RELIABLE_COMMANDS - 1 )] );
	}

	cls.reliableSent = cls.reliableSequence;
	if( cls.reliable ) {
		cls.reliableAcknowledge = cls.reliableSent;
	}
}

void CL_ForwardToServer_f() {
	if( cls.demo.playing ) {
		return;
	}

	if( cls.state != CA_CONNECTED && cls.state != CA_ACTIVE ) {
		Com_Printf( "Can't \"%s\", not connected\n", Cmd_Argv( 0 ) );
		return;
	}

	// don't forward the first argument
	if( Cmd_Argc() > 1 ) {
		CL_AddReliableCommand( Cmd_Args() );
	}
}

void CL_ServerDisconnect_f() {
	char menuparms[MAX_STRING_CHARS];
	int type;
	char reason[MAX_STRING_CHARS];

	type = atoi( Cmd_Argv( 1 ) );
	if( type < 0 || type >= DROP_TYPE_TOTAL ) {
		type = DROP_TYPE_GENERAL;
	}

	Q_strncpyz( reason, Cmd_Argv( 2 ), sizeof( reason ) );

	CL_Disconnect_f();

	Com_Printf( "Connection was closed by server: %s\n", reason );

	snprintf( menuparms, sizeof( menuparms ), "menu_open connfailed dropreason %i servername \"%s\" droptype %i rejectmessage \"%s\"",
				 DROP_REASON_CONNTERMINATED, cls.servername, type, reason );

	Cbuf_ExecuteText( EXEC_NOW, menuparms );
}

void CL_Quit() {
	CL_Disconnect( NULL );
	Com_Quit();
}

static void CL_Quit_f() {
	CL_Quit();
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
	Netchan_OutOfBandPrint( cls.socket, &cls.serveraddress, "%s", temp( "connect {} {} {} \"{}\"\n",
							APP_PROTOCOL_VERSION, Netchan_ClientSessionID(), cls.challenge, Cvar_Userinfo() ) );
}

/*
* CL_CheckForResend
*
* Resend a connect message if the last one has timed out
*/
static void CL_CheckForResend() {
	// FIXME: should use cls.realtime, but it can be old here after starting a server
	int64_t realtime = Sys_Milliseconds();

	if( cls.demo.playing ) {
		return;
	}

	// if the local server is running and we aren't then connect
	if( cls.state == CA_DISCONNECTED && Com_ServerState() ) {
		CL_SetClientState( CA_CONNECTING );
		if( cls.servername ) {
			Mem_ZoneFree( cls.servername );
		}
		cls.servername = ZoneCopyString( "localhost" );
		cls.servertype = SOCKET_LOOPBACK;
		NET_InitAddress( &cls.serveraddress, NA_LOOPBACK );
		if( !NET_OpenSocket( &cls.socket_loopback, cls.servertype, &cls.serveraddress, false ) ) {
			Com_Error( ERR_FATAL, "Couldn't open the loopback socket\n" );
			return;
		}
		cls.socket = &cls.socket_loopback;
	}

	// resend if we haven't gotten a reply yet
	if( cls.state == CA_CONNECTING && !cls.reliable ) {
		if( realtime - cls.connect_time < 3000 ) {
			return;
		}
		if( cls.connect_count > 3 ) {
			CL_Disconnect( "Connection timed out" );
			return;
		}
		cls.connect_count++;
		cls.connect_time = realtime; // for retransmit requests

		Com_Printf( "Connecting to %s...\n", cls.servername );

		Netchan_OutOfBandPrint( cls.socket, &cls.serveraddress, "getchallenge\n" );
	}

	if( cls.state == CA_CONNECTING && cls.reliable ) {
		if( realtime - cls.connect_time < 10000 ) {
			return;
		}

		CL_Disconnect( "Connection timed out" );
	}
}

static void CL_Connect( const char *servername, socket_type_t type, netadr_t *address ) {
	netadr_t socketaddress;

	CL_Disconnect( NULL );

	switch( type ) {
		case SOCKET_LOOPBACK:
			NET_InitAddress( &socketaddress, NA_LOOPBACK );
			if( !NET_OpenSocket( &cls.socket_loopback, SOCKET_LOOPBACK, &socketaddress, false ) ) {
				Com_Error( ERR_FATAL, "Couldn't open the loopback socket: %s\n", NET_ErrorString() ); // FIXME
				return;
			}
			cls.socket = &cls.socket_loopback;
			cls.reliable = false;
			break;

		case SOCKET_UDP:
			cls.socket = ( address->type == NA_IP6 ?  &cls.socket_udp6 :  &cls.socket_udp );
			cls.reliable = false;
			break;

		default:
			assert( false );
			return;
	}

	cls.servertype = type;
	cls.serveraddress = *address;
	if( NET_GetAddressPort( &cls.serveraddress ) == 0 ) {
		NET_SetAddressPort( &cls.serveraddress, PORT_SERVER );
	}

	if( cls.servername ) {
		Mem_ZoneFree( cls.servername );
	}
	cls.servername = ZoneCopyString( servername );

	memset( cl.configstrings, 0, sizeof( cl.configstrings ) );

	CL_SetClientState( CA_CONNECTING );

	cls.connect_time = -99999; // CL_CheckForResend() will fire immediately
	cls.connect_count = 0;
	cls.rejected = false;
	cls.lastPacketReceivedTime = cls.realtime; // reset the timeout limit
}

static void CL_Connect_Cmd_f( socket_type_t socket ) {
	netadr_t serveraddress;
	char *servername, password[64];
	char *connectstring, *connectstring_base;
	const char *tmp;

	if( Cmd_Argc() < 2 ) {
		Com_Printf( "Usage: %s <server>\n", Cmd_Argv( 0 ) );
		return;
	}

	connectstring_base = TempCopyString( Cmd_Argv( 1 ) );
	connectstring = connectstring_base;

	if( !Q_strnicmp( connectstring, APP_URI_SCHEME, strlen( APP_URI_SCHEME ) ) ) {
		connectstring += strlen( APP_URI_SCHEME );
	}

	if( FileExtension( connectstring ) == APP_DEMO_EXTENSION_STR ) {
		char *temp;
		size_t temp_size;
		const char *http_scheme = "http://";

		if( !Q_strnicmp( connectstring, http_scheme, strlen( http_scheme ) ) ) {
			connectstring += strlen( http_scheme );
		}

		temp_size = strlen( "demo " ) + strlen( http_scheme ) + strlen( connectstring ) + 1;
		temp = ( char * ) Mem_TempMalloc( temp_size );
		snprintf( temp, temp_size, "demo %s%s", http_scheme, connectstring );

		Cbuf_ExecuteText( EXEC_NOW, temp );

		Mem_TempFree( temp );
		Mem_TempFree( connectstring_base );
		return;
	}

	if( ( tmp = Q_strrstr( connectstring, "@" ) ) != NULL ) {
		Q_strncpyz( password, connectstring, qmin( sizeof( password ), tmp - connectstring + 1 ) );
		Cvar_Set( "password", password );
		connectstring += ( tmp - connectstring ) + 1;
	}

	if( ( tmp = Q_strrstr( connectstring, "/" ) ) != NULL ) {
		connectstring[tmp - connectstring] = '\0';
	}

	if( !NET_StringToAddress( connectstring, &serveraddress ) ) {
		Mem_TempFree( connectstring_base );
		Com_Printf( "Bad server address\n" );
		return;
	}

	servername = TempCopyString( connectstring );
	CL_Connect( servername, ( serveraddress.type == NA_LOOPBACK ? SOCKET_LOOPBACK : socket ), &serveraddress );

	Mem_TempFree( servername );
	Mem_TempFree( connectstring_base );
}

static void CL_Connect_f() {
	CL_Connect_Cmd_f( SOCKET_UDP );
}


/*
* CL_Rcon_f
*
* Send the rest of the command line over as
* an unconnected command.
*/
static void CL_Rcon_f() {
	char message[1024];
	int i;
	const socket_t *socket;
	const netadr_t *address;

	if( cls.demo.playing ) {
		return;
	}

	if( rcon_client_password->string[0] == '\0' ) {
		Com_Printf( "You must set 'rcon_password' before issuing an rcon command.\n" );
		return;
	}

	// wsw : jal : check for msg len abuse (thx to r1Q2)
	if( strlen( Cmd_Args() ) + strlen( rcon_client_password->string ) + 16 >= sizeof( message ) ) {
		Com_Printf( "Length of password + command exceeds maximum allowed length.\n" );
		return;
	}

	message[0] = (uint8_t)255;
	message[1] = (uint8_t)255;
	message[2] = (uint8_t)255;
	message[3] = (uint8_t)255;
	message[4] = 0;

	Q_strncatz( message, "rcon ", sizeof( message ) );

	Q_strncatz( message, rcon_client_password->string, sizeof( message ) );
	Q_strncatz( message, " ", sizeof( message ) );

	for( i = 1; i < Cmd_Argc(); i++ ) {
		Q_strncatz( message, "\"", sizeof( message ) );
		Q_strncatz( message, Cmd_Argv( i ), sizeof( message ) );
		Q_strncatz( message, "\" ", sizeof( message ) );
	}

	if( cls.state >= CA_CONNECTED ) {
		socket = cls.netchan.socket;
		address = &cls.netchan.remoteAddress;
	} else {
		if( !strlen( rcon_address->string ) ) {
			Com_Printf( "You must be connected, or set the 'rcon_address' cvar to issue rcon commands\n" );
			return;
		}

		if( rcon_address->modified ) {
			if( !NET_StringToAddress( rcon_address->string, &cls.rconaddress ) ) {
				Com_Printf( "Bad rcon_address.\n" );
				return; // we don't clear modified, so it will whine the next time too
			}
			if( NET_GetAddressPort( &cls.rconaddress ) == 0 ) {
				NET_SetAddressPort( &cls.rconaddress, PORT_SERVER );
			}

			rcon_address->modified = false;
		}

		socket = ( cls.rconaddress.type == NA_IP6 ? &cls.socket_udp6 : &cls.socket_udp );
		address = &cls.rconaddress;
	}

	NET_SendPacket( socket, message, (int)strlen( message ) + 1, address );
}

void CL_SetKeyDest( keydest_t key_dest ) {
	if( key_dest != cls.key_dest ) {
		Key_ClearStates();
		cls.key_dest = key_dest;
	}
}

void CL_SetOldKeyDest( keydest_t key_dest ) {
	cls.old_key_dest = key_dest;
}

size_t CL_GetBaseServerURL( char *buffer, size_t buffer_size ) {
	const char *web_url = cls.httpbaseurl;

	if( !buffer || !buffer_size ) {
		return 0;
	}
	if( !web_url || !*web_url ) {
		*buffer = '\0';
		return 0;
	}

	Q_strncpyz( buffer, web_url, buffer_size );
	return strlen( web_url );
}

void CL_ResetServerCount() {
	cl.servercount = -1;
}

void CL_ClearState() {
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
	cls.realtime = 0;
	cls.gametime = 0;
	cls.lastPacketSentTime = 0;
	cls.lastPacketReceivedTime = 0;
}


/*
* CL_SetNext_f
*
* Next is used to set an action which is executed at disconnecting.
*/
static void CL_SetNext_f() {
	if( Cmd_Argc() < 2 ) {
		Com_Printf( "USAGE: next <commands>\n" );
		return;
	}
}


/*
* CL_Disconnect_SendCommand
*
* Sends a disconnect message to the server
*/
static void CL_Disconnect_SendCommand() {
	// wsw : jal : send the packet 3 times to make sure isn't lost
	CL_AddReliableCommand( "disconnect" );
	CL_SendMessagesToServer( true );
	CL_AddReliableCommand( "disconnect" );
	CL_SendMessagesToServer( true );
	CL_AddReliableCommand( "disconnect" );
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
	// We have to shut down webdownloading first
	if( CL_IsDownloading() ) {
		CL_CancelDownload();
		return;
	}

	if( cls.state == CA_UNINITIALIZED ) {
		return;
	}
	if( cls.state == CA_DISCONNECTED ) {
		return;
	}

	bool wasconnecting = cls.state < CA_CONNECTED;

	SV_ShutdownGame( "Owner left the listen server", false );

	cls.connect_time = 0;
	cls.connect_count = 0;
	cls.rejected = false;

	if( cls.demo.recording ) {
		CL_Stop_f();
	}

	if( cls.demo.playing ) {
		CL_DemoCompleted();
	} else {
		CL_Disconnect_SendCommand(); // send a disconnect message to the server
	}

	// udp is kept open all the time, for connectionless messages
	if( cls.socket && cls.socket->type != SOCKET_UDP ) {
		NET_CloseSocket( cls.socket );
	}

	cls.socket = NULL;
	cls.reliable = false;

	if( cls.httpbaseurl ) {
		Mem_Free( cls.httpbaseurl );
		cls.httpbaseurl = NULL;
	}

	S_StopAllSounds( false );

	CL_GameModule_Shutdown();

	CL_ClearState();
	CL_SetClientState( CA_DISCONNECTED );

	if( message != NULL ) {
		char menuparms[MAX_STRING_CHARS];
		snprintf( menuparms, sizeof( menuparms ), "menu_open connfailed dropreason %i servername \"%s\" droptype %i rejectmessage \"%s\"",
					 ( wasconnecting ? DROP_REASON_CONNFAILED : DROP_REASON_CONNERROR ), cls.servername, DROP_TYPE_GENERAL, message );

		Cbuf_ExecuteText( EXEC_NOW, menuparms );
	}
}

void CL_Disconnect_f() {
	CL_Disconnect( NULL );
}

/*
* CL_Changing_f
*
* Just sent as a hint to the client that they should
* drop to full console
*/
void CL_Changing_f() {
	//if we are downloading, we don't change!  This so we don't suddenly stop downloading a map
	if( CL_IsDownloading() ) {
		return;
	}

	if( cls.demo.recording ) {
		CL_Stop_f();
	}

	Com_DPrintf( "CL:Changing\n" );

	memset( cl.configstrings, 0, sizeof( cl.configstrings ) );

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
	if( cls.demo.playing ) {
		return;
	}

	//if we are downloading, we don't change!  This so we don't suddenly stop downloading a map
	if( CL_IsDownloading() ) {
		CL_ReconnectAfterDownload();
		return;
	}

	if( cls.state < CA_CONNECTED ) {
		Com_Printf( "Error: CL_ServerReconnect_f while not connected\n" );
		return;
	}

	if( cls.demo.recording ) {
		CL_Stop_f();
	}

	cls.connect_count = 0;
	cls.rejected = false;

	CL_GameModule_Shutdown();
	S_StopAllSounds( true );

	Com_Printf( "Reconnecting...\n" );

	cls.connect_time = Sys_Milliseconds() - 1500;

	memset( cl.configstrings, 0, sizeof( cl.configstrings ) );
	CL_SetClientState( CA_HANDSHAKE );
	CL_AddReliableCommand( "new" );
}

/*
* CL_Reconnect_f
*
* User reconnect command.
*/
void CL_Reconnect_f() {
	char *servername;
	socket_type_t servertype;
	netadr_t serveraddress;

	if( !cls.servername ) {
		Com_Printf( "Can't reconnect, never connected\n" );
		return;
	}

	servername = TempCopyString( cls.servername );
	servertype = cls.servertype;
	serveraddress = cls.serveraddress;
	CL_Disconnect( NULL );
	CL_Connect( servername, servertype, &serveraddress );
	Mem_TempFree( servername );
}

/*
* CL_ConnectionlessPacket
*
* Responses to broadcasts, etc
*/
static void CL_ConnectionlessPacket( const socket_t *socket, const netadr_t *address, msg_t *msg ) {
	char *s;
	const char *c;

	MSG_BeginReading( msg );
	MSG_ReadInt32( msg ); // skip the -1

	s = MSG_ReadStringLine( msg );

	if( !strncmp( s, "getserversResponse\\", 19 ) ) {
		Com_DPrintf( "%s: %s\n", NET_AddressToString( address ), "getserversResponse" );
		CL_ParseGetServersResponse( socket, address, msg, false );
		return;
	}

	if( !strncmp( s, "getserversExtResponse", 21 ) ) {
		Com_DPrintf( "%s: %s\n", NET_AddressToString( address ), "getserversExtResponse" );
		CL_ParseGetServersResponse( socket, address, msg, true );
		return;
	}

	Cmd_TokenizeString( s );
	c = Cmd_Argv( 0 );

	Com_DPrintf( "%s: %s\n", NET_AddressToString( address ), s );

	// server responding to a status broadcast
	if( !strcmp( c, "info" ) ) {
		CL_ParseStatusMessage( socket, address, msg );
		return;
	}

	// jal : wsw
	// server responding to a detailed info broadcast
	if( !strcmp( c, "infoResponse" ) ) {
		CL_ParseGetInfoResponse( socket, address, msg );
		return;
	}
	if( !strcmp( c, "statusResponse" ) ) {
		CL_ParseGetStatusResponse( socket, address, msg );
		return;
	}

	if( cls.demo.playing ) {
		Com_DPrintf( "Received connectionless cmd \"%s\" from %s while playing a demo\n", s, NET_AddressToString( address ) );
		return;
	}

	// server connection
	if( !strcmp( c, "client_connect" ) ) {
		if( cls.state == CA_CONNECTED ) {
			Com_Printf( "Dup connect received.  Ignored.\n" );
			return;
		}
		// these two are from Q3
		if( cls.state != CA_CONNECTING ) {
			Com_Printf( "client_connect packet while not connecting.  Ignored.\n" );
			return;
		}
		if( !NET_CompareAddress( address, &cls.serveraddress ) ) {
			Com_Printf( "client_connect from a different address.  Ignored.\n" );
			Com_Printf( "Was %s should have been %s\n", NET_AddressToString( address ),
						NET_AddressToString( &cls.serveraddress ) );
			return;
		}

		cls.rejected = false;

		Q_strncpyz( cls.session, MSG_ReadStringLine( msg ), sizeof( cls.session ) );

		Netchan_Setup( &cls.netchan, socket, address, Netchan_ClientSessionID() );
		memset( cl.configstrings, 0, sizeof( cl.configstrings ) );
		CL_SetClientState( CA_HANDSHAKE );
		CL_AddReliableCommand( "new" );
		return;
	}

	// reject packet, used to inform the client that connection attemp didn't succeed
	if( !strcmp( c, "reject" ) ) {
		int rejectflag;

		if( cls.state != CA_CONNECTING ) {
			Com_Printf( "reject packet while not connecting, ignored\n" );
			return;
		}
		if( !NET_CompareAddress( address, &cls.serveraddress ) ) {
			Com_Printf( "reject from a different address, ignored\n" );
			Com_Printf( "Was %s should have been %s\n", NET_AddressToString( address ),
						NET_AddressToString( &cls.serveraddress ) );
			return;
		}

		cls.rejected = true;

		cls.rejecttype = atoi( MSG_ReadStringLine( msg ) );
		if( cls.rejecttype < 0 || cls.rejecttype >= DROP_TYPE_TOTAL ) {
			cls.rejecttype = DROP_TYPE_GENERAL;
		}

		rejectflag = atoi( MSG_ReadStringLine( msg ) );

		Q_strncpyz( cls.rejectmessage, MSG_ReadStringLine( msg ), sizeof( cls.rejectmessage ) );
		if( strlen( cls.rejectmessage ) > sizeof( cls.rejectmessage ) - 2 ) {
			cls.rejectmessage[strlen( cls.rejectmessage ) - 2] = '.';
			cls.rejectmessage[strlen( cls.rejectmessage ) - 1] = '.';
			cls.rejectmessage[strlen( cls.rejectmessage )] = '.';
		}

		Com_Printf( "Connection refused: %s\n", cls.rejectmessage );
		if( rejectflag & DROP_FLAG_AUTORECONNECT ) {
			Com_Printf( "Automatic reconnecting allowed.\n" );
		} else {
			char menuparms[MAX_STRING_CHARS];

			Com_Printf( "Automatic reconnecting not allowed.\n" );

			CL_Disconnect( NULL );
			snprintf( menuparms, sizeof( menuparms ), "menu_open connfailed dropreason %i servername \"%s\" droptype %i rejectmessage \"%s\"",
						 DROP_REASON_CONNFAILED, cls.servername, cls.rejecttype, cls.rejectmessage );

			Cbuf_ExecuteText( EXEC_NOW, menuparms );
		}

		return;
	}

	// remote command from gui front end
	if( !strcmp( c, "cmd" ) ) {
		if( !NET_IsLocalAddress( address ) ) {
			Com_Printf( "Command packet from remote host, ignored\n" );
			return;
		}
		s = MSG_ReadString( msg );
		Cbuf_AddText( s );
		Cbuf_AddText( "\n" );
		return;
	}
	// print command from somewhere
	if( !strcmp( c, "print" ) ) {
		// CA_CONNECTING is allowed, because old servers send protocol mismatch connection error message with it
		if( ( ( cls.state != CA_UNINITIALIZED && cls.state != CA_DISCONNECTED ) &&
			  NET_CompareAddress( address, &cls.serveraddress ) ) ||
			( rcon_address->string[0] != '\0' && NET_CompareAddress( address, &cls.rconaddress ) ) ) {
			s = MSG_ReadString( msg );
			Com_Printf( "%s", s );
			return;
		} else {
			Com_Printf( "Print packet from unknown host, ignored\n" );
			return;
		}
	}

	// ping from somewhere
	if( !strcmp( c, "ping" ) ) {
		// send any args back with the acknowledgement
		Netchan_OutOfBandPrint( socket, address, "ack %s", Cmd_Args() );
		return;
	}

	// ack from somewhere
	if( !strcmp( c, "ack" ) ) {
		return;
	}

	// challenge from the server we are connecting to
	if( !strcmp( c, "challenge" ) ) {
		// these two are from Q3
		if( cls.state != CA_CONNECTING ) {
			Com_Printf( "challenge packet while not connecting, ignored\n" );
			return;
		}
		if( !NET_CompareAddress( address, &cls.serveraddress ) ) {
			Com_Printf( "challenge from a different address, ignored\n" );
			Com_Printf( "Was %s", NET_AddressToString( address ) );
			Com_Printf( " should have been %s\n", NET_AddressToString( &cls.serveraddress ) );
			return;
		}

		cls.challenge = atoi( Cmd_Argv( 1 ) );
		//wsw : r1q2[start]
		//r1: reset the timer so we don't send dup. getchallenges
		cls.connect_time = Sys_Milliseconds();
		//wsw : r1q2[end]
		CL_SendConnectPacket();
		return;
	}

	// echo request from server
	if( !strcmp( c, "echo" ) ) {
		Netchan_OutOfBandPrint( socket, address, "%s", Cmd_Argv( 1 ) );
		return;
	}

	Com_Printf( "Unknown connectionless packet from %s\n%s\n", NET_AddressToString( address ), c );
}

static bool CL_ProcessPacket( netchan_t *netchan, msg_t *msg ) {
	int zerror;

	if( !Netchan_Process( netchan, msg ) ) {
		return false; // wasn't accepted for some reason

	}
	// now if compressed, expand it
	MSG_BeginReading( msg );
	MSG_ReadInt32( msg ); // sequence
	MSG_ReadInt32( msg ); // sequence_ack
	if( msg->compressed ) {
		zerror = Netchan_DecompressMessage( msg );
		if( zerror < 0 ) {
			// compression error. Drop the packet
			Com_Printf( "CL_ProcessPacket: Compression error %i. Dropping packet\n", zerror );
			return false;
		}
	}

	return true;
}

void CL_ReadPackets() {
	msg_t msg;
	uint8_t msgData[MAX_MSGLEN];
	int ret;
	socket_t *socket;
	netadr_t address;

	socket_t* sockets [] =
	{
		&cls.socket_loopback,
		&cls.socket_udp,
		&cls.socket_udp6,
	};

	MSG_Init( &msg, msgData, sizeof( msgData ) );

	for( size_t socketind = 0; socketind < ARRAY_COUNT( sockets ); socketind++ ) {
		socket = sockets[socketind];

		while( socket->open && ( ret = NET_GetPacket( socket, &address, &msg ) ) != 0 ) {
			if( ret == -1 ) {
				Com_Printf( "Error receiving packet with %s: %s\n", NET_SocketToString( socket ), NET_ErrorString() );
				if( cls.reliable && cls.socket == socket ) {
					CL_Disconnect( va( "Error receiving packet: %s\n", NET_ErrorString() ) );
				}

				continue;
			}

			// remote command packet
			if( *(int *)msg.data == -1 ) {
				CL_ConnectionlessPacket( socket, &address, &msg );
				continue;
			}

			if( cls.demo.playing ) {
				// only allow connectionless packets during demo playback
				continue;
			}

			if( cls.state == CA_DISCONNECTED || cls.state == CA_CONNECTING ) {
				Com_DPrintf( "%s: Not connected\n", NET_AddressToString( &address ) );
				continue; // dump it if not connected
			}

			if( msg.cursize < 8 ) {
				//wsw : r1q2[start]
				//r1: delegated to DPrintf (someone could spam your console with crap otherwise)
				Com_DPrintf( "%s: Runt packet\n", NET_AddressToString( &address ) );
				//wsw : r1q2[end]
				continue;
			}

			//
			// packet from server
			//
			if( !NET_CompareAddress( &address, &cls.netchan.remoteAddress ) ) {
				Com_DPrintf( "%s: Sequenced packet without connection\n", NET_AddressToString( &address ) );
				continue;
			}
			if( !CL_ProcessPacket( &cls.netchan, &msg ) ) {
				continue; // wasn't accepted for some reason, like only one fragment of bigger message

			}
			CL_ParseServerMessage( &msg );
			cls.lastPacketReceivedTime = cls.realtime;
		}
	}

	if( cls.demo.playing ) {
		return;
	}

	// not expected, but could happen if cls.realtime is cleared and lastPacketReceivedTime is not
	if( cls.lastPacketReceivedTime > cls.realtime ) {
		cls.lastPacketReceivedTime = cls.realtime;
	}

	// check timeout
	if( cls.state >= CA_HANDSHAKE && cls.lastPacketReceivedTime ) {
		if( cls.lastPacketReceivedTime + cl_timeout->value * 1000 < cls.realtime ) {
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

static void CL_Userinfo_f() {
	Com_Printf( "User info settings:\n" );
	Info_Print( Cvar_Userinfo() );
}

static int precache_spawncount;

void CL_FinishConnect() {
	CL_GameModule_Init();
	CL_AddReliableCommand( va( "begin %i\n", precache_spawncount ) );
}

/*
* CL_Precache_f
*
* The server will send this command right
* before allowing the client into the server
*/
void CL_Precache_f() {
	if( cls.demo.playing ) {
		if( !cls.demo.play_jump ) {
			CL_GameModule_Init();
		} else {
			CL_GameModule_Reset();
			S_StopAllSounds( false );
		}

		cls.demo.play_ignore_next_frametime = true;

		return;
	}

	precache_spawncount = atoi( Cmd_Argv( 1 ) );

	const char * mapname = Cmd_Argv( 2 );
	u64 hash = Hash64( mapname, strlen( mapname ), Hash64( "maps/" ) );

	if( FindMap( StringHash( hash ) ) == NULL ) {
		TempAllocator temp = cls.frame_arena.temp();
		CL_DownloadFile( temp( "maps/{}.bsp", Cmd_Argv( 2 ) ), false );
		return;
	}

	CL_FinishConnect();
}

static void CL_WriteConfiguration() {
	TempAllocator temp = cls.frame_arena.temp();

	DynamicString config( &temp );

	config += "// key bindings\r\n";
	Key_WriteBindings( &config );

	config += "\r\n// variables\r\n";
	Cvar_WriteVariables( &config );

	DynamicString path( &temp, "{}/base/config.cfg", HomeDirPath() );
	if( !WriteFile( &temp, path.c_str(), config.c_str(), config.length() ) ) {
		Com_Printf( "Couldn't write %s.\n", path.c_str() );
	}
}

void CL_SetClientState( connstate_t state ) {
	cls.state = state;
	Com_SetClientState( state );

	switch( state ) {
		case CA_DISCONNECTED:
			Con_Close();
			UI_ShowMainMenu();
			CL_SetKeyDest( key_menu );
			break;
		case CA_CONNECTING:
			cls.cgameActive = false;
			Con_Close();
			UI_ShowConnectingScreen();
			CL_SetKeyDest( key_game );
			break;
		case CA_CONNECTED:
			cls.cgameActive = false;
			Con_Close();
			Cvar_FixCheatVars();
			break;
		case CA_ACTIVE:
			Con_Close();
			UI_HideMenu();
			S_StopBackgroundTrack();
			CL_SetKeyDest( key_game );
			break;
		default:
			break;
	}
}

/*
* CL_ShowServerIP_f - wsw : pb : show the ip:port of the server the client is connected to
*/
static void CL_ShowServerIP_f() {
	if( cls.state != CA_CONNECTED && cls.state != CA_ACTIVE ) {
		Com_Printf( "Not connected to a server\n" );
		return;
	}

	Com_Printf( "Connected to server:\n" );
	Com_Printf( "Name: %s\n", cls.servername );
	Com_Printf( "Address: %s\n", NET_AddressToString( &cls.serveraddress ) );
}

static void CL_InitLocal() {
	cvar_t *name;
	TempAllocator temp = cls.frame_arena.temp();

	cls.state = CA_DISCONNECTED;
	Com_SetClientState( CA_DISCONNECTED );

	//
	// register our variables
	//
	cl_maxfps =     Cvar_Get( "cl_maxfps", "250", CVAR_ARCHIVE );
	cl_pps =        Cvar_Get( "cl_pps", "40", CVAR_ARCHIVE );

	cl_extrapolationTime =  Cvar_Get( "cl_extrapolationTime", "0", CVAR_DEVELOPER );
	cl_extrapolate = Cvar_Get( "cl_extrapolate", "1", CVAR_ARCHIVE );

#if PUBLIC_BUILD
	cl_hotloadAssets = Cvar_Get( "cl_hotloadAssets", "0", CVAR_ARCHIVE );
#else
	cl_hotloadAssets = Cvar_Get( "cl_hotloadAssets", "1", CVAR_ARCHIVE );
#endif

	cl_shownet =        Cvar_Get( "cl_shownet", "0", 0 );
	cl_timeout =        Cvar_Get( "cl_timeout", "120", 0 );

	rcon_client_password =  Cvar_Get( "rcon_password", "", 0 );
	rcon_address =      Cvar_Get( "rcon_address", "", 0 );

	// wsw : debug netcode
	cl_debug_serverCmd =    Cvar_Get( "cl_debug_serverCmd", "0", CVAR_ARCHIVE | CVAR_CHEAT );
	cl_debug_timeDelta =    Cvar_Get( "cl_debug_timeDelta", "0", CVAR_ARCHIVE /*|CVAR_CHEAT*/ );

	cl_devtools = Cvar_Get( "cl_devtools", "0", CVAR_ARCHIVE );

	//
	// userinfo
	//
	info_password =     Cvar_Get( "password", "", CVAR_USERINFO );

	name = Cvar_Get( "name", "", CVAR_USERINFO | CVAR_ARCHIVE );
	if( !name->string[0] ) {
		Cvar_Set( name->name, temp( "user{06}", random_uniform( &cls.rng, 0, 1000000 ) ) );
	}

	Cvar_Get( "hand", "0", CVAR_USERINFO | CVAR_ARCHIVE );

	Cvar_Get( "cl_download_name", "", CVAR_READONLY );
	Cvar_Get( "cl_download_percent", "0", CVAR_READONLY );

	//
	// register our commands
	//
	Cmd_AddCommand( "cmd", CL_ForwardToServer_f );
	Cmd_AddCommand( "requestservers", CL_GetServers_f );
	Cmd_AddCommand( "getinfo", CL_QueryGetInfoMessage_f ); // wsw : jal : ask for server info
	Cmd_AddCommand( "getstatus", CL_QueryGetStatusMessage_f ); // wsw : jal : ask for server info
	Cmd_AddCommand( "userinfo", CL_Userinfo_f );
	Cmd_AddCommand( "disconnect", CL_Disconnect_f );
	Cmd_AddCommand( "record", CL_Record_f );
	Cmd_AddCommand( "stop", CL_Stop_f );
	Cmd_AddCommand( "quit", CL_Quit_f );
	Cmd_AddCommand( "connect", CL_Connect_f );
	Cmd_AddCommand( "reconnect", CL_Reconnect_f );
	Cmd_AddCommand( "rcon", CL_Rcon_f );
	Cmd_AddCommand( "demo", CL_PlayDemo_f );
	Cmd_AddCommand( "yolodemo", CL_YoloDemo_f );
	Cmd_AddCommand( "next", CL_SetNext_f );
	Cmd_AddCommand( "pingserver", CL_PingServer_f );
	Cmd_AddCommand( "demopause", CL_PauseDemo_f );
	Cmd_AddCommand( "demojump", CL_DemoJump_f );
	Cmd_AddCommand( "showserverip", CL_ShowServerIP_f );

	Cmd_SetCompletionFunc( "demo", CL_DemoComplete );
	Cmd_SetCompletionFunc( "yolodemo", CL_DemoComplete );
}

static void CL_ShutdownLocal() {
	cls.state = CA_UNINITIALIZED;
	Com_SetClientState( CA_UNINITIALIZED );

	Cmd_RemoveCommand( "cmd" );
	Cmd_RemoveCommand( "requestservers" );
	Cmd_RemoveCommand( "getinfo" );
	Cmd_RemoveCommand( "getstatus" );
	Cmd_RemoveCommand( "userinfo" );
	Cmd_RemoveCommand( "disconnect" );
	Cmd_RemoveCommand( "record" );
	Cmd_RemoveCommand( "stop" );
	Cmd_RemoveCommand( "quit" );
	Cmd_RemoveCommand( "connect" );
	Cmd_RemoveCommand( "reconnect" );
	Cmd_RemoveCommand( "rcon" );
	Cmd_RemoveCommand( "demo" );
	Cmd_RemoveCommand( "yolodemo" );
	Cmd_RemoveCommand( "next" );
	Cmd_RemoveCommand( "pingserver" );
	Cmd_RemoveCommand( "demopause" );
	Cmd_RemoveCommand( "demojump" );
	Cmd_RemoveCommand( "showserverip" );
}

//============================================================================

/*
* CL_AdjustServerTime - adjust delta to new frame snap timestamp
*/
void CL_AdjustServerTime( unsigned int gameMsec ) {
	ZoneScoped;

	// hurry up if coming late (unless in demos)
	if( !cls.demo.playing ) {
		if( ( cl.newServerTimeDelta < cl.serverTimeDelta ) && gameMsec > 0 ) {
			cl.serverTimeDelta--;
		}
		if( cl.newServerTimeDelta > cl.serverTimeDelta ) {
			cl.serverTimeDelta++;
		}
	}

	cl.serverTime = cls.gametime + cl.serverTimeDelta;

	// it launches a new snapshot when the timestamp of the CURRENT snap is reached.
	if( cl.pendingSnapNum && ( cl.serverTime >= cl.snapShots[cl.currentSnapNum & UPDATE_MASK].serverTime ) ) {
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
	int i, count;
	double delta;
	snapshot_t  *snap;

	if( cls.demo.playing ) {
		if( cl.currentSnapNum <= 0 ) { // if first snap
			return cl.serverTimeDeltas[cl.pendingSnapNum & MASK_TIMEDELTAS_BACKUP];
		}

		return cl.serverTimeDeltas[cl.currentSnapNum & MASK_TIMEDELTAS_BACKUP];
	}

	i = cl.receivedSnapNum - Min2( MAX_TIMEDELTAS_BACKUP, 8 );
	if( i < 0 ) {
		i = 0;
	}

	for( delta = 0, count = 0; i <= cl.receivedSnapNum; i++ ) {
		snap = &cl.snapShots[i & UPDATE_MASK];
		if( snap->valid && snap->serverFrame == i ) {
			delta += (double)cl.serverTimeDeltas[i & MASK_TIMEDELTAS_BACKUP];
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
void CL_UpdateSnapshot() {
	ZoneScoped;

	snapshot_t  *snap;
	int i;

	// see if there is any pending snap to be fired
	if( !cl.pendingSnapNum && ( cl.currentSnapNum != cl.receivedSnapNum ) ) {
		snap = NULL;
		for( i = cl.currentSnapNum + 1; i <= cl.receivedSnapNum; i++ ) {
			if( cl.snapShots[i & UPDATE_MASK].valid && ( cl.snapShots[i & UPDATE_MASK].serverFrame > cl.currentSnapNum ) ) {
				snap = &cl.snapShots[i & UPDATE_MASK];
				//torbenh: this break was the source of the lag bug at cl_fps < sv_pps
				//break;
			}
		}

		if( snap ) { // valid pending snap found
			cl.pendingSnapNum = snap->serverFrame;

			cl.newServerTimeDelta = CL_SmoothTimeDeltas();

			if( cl_extrapolationTime->modified ) {
				if( cl_extrapolationTime->integer > (int)cl.snapFrameTime - 1 ) {
					Cvar_ForceSet( "cl_extrapolationTime", va( "%i", (int)cl.snapFrameTime - 1 ) );
				} else if( cl_extrapolationTime->integer < 0 ) {
					Cvar_ForceSet( "cl_extrapolationTime", "0" );
				}

				cl_extrapolationTime->modified = false;
			}

			if( !cls.demo.playing && cl_extrapolate->integer ) {
				cl.newServerTimeDelta += cl_extrapolationTime->integer;
			}

			// if we don't have current snap (or delay is too big) don't wait to fire the pending one
			if( ( !cls.demo.play_jump && cl.currentSnapNum <= 0 ) ||
				( !cls.demo.playing && Abs( cl.newServerTimeDelta - cl.serverTimeDelta ) > 200 ) ) {
				cl.serverTimeDelta = cl.newServerTimeDelta;
			}
		}
	}
}

void CL_Netchan_Transmit( msg_t *msg ) {
	// if we got here with unsent fragments, fire them all now
	Netchan_PushAllFragments( &cls.netchan );

	if( msg->cursize > 60 ) {
		int zerror = Netchan_CompressMessage( msg );
		if( zerror < 0 ) { // it's compression error, just send uncompressed
			Com_DPrintf( "CL_Netchan_Transmit (ignoring compression): Compression error %i\n", zerror );
		}
	}

	Netchan_Transmit( &cls.netchan, msg );
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
		Cvar_ForceSet( "cl_pps", va( "%s", cl_pps->dvalue ) );
	}

	elapsedTime = cls.realtime - lastPacketTime;
	float minTime = ( 1000.0f / cl_pps->value );

	// don't let cl_pps be smaller than sv_pps
	if( cls.state == CA_ACTIVE && !cls.demo.playing && cl.snapFrameTime ) {
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
	msg_t message;
	uint8_t messageData[MAX_MSGLEN];

	if( cls.state == CA_DISCONNECTED || cls.state == CA_CONNECTING ) {
		return;
	}

	if( cls.demo.playing ) {
		return;
	}

	MSG_Init( &message, messageData, sizeof( messageData ) );
	MSG_Clear( &message );

	// send only reliable commands during connecting time
	if( cls.state < CA_ACTIVE ) {
		if( sendNow || cls.realtime > 100 + cls.lastPacketSentTime ) {
			// write the command ack
			if( !cls.reliable ) {
				MSG_WriteUint8( &message, clc_svcack );
				MSG_WriteIntBase128( &message, cls.lastExecutedServerCommand );
			}
			//write up the clc commands
			CL_UpdateClientCommandsToServer( &message );
			if( message.cursize > 0 ) {
				CL_Netchan_Transmit( &message );
			}
		}
	} else if( sendNow || CL_MaxPacketsReached() ) {
		// write the command ack
		if( !cls.reliable ) {
			MSG_WriteUint8( &message, clc_svcack );
			MSG_WriteIntBase128( &message, cls.lastExecutedServerCommand );
		}
		// send a userinfo update if needed
		if( userinfo_modified ) {
			userinfo_modified = false;
			CL_AddReliableCommand( va( "usri \"%s\"", Cvar_Userinfo() ) );
		}
		CL_UpdateClientCommandsToServer( &message );
		CL_WriteUcmdsToMessage( &message );
		if( message.cursize > 0 ) {
			CL_Netchan_Transmit( &message );
		}
	}
}

static void CL_NetFrame( int realMsec, int gameMsec ) {
	ZoneScoped;

	// read packets from server
	if( realMsec > 5000 ) { // if in the debugger last frame, don't timeout
		cls.lastPacketReceivedTime = cls.realtime;
	}

	if( cls.demo.playing ) {
		CL_ReadDemoPackets(); // fetch results from demo file
	}
	CL_ReadPackets(); // fetch results from server

	// send packets to server
	if( cls.netchan.unsentFragments ) {
		Netchan_TransmitNextFragment( &cls.netchan );
	} else {
		CL_SendMessagesToServer( false );
	}

	// resend a connection request if necessary
	CL_CheckForResend();
	CL_CheckDownloadTimeout();

	CL_ServerListFrame();
}

void CL_Frame( int realMsec, int gameMsec ) {
	ZoneScoped;

	TracyPlot( "Client frame arena max utilisation", cls.frame_arena.max_utilisation() );
	cls.frame_arena.clear();

	u64 entropy[ 2 ];
	CSPRNG_Bytes( entropy, sizeof( entropy ) );
	cls.rng = new_rng( entropy[ 0 ], entropy[ 1 ] );

	static int allRealMsec = 0, allGameMsec = 0, extraMsec = 0;
	static float roundingMsec = 0.0f;
	int minMsec;
	float maxFps;

	cls.monotonicTime += realMsec;
	cls.realtime += realMsec;

	if( cls.demo.playing && cls.demo.play_ignore_next_frametime ) {
		gameMsec = 0;
		cls.demo.play_ignore_next_frametime = false;
	}

	if( cls.demo.playing ) {
		if( cls.demo.paused ) {
			gameMsec = 0;
		} else {
			CL_LatchedDemoJump();
		}
	}

	cls.gametime += gameMsec;

	allRealMsec += realMsec;
	allGameMsec += gameMsec;

	DoneHotloadingAssets();

	if( cl_hotloadAssets->integer != 0 ) {
		static s64 last_hotload_time = 0;
		static bool last_focused = true;

		bool focused = IsWindowFocused();
		bool just_became_focused = focused && !last_focused;

		// hotload assets when the window regains focus or every 1 second when not focused
		if( just_became_focused || ( !focused && cls.monotonicTime - last_hotload_time >= 1000 ) ) {
			TempAllocator temp = cls.frame_arena.temp();
			HotloadAssets( &temp );

			last_hotload_time = cls.monotonicTime;
		}

		last_focused = focused;
	}

	CL_UpdateSnapshot();
	CL_AdjustServerTime( gameMsec );
	CL_UserInputFrame( realMsec );
	CL_NetFrame( realMsec, gameMsec );
	PumpDownloads();

	const int absMinFps = 24;

	// do not allow setting cl_maxfps to very low values to prevent cheating
	if( cl_maxfps->integer < absMinFps ) {
		Cvar_ForceSet( "cl_maxfps", va( "%i", absMinFps ) );
	}
	maxFps = IsWindowFocused() ? cl_maxfps->value : absMinFps;
	minMsec = Max2( 1000.0f / maxFps, 1.0f );
	roundingMsec += Max2( 1000.0f / maxFps, 1.0f ) - minMsec;

	if( roundingMsec >= 1.0f ) {
		minMsec += (int)roundingMsec;
		roundingMsec -= (int)roundingMsec;
	}

	if( allRealMsec + extraMsec < minMsec ) {
		// let CPU sleep while minimized
		bool sleep = cls.state == CA_DISCONNECTED || !IsWindowFocused();

		if( sleep && minMsec - extraMsec > 1 ) {
			Sys_Sleep( minMsec - extraMsec - 1 );
		}
		return;
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

	SCR_UpdateScreen();
	RendererSubmitFrame();

	// update audio
	if( cls.state != CA_ACTIVE ) {
		S_Update( Vec3( 0 ), Vec3( 0 ), axis_identity );
	}

	allRealMsec = 0;
	allGameMsec = 0;

	cl.prevviewangles = cl.viewangles;

	cls.framecount++;

	SwapBuffers();
}

void CL_Init() {
	ZoneScoped;

	constexpr size_t frame_arena_size = 1024 * 1024; // 1MB
	void * frame_arena_memory = ALLOC_SIZE( sys_allocator, frame_arena_size, 16 );
	cls.frame_arena = ArenaAllocator( frame_arena_memory, frame_arena_size );

	u64 entropy[ 2 ];
	CSPRNG_Bytes( entropy, sizeof( entropy ) );
	cls.rng = new_rng( entropy[ 0 ], entropy[ 1 ] );

	cls.monotonicTime = 0;

	netadr_t address;

	assert( !cl_initialized );

	cl_initialized = true;

	InitThreadPool();

	ThreadPoolDo( []( TempAllocator * temp, void * data ) {
		InitAssets( temp );
	} );

	VID_Init();

	ThreadPoolFinish();

	InitRenderer();
	InitMaps();

	cls.white_material = FindMaterial( "$whiteimage" );

	if( !S_Init() ) {
		Com_Printf( S_COLOR_RED "Couldn't initialise audio engine\n" );
	}

	CL_ClearState();

	// IPv4
	NET_InitAddress( &address, NA_IP );
	if( !NET_OpenSocket( &cls.socket_udp, SOCKET_UDP, &address, false ) ) {
		Com_Error( ERR_FATAL, "Couldn't open UDP socket: %s", NET_ErrorString() );
	}

	// IPv6
	NET_InitAddress( &address, NA_IP6 );
	if( !NET_OpenSocket( &cls.socket_udp6, SOCKET_UDP, &address, false ) ) {
		Com_Printf( "Error: Couldn't open UDP6 socket: %s", NET_ErrorString() );
	}

	SCR_InitScreen();

	CL_InitLocal();
	CL_InitInput();

	InitDownloads();

	CL_InitImGui();
	UI_Init();

	UI_ShowMainMenu();

	CL_InitServerList();

	Mem_DebugCheckSentinelsGlobal();
}

/*
* CL_Shutdown
*
* FIXME: this is a callback from Sys_Quit and Com_Error.  It would be better
* to run quit through here before the final handoff to the sys code.
*/
void CL_Shutdown() {
	if( !cl_initialized ) {
		return;
	}

	S_StopAllSounds( true );

	CL_ShutDownServerList();

	CL_WriteConfiguration();

	CL_Disconnect( NULL );
	NET_CloseSocket( &cls.socket_udp );
	NET_CloseSocket( &cls.socket_udp6 );
	// TOCHECK: Shouldn't we close the TCP socket too?
	if( cls.servername ) {
		Mem_ZoneFree( cls.servername );
		cls.servername = NULL;
	}

	UI_Shutdown();
	CL_ShutdownImGui();

	CL_GameModule_Shutdown();
	S_Shutdown();
	ShutdownMaps();
	ShutdownRenderer();
	DestroyWindow();

	ShutdownDownloads();

	CL_ShutdownLocal();

	ShutdownThreadPool();

	Con_Shutdown();

	ShutdownAssets();

	cls.state = CA_UNINITIALIZED;
	cl_initialized = false;

	FREE( sys_allocator, cls.frame_arena.get_memory() );
}
