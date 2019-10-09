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
#include "client/renderer/renderer.h"
#include "client/sdl/sdl_window.h"
#include "qcommon/assets.h"
#include "qcommon/asyncstream.h"
#include "qcommon/version.h"
#include "qcommon/hash.h"
#include "qcommon/csprng.h"

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

cvar_t *cl_downloads;
cvar_t *cl_downloads_from_web;
cvar_t *cl_downloads_from_web_timeout;


static char cl_nextString[MAX_STRING_CHARS];
static char cl_connectChain[MAX_STRING_CHARS];

client_static_t cls;
client_state_t cl;

entity_state_t cl_baselines[MAX_EDICTS];

static bool cl_initialized = false;

static async_stream_module_t *cl_async_stream;

//======================================================================


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

/*
* CL_ForwardToServer_f
*/
void CL_ForwardToServer_f( void ) {
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

/*
* CL_ServerDisconnect_f
*/
void CL_ServerDisconnect_f( void ) {
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

	Q_snprintfz( menuparms, sizeof( menuparms ), "menu_open connfailed dropreason %i servername \"%s\" droptype %i rejectmessage \"%s\"",
				 DROP_REASON_CONNTERMINATED, cls.servername, type, reason );

	Cbuf_ExecuteText( EXEC_NOW, menuparms );
}

/*
* CL_Quit
*/
void CL_Quit( void ) {
	CL_Disconnect( NULL );
	Com_Quit();
}

/*
* CL_Quit_f
*/
static void CL_Quit_f( void ) {
	CL_Quit();
}

/*
* CL_SendConnectPacket
*
* We have gotten a challenge from the server, so try and
* connect.
*/
static void CL_SendConnectPacket( void ) {
	userinfo_modified = false;

	Netchan_OutOfBandPrint( cls.socket, &cls.serveraddress, "connect %i %i %i \"%s\"\n",
							APP_PROTOCOL_VERSION, Netchan_GamePort(), cls.challenge, Cvar_Userinfo() );
}

/*
* CL_CheckForResend
*
* Resend a connect message if the last one has timed out
*/
static void CL_CheckForResend( void ) {
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
		if( realtime - cls.connect_time < 3000 ) {
			return;
		}

		if( realtime - cls.connect_time < 10000 ) {
			return;
		}

		CL_Disconnect( "Connection timed out" );
	}
}

/*
* CL_Connect
*/
static void CL_Connect( const char *servername, socket_type_t type, netadr_t *address, const char *serverchain ) {
	netadr_t socketaddress;

	cl_connectChain[0] = '\0';
	cl_nextString[0] = '\0';

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

	if( serverchain[0] ) {
		Q_strncpyz( cl_connectChain, serverchain, sizeof( cl_connectChain ) );
	}

	cls.connect_time = -99999; // CL_CheckForResend() will fire immediately
	cls.connect_count = 0;
	cls.rejected = false;
	cls.lastPacketReceivedTime = cls.realtime; // reset the timeout limit
	cls.mv = false;
}

/*
* CL_Connect_Cmd_f
*/
static void CL_Connect_Cmd_f( socket_type_t socket ) {
	netadr_t serveraddress;
	char *servername, password[64];
	const char *extension;
	char *connectstring, *connectstring_base;
	const char *tmp;
	const char *serverchain;

	if( Cmd_Argc() < 2 ) {
		Com_Printf( "Usage: %s <server>\n", Cmd_Argv( 0 ) );
		return;
	}

	connectstring_base = TempCopyString( Cmd_Argv( 1 ) );
	connectstring = connectstring_base;
	serverchain = Cmd_Argc() >= 3 ? Cmd_Argv( 2 ) : "";

	if( !Q_strnicmp( connectstring, APP_URI_SCHEME, strlen( APP_URI_SCHEME ) ) ) {
		connectstring += strlen( APP_URI_SCHEME );
	}

	extension = COM_FileExtension( connectstring );
	if( extension && !Q_stricmp( extension, APP_DEMO_EXTENSION_STR ) ) {
		char *temp;
		size_t temp_size;
		const char *http_scheme = "http://";

		if( !Q_strnicmp( connectstring, http_scheme, strlen( http_scheme ) ) ) {
			connectstring += strlen( http_scheme );
		}

		temp_size = strlen( "demo " ) + strlen( http_scheme ) + strlen( connectstring ) + 1;
		temp = ( char * ) Mem_TempMalloc( temp_size );
		Q_snprintfz( temp, temp_size, "demo %s%s", http_scheme, connectstring );

		Cbuf_ExecuteText( EXEC_NOW, temp );

		Mem_TempFree( temp );
		Mem_TempFree( connectstring_base );
		return;
	}

	if( ( tmp = Q_strrstr( connectstring, "@" ) ) != NULL ) {
		Q_strncpyz( password, connectstring, min( sizeof( password ),( tmp - connectstring + 1 ) ) );
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
	CL_Connect( servername, ( serveraddress.type == NA_LOOPBACK ? SOCKET_LOOPBACK : socket ),
				&serveraddress, serverchain );

	Mem_TempFree( servername );
	Mem_TempFree( connectstring_base );
}

/*
* CL_Connect_f
*/
static void CL_Connect_f( void ) {
	CL_Connect_Cmd_f( SOCKET_UDP );
}


/*
* CL_Rcon_f
*
* Send the rest of the command line over as
* an unconnected command.
*/
static void CL_Rcon_f( void ) {
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

/*
* CL_GetClipboardData
*/
char *CL_GetClipboardData( void ) {
	return Sys_GetClipboardData();
}

/*
* CL_SetClipboardData
*/
void CL_SetClipboardData( const char *data ) {
	Sys_SetClipboardData( data );
}

/*
* CL_FreeClipboardData
*/
void CL_FreeClipboardData( char *data ) {
	Sys_FreeClipboardData( data );
}

/*
* CL_GetKeyDest
*/
keydest_t CL_GetKeyDest( void ) {
	return cls.key_dest;
}

/*
* CL_SetKeyDest
*/
void CL_SetKeyDest( keydest_t key_dest ) {
	if( cls.key_dest != key_dest ) {
		CL_ClearInputState();
		cls.key_dest = key_dest;
		Con_SetMessageMode();
	}
}

/*
* CL_SetOldKeyDest
*/
void CL_SetOldKeyDest( keydest_t key_dest ) {
	cls.old_key_dest = key_dest;
}

/*
* CL_GetBaseServerURL
*/
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

/*
* CL_ResetServerCount
*/
void CL_ResetServerCount( void ) {
	cl.servercount = -1;
}

/*
* CL_ClearState
*/
void CL_ClearState( void ) {
	if( cl.cms ) {
		CM_ReleaseReference( cl.cms );
	}

	if( cl.frames_areabits ) {
		Mem_Free( cl.frames_areabits );
	}

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
static void CL_SetNext_f( void ) {
	if( Cmd_Argc() < 2 ) {
		Com_Printf( "USAGE: next <commands>\n" );
		return;
	}

	// jalfixme: I'm afraid of this being too powerful, since it basically
	// is allowed to execute everything. Shall we check for something?
	Q_strncpyz( cl_nextString, Cmd_Args(), sizeof( cl_nextString ) );
	Com_Printf( "NEXT: %s\n", cl_nextString );
}


/*
* CL_ExecuteNext
*/
static void CL_ExecuteNext( void ) {
	if( !strlen( cl_nextString ) ) {
		return;
	}

	Cbuf_ExecuteText( EXEC_APPEND, cl_nextString );
	memset( cl_nextString, 0, sizeof( cl_nextString ) );
}

/*
* CL_Disconnect_SendCommand
*
* Sends a disconnect message to the server
*/
static void CL_Disconnect_SendCommand( void ) {
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
	char menuparms[MAX_STRING_CHARS];
	bool wasconnecting;

	// We have to shut down webdownloading first
	if( cls.download.web && !cls.download.disconnect ) {
		cls.download.disconnect = true;
		return;
	}

	if( cls.state == CA_UNINITIALIZED ) {
		return;
	}
	if( cls.state == CA_DISCONNECTED ) {
		goto done;
	}

	if( cls.state < CA_CONNECTED ) {
		wasconnecting = true;
	} else {
		wasconnecting = false;
	}

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
	FS_RemovePurePaks();

	Com_FreePureList( &cls.purelist );

	cls.sv_pure = false;

	// udp is kept open all the time, for connectionless messages
	if( cls.socket && cls.socket->type != SOCKET_UDP ) {
		NET_CloseSocket( cls.socket );
	}

	cls.socket = NULL;
	cls.reliable = false;
	cls.mv = false;

	if( cls.httpbaseurl ) {
		Mem_Free( cls.httpbaseurl );
		cls.httpbaseurl = NULL;
	}

	CL_RestartMedia();

	CL_ClearState();
	CL_SetClientState( CA_DISCONNECTED );

	if( cls.download.requestname ) {
		cls.download.pending_reconnect = false;
		cls.download.cancelled = true;
		CL_DownloadDone();
	}

	if( cl_connectChain[0] == '\0' ) {
		if( message != NULL ) {
			Q_snprintfz( menuparms, sizeof( menuparms ), "menu_open connfailed dropreason %i servername \"%s\" droptype %i rejectmessage \"%s\"",
						 ( wasconnecting ? DROP_REASON_CONNFAILED : DROP_REASON_CONNERROR ), cls.servername, DROP_TYPE_GENERAL, message );

			Cbuf_ExecuteText( EXEC_NOW, menuparms );
		}
	} else {
		const char *s = strchr( cl_connectChain, ',' );
		if( s ) {
			cl_connectChain[s - cl_connectChain] = '\0';
		} else {
			s = cl_connectChain + strlen( cl_connectChain ) - 1;
		}
		Q_snprintfz( cl_nextString, sizeof( cl_nextString ), "connect \"%s\" \"%s\"", cl_connectChain, s + 1 );
	}

done:
	SCR_EndLoadingPlaque(); // get rid of loading plaque

	// in case we disconnect while in download phase
	CL_FreeDownloadList();

	CL_ExecuteNext(); // start next action if any is defined
}

void CL_Disconnect_f( void ) {
	cl_connectChain[0] = '\0';
	cl_nextString[0] = '\0';

	// We have to shut down webdownloading first
	if( cls.download.web ) {
		cls.download.disconnect = true;
		return;
	}

	CL_Disconnect( NULL );
}

/*
* CL_Changing_f
*
* Just sent as a hint to the client that they should
* drop to full console
*/
void CL_Changing_f( void ) {
	//ZOID
	//if we are downloading, we don't change!  This so we don't suddenly stop downloading a map
	if( cls.download.filenum || cls.download.web ) {
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
void CL_ServerReconnect_f( void ) {
	if( cls.demo.playing ) {
		return;
	}

	//if we are downloading, we don't change!  This so we don't suddenly stop downloading a map
	if( cls.download.filenum || cls.download.web ) {
		cls.download.pending_reconnect = true;
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
void CL_Reconnect_f( void ) {
	char *servername;
	socket_type_t servertype;
	netadr_t serveraddress;

	if( !cls.servername ) {
		Com_Printf( "Can't reconnect, never connected\n" );
		return;
	}

	cl_connectChain[0] = '\0';
	cl_nextString[0] = '\0';

	servername = TempCopyString( cls.servername );
	servertype = cls.servertype;
	serveraddress = cls.serveraddress;
	CL_Disconnect( NULL );
	CL_Connect( servername, servertype, &serveraddress, "" );
	Mem_TempFree( servername );
}

/*
* CL_ConnectionlessPacket
*
* Responses to broadcasts, etc
*/
static void CL_ConnectionlessPacket( const socket_t *socket, const netadr_t *address, msg_t *msg ) {
	char *s;
	char *c;

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

		Netchan_Setup( &cls.netchan, socket, address, Netchan_GamePort() );
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
			Q_snprintfz( menuparms, sizeof( menuparms ), "menu_open connfailed dropreason %i servername \"%s\" droptype %i rejectmessage \"%s\"",
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

/*
* CL_ProcessPacket
*/
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

/*
* CL_ReadPackets
*/
void CL_ReadPackets( void ) {
	static msg_t msg;
	static uint8_t msgData[MAX_MSGLEN];
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

/*
* CL_Userinfo_f
*/
static void CL_Userinfo_f( void ) {
	Com_Printf( "User info settings:\n" );
	Info_Print( Cvar_Userinfo() );
}

static int precache_check; // for autodownload of precache items
static int precache_spawncount;
static int precache_tex;
static int precache_pure;

#define PLAYER_MULT 5

// ENV_CNT is map load
#define ENV_CNT ( CS_PLAYERINFOS + MAX_CLIENTS * PLAYER_MULT )
#define TEXTURE_CNT ( ENV_CNT + 1 )

static unsigned int CL_LoadMap( const char *name ) {
	int i;
	int areas;

	unsigned int map_checksum;

	assert( !cl.cms );

	// if local server is running, share the collision model,
	// increasing the ref counter
	if( Com_ServerState() ) {
		cl.cms = Com_ServerCM( &map_checksum );
	} else {
		cl.cms = CM_New( NULL );
		CM_LoadMap( cl.cms, name, true, &map_checksum );
	}

	assert( cl.cms );

	// allocate memory for areabits
	areas = CM_NumAreas( cl.cms );
	areas *= CM_AreaRowSize( cl.cms );

	cl.frames_areabits = ( uint8_t * ) Mem_ZoneMalloc( UPDATE_BACKUP * areas );
	for( i = 0; i < UPDATE_BACKUP; i++ ) {
		cl.snapShots[i].areabytes = areas;
		cl.snapShots[i].areabits = cl.frames_areabits + i * areas;
	}

	// check memory integrity
	Mem_DebugCheckSentinelsGlobal();

	return map_checksum;
}

void CL_RequestNextDownload( void ) {
	char tempname[MAX_CONFIGSTRING_CHARS + 4];
	purelist_t *purefile;
	int i;

	if( cls.state != CA_CONNECTED ) {
		return;
	}

	// pure list
	if( cls.sv_pure ) {
		// skip
		if( !cl_downloads->integer ) {
			precache_pure = -1;
		}

		// try downloading
		if( precache_pure != -1 ) {
			i = 0;
			purefile = cls.purelist;
			while( i < precache_pure && purefile ) {
				purefile = purefile->next;
				i++;
			}

			while( purefile ) {
				precache_pure++;
				if( !CL_CheckOrDownloadFile( purefile->filename ) ) {
					return;
				}
				purefile = purefile->next;
			}
			precache_pure = -1;
		}

		if( precache_pure == -1 ) {
			bool failed = false;
			char message[MAX_STRING_CHARS];

			Q_snprintfz( message, sizeof( message ), "Pure check failed:" );

			purefile = cls.purelist;
			while( purefile ) {
				Com_DPrintf( "Adding pure file: %s\n", purefile->filename );
				if( !FS_AddPurePak( purefile->checksum ) ) {
					failed = true;
					Q_strncatz( message, " ", sizeof( message ) );
					Q_strncatz( message, purefile->filename, sizeof( message ) );
				}
				purefile = purefile->next;
			}

			if( failed ) {
				Com_Error( ERR_DROP, "%s", message );
				return;
			}
		}
	}

	// skip if download not allowed
	if( !cl_downloads->integer && precache_check < ENV_CNT ) {
		precache_check = ENV_CNT;
	}

	//ZOID
	if( precache_check == CS_WORLDMODEL ) { // confirm map
		precache_check = CS_MODELS; // 0 isn't used

		if( !CL_CheckOrDownloadFile( cl.configstrings[CS_WORLDMODEL] ) ) {
			return; // started a download
		}
	}

	if( precache_check >= CS_MODELS && precache_check < CS_MODELS + MAX_MODELS ) {
		while( precache_check < CS_MODELS + MAX_MODELS && cl.configstrings[precache_check][0] ) {
			if( cl.configstrings[precache_check][0] == '*' ||
				cl.configstrings[precache_check][0] == '$' || // disable playermodel downloading for now
				cl.configstrings[precache_check][0] == '#' ) {
				precache_check++;
				continue;
			}

			if( !CL_CheckOrDownloadFile( cl.configstrings[precache_check++] ) ) {
				return; // started a download
			}
		}
		precache_check = CS_SOUNDS;
	}

	if( precache_check >= CS_SOUNDS && precache_check < CS_SOUNDS + MAX_SOUNDS ) {
		if( precache_check == CS_SOUNDS ) {
			precache_check++; // zero is blank

		}
		while( precache_check < CS_SOUNDS + MAX_SOUNDS && cl.configstrings[precache_check][0] ) {
			Q_strncpyz( tempname, cl.configstrings[precache_check++], sizeof( tempname ) );
			if( !COM_FileExtension( tempname ) ) {
				if( !FS_FirstExtension( tempname, SOUND_EXTENSIONS, NUM_SOUND_EXTENSIONS ) ) {
					COM_DefaultExtension( tempname, ".wav", sizeof( tempname ) );
					if( !CL_CheckOrDownloadFile( tempname ) ) {
						return; // started a download
					}
				}
			} else {
				if( !CL_CheckOrDownloadFile( tempname ) ) {
					return; // started a download
				}
			}
		}
		precache_check = CS_IMAGES;
	}
	if( precache_check >= CS_IMAGES && precache_check < CS_IMAGES + MAX_IMAGES ) {
		if( precache_check == CS_IMAGES ) {
			precache_check++; // zero is blank

		}
		// precache phase completed
		precache_check = ENV_CNT;
	}

	if( precache_check == ENV_CNT ) {
		bool restart = false;
		bool vid_restart = false;
		const char *restart_msg = "";
		unsigned map_checksum;

		// we're done with the download phase, so clear the list
		CL_FreeDownloadList();
		if( cls.pure_restart ) {
			restart = true;
			restart_msg = "Pure server. Restarting media...";
		}
		if( cls.download.successCount ) {
			restart = true;
			vid_restart = true;
			restart_msg = "Files downloaded. Restarting media...";
		}

		if( restart ) {
			Com_Printf( "%s\n", restart_msg );

			if( vid_restart ) {
				// no media is going to survive a vid_restart...
				Cbuf_ExecuteText( EXEC_NOW, "s_restart 1\n" );
			}
		}

		if( !vid_restart ) {
			CL_RestartMedia();
		}

		cls.download.successCount = 0;

		map_checksum = CL_LoadMap( cl.configstrings[CS_WORLDMODEL] );
		if( map_checksum != (unsigned)atoi( cl.configstrings[CS_MAPCHECKSUM] ) ) {
			Com_Error( ERR_DROP, "Local map version differs from server: %u != '%u'",
					   map_checksum, (unsigned)atoi( cl.configstrings[CS_MAPCHECKSUM] ) );
			return;
		}

		precache_check = TEXTURE_CNT;
	}

	if( precache_check == TEXTURE_CNT ) {
		precache_check = TEXTURE_CNT + 1;
		precache_tex = 0;
	}

	// confirm existance of textures, download any that don't exist
	if( precache_check == TEXTURE_CNT + 1 ) {
		precache_check = TEXTURE_CNT + 999;
	}

	// load client game module
	CL_GameModule_Init();
	CL_AddReliableCommand( va( "begin %i\n", precache_spawncount ) );
}

/*
* CL_Precache_f
*
* The server will send this command right
* before allowing the client into the server
*/
void CL_Precache_f( void ) {
	FS_RemovePurePaks();

	if( cls.demo.playing ) {
		if( !cls.demo.play_jump ) {
			CL_LoadMap( cl.configstrings[CS_WORLDMODEL] );

			CL_GameModule_Init();
		} else {
			CL_GameModule_Reset();
			S_StopAllSounds( false );
		}

		cls.demo.play_ignore_next_frametime = true;

		return;
	}

	precache_pure = 0;
	precache_check = CS_WORLDMODEL;
	precache_spawncount = atoi( Cmd_Argv( 1 ) );

	CL_RequestNextDownload();
}

/*
* CL_WriteConfiguration
*
* Writes key bindings, archived cvars and aliases to a config file
*/
static void CL_WriteConfiguration( const char *name ) {
	int file;

	if( FS_FOpenFile( name, &file, FS_WRITE ) == -1 ) {
		Com_Printf( "Couldn't write %s.\n", name );
		return;
	}

	FS_Printf( file, "\r\n// key bindings\r\n" );
	Key_WriteBindings( file );

	FS_Printf( file, "\r\n// variables\r\n" );
	Cvar_WriteVariables( file );

	FS_Printf( file, "\r\n// aliases\r\n" );
	Cmd_WriteAliases( file );

	FS_FCloseFile( file );
}


/*
* CL_WriteConfig_f
*/
static void CL_WriteConfig_f( void ) {
	char *name;
	int name_size;

	if( Cmd_Argc() != 2 ) {
		Com_Printf( "Usage: writeconfig <filename>\n" );
		return;
	}

	name_size = sizeof( char ) * ( strlen( Cmd_Argv( 1 ) ) + strlen( ".cfg" ) + 1 );
	name = ( char * ) Mem_TempMalloc( name_size );
	Q_strncpyz( name, Cmd_Argv( 1 ), name_size );
	COM_SanitizeFilePath( name );

	if( !COM_ValidateRelativeFilename( name ) ) {
		Com_Printf( "Invalid filename" );
		Mem_TempFree( name );
		return;
	}

	COM_DefaultExtension( name, ".cfg", name_size );

	Com_Printf( "Writing: %s\n", name );
	CL_WriteConfiguration( name );

	Mem_TempFree( name );
}

/*
* CL_SetClientState
*/
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
			UI_HideMenu();
			S_StopBackgroundTrack();
			CL_SetKeyDest( key_game );
			break;
		case CA_CONNECTED:
			cls.cgameActive = false;
			Con_Close();
			Cvar_FixCheatVars();
			break;
		case CA_ACTIVE:
			cl_connectChain[0] = '\0';
			Con_Close();
			UI_HideMenu();
			CL_SetKeyDest( key_game );
			break;
		default:
			break;
	}
}

/*
* CL_GetClientState
*/
connstate_t CL_GetClientState( void ) {
	return cls.state;
}

/*
* CL_InitMedia
*/
void CL_InitMedia( void ) {
	if( cls.mediaInitialized ) {
		return;
	}
	if( cls.state == CA_UNINITIALIZED ) {
		return;
	}

	// random seed to be shared among game modules so pseudo-random stuff is in sync
	if( cls.state != CA_CONNECTED ) {
		srand( time( NULL ) );
	}

	cls.mediaInitialized = true;

	S_StopAllSounds( true );

	SCR_RegisterConsoleMedia();

	// load user interface
	CL_InitImGui();
	UI_Init();

	// check memory integrity
	Mem_DebugCheckSentinelsGlobal();
}

/*
* CL_ShutdownMedia
*/
void CL_ShutdownMedia( void ) {
	if( !cls.mediaInitialized ) {
		return;
	}

	cls.mediaInitialized = false;

	S_StopAllSounds( true );

	// shutdown cgame
	CL_GameModule_Shutdown();
}

/*
* CL_RestartMedia
*/
void CL_RestartMedia( void ) {
	if( cls.mediaInitialized ) {
		// shutdown cgame
		CL_GameModule_Shutdown();

		cls.mediaInitialized = false;
	}

	S_StopAllSounds( true );

	// random seed to be shared among game modules so pseudo-random stuff is in sync
	if( cls.state != CA_CONNECTED ) {
		srand( time( NULL ) );
	}

	cls.mediaInitialized = true;

	// register console font and background
	SCR_RegisterConsoleMedia();

	UI_HideMenu();

	// check memory integrity
	Mem_DebugCheckSentinelsGlobal();
}

/*
* CL_ShowIP_f - wsw : jal : taken from Q3 (it only shows the ip when server was started)
*/
static void CL_ShowIP_f( void ) {
	NET_ShowIP();
}

/*
* CL_ShowServerIP_f - wsw : pb : show the ip:port of the server the client is connected to
*/
static void CL_ShowServerIP_f( void ) {
	if( cls.state != CA_CONNECTED && cls.state != CA_ACTIVE ) {
		Com_Printf( "Not connected to a server\n" );
		return;
	}

	Com_Printf( "Connected to server:\n" );
	Com_Printf( "Name: %s\n", cls.servername );
	Com_Printf( "Address: %s\n", NET_AddressToString( &cls.serveraddress ) );
}

/*
* CL_InitLocal
*/
static void CL_InitLocal( void ) {
	cvar_t *name;

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

	cl_downloads =      Cvar_Get( "cl_downloads", "1", CVAR_ARCHIVE );
	cl_downloads_from_web = Cvar_Get( "cl_downloads_from_web", "1", CVAR_ARCHIVE | CVAR_READONLY );
	cl_downloads_from_web_timeout = Cvar_Get( "cl_downloads_from_web_timeout", "600", CVAR_ARCHIVE );

	//
	// userinfo
	//
	info_password =     Cvar_Get( "password", "", CVAR_USERINFO );

	name = Cvar_Get( "name", "", CVAR_USERINFO | CVAR_ARCHIVE );
	if( !name->string[0] ) {
		Cvar_Set( name->name, "Player" );
	}

	Cvar_Get( "clan", "", CVAR_USERINFO | CVAR_ARCHIVE );
	Cvar_Get( "hand", "0", CVAR_USERINFO | CVAR_ARCHIVE );
	Cvar_Get( "handicap", "0", CVAR_USERINFO | CVAR_ARCHIVE );

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
	Cmd_AddCommand( "writeconfig", CL_WriteConfig_f );
	Cmd_AddCommand( "showip", CL_ShowIP_f ); // jal : wsw : print our ip
	Cmd_AddCommand( "demo", CL_PlayDemo_f );
	Cmd_AddCommand( "next", CL_SetNext_f );
	Cmd_AddCommand( "pingserver", CL_PingServer_f );
	Cmd_AddCommand( "demopause", CL_PauseDemo_f );
	Cmd_AddCommand( "demojump", CL_DemoJump_f );
	Cmd_AddCommand( "showserverip", CL_ShowServerIP_f );
	Cmd_AddCommand( "downloadstatus", CL_DownloadStatus_f );
	Cmd_AddCommand( "downloadcancel", CL_DownloadCancel_f );

	Cmd_SetCompletionFunc( "demo", CL_DemoComplete );
}

/*
* CL_ShutdownLocal
*/
static void CL_ShutdownLocal( void ) {
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
	Cmd_RemoveCommand( "writeconfig" );
	Cmd_RemoveCommand( "showip" );
	Cmd_RemoveCommand( "demo" );
	Cmd_RemoveCommand( "next" );
	Cmd_RemoveCommand( "pingserver" );
	Cmd_RemoveCommand( "demopause" );
	Cmd_RemoveCommand( "demojump" );
	Cmd_RemoveCommand( "showserverip" );
	Cmd_RemoveCommand( "downloadstatus" );
	Cmd_RemoveCommand( "downloadcancel" );
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

/*
* CL_RestartTimeDeltas
*/
void CL_RestartTimeDeltas( int newTimeDelta ) {
	int i;

	cl.serverTimeDelta = cl.newServerTimeDelta = newTimeDelta;
	for( i = 0; i < MAX_TIMEDELTAS_BACKUP; i++ )
		cl.serverTimeDeltas[i] = newTimeDelta;

	if( cl_debug_timeDelta->integer ) {
		Com_Printf( S_COLOR_CYAN "***** timeDelta restarted\n" );
	}
}

/*
* CL_SmoothTimeDeltas
*/
int CL_SmoothTimeDeltas( void ) {
	int i, count;
	double delta;
	snapshot_t  *snap;

	if( cls.demo.playing ) {
		if( cl.currentSnapNum <= 0 ) { // if first snap
			return cl.serverTimeDeltas[cl.pendingSnapNum & MASK_TIMEDELTAS_BACKUP];
		}

		return cl.serverTimeDeltas[cl.currentSnapNum & MASK_TIMEDELTAS_BACKUP];
	}

	i = cl.receivedSnapNum - min( MAX_TIMEDELTAS_BACKUP, 8 );
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
void CL_UpdateSnapshot( void ) {
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
				( !cls.demo.playing && abs( cl.newServerTimeDelta - cl.serverTimeDelta ) > 200 ) ) {
				cl.serverTimeDelta = cl.newServerTimeDelta;
			}
		}
	}
}

/*
* CL_Netchan_Transmit
*/
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

/*
* CL_MaxPacketsReached
*/
static bool CL_MaxPacketsReached( void ) {
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
	if( cls.mv ) {
		minpackettime = ( 1000.0f / 2 );
	} else {
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
	}

	if( elapsedTime < minpackettime ) {
		return false;
	}

	lastPacketTime = cls.realtime;
	return true;
}

/*
* CL_SendMessagesToServer
*/
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

/*
* CL_NetFrame
*/
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

/*
* CL_Frame
*/
void CL_Frame( int realMsec, int gameMsec ) {
	ZoneScoped;

	TracyPlot( "Frame arena max utilisation", cls.frame_arena.max_utilisation() );
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

		bool focused = VID_AppIsActive();
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

	if( cl_maxfps->integer > 0 && !cls.demo.playing ) {
		const int absMinFps = 24;

		// do not allow setting cl_maxfps to very low values to prevent cheating
		if( cl_maxfps->integer < absMinFps ) {
			Cvar_ForceSet( "cl_maxfps", va( "%i", absMinFps ) );
		}
		maxFps = VID_AppIsActive() ? cl_maxfps->value : absMinFps;
		minMsec = max( ( 1000.0f / maxFps ), 1 );
		roundingMsec += max( ( 1000.0f / maxFps ), 1.0f ) - minMsec;
	} else {
		maxFps = 10000.0f;
		minMsec = 1;
		roundingMsec = 0;
	}

	if( roundingMsec >= 1.0f ) {
		minMsec += (int)roundingMsec;
		roundingMsec -= (int)roundingMsec;
	}

	if( allRealMsec + extraMsec < minMsec ) {
		// let CPU sleep while minimized
		bool sleep = cls.state == CA_DISCONNECTED || !VID_AppIsActive() || VID_AppIsMinimized(); // FIXME: not sure about listen server here..

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
	if( host_speeds->integer ) {
		time_before_ref = Sys_Milliseconds();
	}

	u32 viewport_width, viewport_height;
	VID_GetViewportSize( &viewport_width, &viewport_height );
	RendererBeginFrame( viewport_width, viewport_height );

	SCR_UpdateScreen();
	RendererSubmitFrame();
	if( host_speeds->integer ) {
		time_after_ref = Sys_Milliseconds();
	}

	// update audio
	if( cls.state != CA_ACTIVE && !cls.disable_screen ) {
		S_Update( vec3_origin, vec3_origin, axis_identity );
	}

	allRealMsec = 0;
	allGameMsec = 0;

	cls.framecount++;

	VID_Swap();
}

//============================================================================

/*
* CL_AsyncStream_Alloc
*/
static void *CL_AsyncStream_Alloc( size_t size, const char *filename, int fileline ) {
	return _Mem_Alloc( zoneMemPool, size, 0, 0, filename, fileline );
}

/*
* CL_AsyncStream_Free
*/
static void CL_AsyncStream_Free( void *data, const char *filename, int fileline ) {
	_Mem_Free( data, 0, 0, filename, fileline );
}

/*
* CL_InitAsyncStream
*/
static void CL_InitAsyncStream( void ) {
	cl_async_stream = AsyncStream_InitModule( "Client", CL_AsyncStream_Alloc, CL_AsyncStream_Free );
}

/*
* CL_ShutdownAsyncStream
*/
static void CL_ShutdownAsyncStream( void ) {
	if( !cl_async_stream ) {
		return;
	}

	AsyncStream_ShutdownModule( cl_async_stream );
	cl_async_stream = NULL;
}

/*
* CL_AddSessionHttpRequestHeaders
*/
int CL_AddSessionHttpRequestHeaders( const char *url, const char **headers ) {
	static char pH[32];

	if( cls.httpbaseurl && *cls.httpbaseurl ) {
		if( !strncmp( url, cls.httpbaseurl, strlen( cls.httpbaseurl ) ) ) {
			Q_snprintfz( pH, sizeof( pH ), "%i", cl.playernum );

			headers[0] = "X-Client";
			headers[1] = pH;
			headers[2] = "X-Session";
			headers[3] = cls.session;
			return 4;
		}
	}
	return 0;
}

/*
* CL_AsyncStreamRequest
*/
void CL_AsyncStreamRequest( const char *url, const char **headers, int timeout, int resumeFrom,
							size_t ( *read_cb )( const void *, size_t, float, int, const char *, void * ),
							void ( *done_cb )( int, const char *, void * ),
							void ( *header_cb )( const char *, void * ), void *privatep, bool urlencodeUnsafe ) {
	char *tmpUrl = NULL;
	const char *safeUrl;

	if( urlencodeUnsafe ) {
		// urlencode unsafe characters
		size_t allocSize = strlen( url ) * 3 + 1;
		tmpUrl = ( char * )Mem_TempMalloc( allocSize );
		AsyncStream_UrlEncodeUnsafeChars( url, tmpUrl, allocSize );

		safeUrl = tmpUrl;
	} else {
		safeUrl = url;
	}

	AsyncStream_PerformRequestExt( cl_async_stream, safeUrl, "GET", NULL, headers, timeout,
								   resumeFrom, read_cb, done_cb, (async_stream_header_cb_t)header_cb, NULL );

	if( urlencodeUnsafe ) {
		Mem_TempFree( tmpUrl );
	}
}

//============================================================================

/*
* CL_Init
*/
void CL_Init( void ) {
	ZoneScoped;

	constexpr size_t frame_arena_size = 1024 * 1024;
	void * frame_arena_memory = ALLOC_SIZE( sys_allocator, frame_arena_size, 16 );
	cls.frame_arena = ArenaAllocator( frame_arena_memory, frame_arena_size );

	cls.monotonicTime = 0;

	netadr_t address;

	assert( !cl_initialized );

	cl_initialized = true;

	{
		TempAllocator temp = cls.frame_arena.temp();
		InitAssets( &temp );
	}

	Con_Init();

	VID_Init();

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
	cls.disable_screen = true; // don't draw yet

	CL_InitLocal();
	CL_InitInput();

	CL_InitAsyncStream();

	CL_InitMedia();

	UI_ShowMainMenu();

	CL_InitServerList();

	ML_Init();
}

/*
* CL_Shutdown
*
* FIXME: this is a callback from Sys_Quit and Com_Error.  It would be better
* to run quit through here before the final handoff to the sys code.
*/
void CL_Shutdown( void ) {
	if( !cl_initialized ) {
		return;
	}

	S_StopAllSounds( true );

	ML_Shutdown();
	CL_ShutDownServerList();

	CL_WriteConfiguration( "config.cfg" );

	CL_Disconnect( NULL );
	NET_CloseSocket( &cls.socket_udp );
	NET_CloseSocket( &cls.socket_udp6 );
	// TOCHECK: Shouldn't we close the TCP socket too?
	if( cls.servername ) {
		Mem_ZoneFree( cls.servername );
		cls.servername = NULL;
	}

	CL_ShutdownImGui();
	CL_GameModule_Shutdown();
	S_Shutdown();
	CL_ShutdownInput();
	VID_Shutdown();

	CL_ShutdownMedia();

	CL_ShutdownAsyncStream();

	CL_ShutdownLocal();

	SCR_ShutdownScreen();

	Con_Shutdown();

	ShutdownAssets();

	cls.state = CA_UNINITIALIZED;
	cl_initialized = false;

	FREE( sys_allocator, cls.frame_arena.get_memory() );
}
