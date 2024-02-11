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

#include <time.h>

#include "client/client.h"
#include "client/downloads.h"
#include "qcommon/version.h"

struct DownloadInProgress {
	char * path;
	DownloadCompleteCallback callback;
};

static DownloadInProgress download;

static void OnDownloadDone( int http_status, Span< const u8 > data ) {
	Com_Printf( "Download %s: %s (%i)\n", data.ptr != NULL ? "successful" : "failed", download.path, http_status );

	download.callback( download.path, data );

	Free( sys_allocator, download.path );
	download.path = NULL;
}

bool CL_DownloadFile( Span< const char > filename, DownloadCompleteCallback callback ) {
	if( download.path ) {
		Com_Printf( "Already downloading something.\n" );
		return false;
	}

	if( !COM_ValidateRelativeFilename( filename ) ) {
		Com_GGPrint( "Can't download: {}, invalid filename.", filename );
		return false;
	}

	Com_GGPrint( "Asking to download: {}", filename );

	download = { };
	download.path = ( *sys_allocator )( "{}", filename );
	download.callback = callback;

	TempAllocator temp = cls.frame_arena.temp();

	const char * url = temp( "{}/{}", cls.download_url, filename );
	StartDownload( url, OnDownloadDone, NULL, 0 );

	Com_Printf( "Downloading %s\n", url );

	return true;
}

/*
=====================================================================

SERVER CONNECTING MESSAGES

=====================================================================
*/

static void CL_ParseServerData( msg_t *msg ) {
	// wipe the client_state_t struct
	CL_ClearState();
	CL_SetClientState( CA_CONNECTED );

	// parse protocol version number
	u32 i = MSG_ReadUint32( msg );

	if( i != APP_PROTOCOL_VERSION ) {
		if( CL_DemoPlaying() ) {
			Com_Printf( S_COLOR_YELLOW "This demo was recorded with an old version of the game and may be broken!\n" );
			if( !CL_YoloDemo() ) {
				Com_Error( "Use yolodemo to force load it" );
			}
		}
		else {
			Com_GGError( "Server returned version {}, not {}", i, APP_PROTOCOL_VERSION );
		}
	}

	cl.servercount = MSG_ReadInt32( msg );
	cl.snapFrameTime = (unsigned int)MSG_ReadInt16( msg );

	TempAllocator temp = cls.frame_arena.temp();

	// set extrapolation time to half snapshot time
	Cvar_ForceSet( "cl_extrapolationTime", temp.sv( "{}", cl.snapFrameTime / 2 ) );
	cl_extrapolationTime->modified = false;

	cl.max_clients = MSG_ReadUint8( msg );
	cl.playernum = MSG_ReadInt16( msg );
	cls.server_name = CopyString( sys_allocator, MSG_ReadString( msg ) );

	const char * download_url = MSG_ReadString( msg );
	if( !StrEqual( download_url, "" ) ) {
		cls.download_url = CopyString( sys_allocator, download_url );
	}
	else {
		cls.download_url = CopyString( sys_allocator, temp( "http://{}", cls.serveraddress ) );
	}

	msg_t * args = CL_AddReliableCommand( ClientCommand_Baselines );
	MSG_WriteInt32( args, cl.servercount );
	MSG_WriteUint32( args, 0 );
}

static void CL_ParseBaseline( msg_t *msg ) {
	SNAP_ParseBaseline( msg, cl_baselines );
}

static void CL_ParseFrame( msg_t *msg ) {
	const snapshot_t * oldSnap = cl.receivedSnapNum > 0 ? &cl.snapShots[ cl.receivedSnapNum % ARRAY_COUNT( cl.snapShots ) ] : NULL;
	const snapshot_t * snap = SNAP_ParseFrame( msg, oldSnap, cl.snapShots, cl_baselines, cl_shownet->integer );
	if( !snap->valid )
		return;

	cl.receivedSnapNum = snap->serverFrame;

	CL_DemoBaseline( snap );

	if( cl_debug_timeDelta->integer ) {
		if( oldSnap != NULL && ( oldSnap->serverFrame + 1 != snap->serverFrame ) ) {
			Com_Printf( S_COLOR_RED "***** SnapShot lost\n" );
		}
	}

	// the first snap, fill all the timeDeltas with the same value
	// don't let delta add big jumps to the smoothing ( a stable connection produces jumps inside +-3 range)
	int delta = ( snap->serverTime - cl.snapFrameTime ) - cls.gametime;
	if( cl.currentSnapNum <= 0 || delta < cl.newServerTimeDelta - 175 || delta > cl.newServerTimeDelta + 175 ) {
		CL_RestartTimeDeltas( delta );
	} else {
		if( cl_debug_timeDelta->integer ) {
			if( delta < cl.newServerTimeDelta - (int)cl.snapFrameTime ) {
				Com_Printf( S_COLOR_CYAN "***** timeDelta low clamp\n" );
			} else if( delta > cl.newServerTimeDelta + (int)cl.snapFrameTime ) {
				Com_Printf( S_COLOR_CYAN "***** timeDelta high clamp\n" );
			}
		}

		delta = Clamp( cl.newServerTimeDelta - (int)cl.snapFrameTime, delta, cl.newServerTimeDelta + (int)cl.snapFrameTime );

		cl.serverTimeDeltas[ cl.receivedSnapNum % ARRAY_COUNT( cl.serverTimeDeltas ) ] = delta;
	}
}

static void CL_RequestMoreBaselines( const Tokenized & args ) {
	if( CL_DemoPlaying() ) {
		return;
	}

	if( cls.state != CA_CONNECTED && cls.state != CA_ACTIVE ) {
		Com_Printf( "Can't rqeuest more baselines, not connected\n" );
		return;
	}

	msg_t * msg = CL_AddReliableCommand( ClientCommand_Baselines );
	MSG_WriteInt32( msg, SpanToInt( args.tokens[ 1 ], 0 ) );
	MSG_WriteUint32( msg, SpanToInt( args.tokens[ 2 ], 0 ) );
}

static const struct {
	const char * name;
	void ( *func )( const Tokenized & args );
	size_t num_args;
} svcmds[] = {
	{ "forcereconnect", []( const Tokenized & args ) { CL_Reconnect_f(); } },
	{ "reconnect", []( const Tokenized & args ) { CL_ServerReconnect_f(); }, 0 },
	{ "changing", []( const Tokenized & args ) { CL_Changing_f(); }, 0 },
	{ "precache", CL_Precache_f, 2 },
	{ "baselines", CL_RequestMoreBaselines, 2 },
	{ "disconnect", CL_ServerDisconnect_f, 1 },
};

static void CL_ParseServerCommand( msg_t * msg ) {
	TempAllocator temp = cls.frame_arena.temp();

	const char * text = MSG_ReadString( msg );
	Tokenized args = Tokenize( &temp, MakeSpan( text ) );

	if( cl_debug_serverCmd->integer && ( cls.state < CA_ACTIVE || CL_DemoPlaying() ) ) {
		Com_Printf( "CL_ParseServerCommand: \"%s\"\n", text );
	}

	for( auto cmd : svcmds ) {
		if( StrEqual( cmd.name, args.tokens[ 0 ] ) && args.tokens.n == cmd.num_args + 1 ) {
			cmd.func( args );
			return;
		}
	}

	Assert( is_public_build );
	Com_GGPrint( "Unknown server command: {}", args.tokens[ 0 ] );
}

/*
=====================================================================

ACTION MESSAGES

=====================================================================
*/

void CL_ParseServerMessage( msg_t *msg ) {
	if( cl_shownet->integer == 1 ) {
		Com_GGPrint( "{} ", msg->cursize );
	} else if( cl_shownet->integer >= 2 ) {
		Com_Printf( "------------------\n" );
	}

	size_t msg_header_offset = msg->readcount;

	// parse the message
	while( msg->readcount < msg->cursize ) {
		int cmd = MSG_ReadUint8( msg );
		if( cl_debug_serverCmd->integer & 4 ) {
			Com_GGPrint( "{}:CMD {} {}", msg->readcount - 1, cmd, !svc_strings[cmd] ? "bad" : svc_strings[cmd] );
		}

		if( cl_shownet->integer >= 2 ) {
			if( !svc_strings[cmd] ) {
				Com_GGPrint( "{}:BAD CMD {}", msg->readcount - 1, cmd );
			} else {
				SHOWNET( msg, svc_strings[cmd] );
			}
		}

		// other commands
		switch( cmd ) {
			default:
				Com_Error( "CL_ParseServerMessage: Illegible server message" );
				break;

			case svc_servercmd: {
				int cmdNum = MSG_ReadInt32( msg );
				if( cmdNum < 0 ) {
					Com_Error( "CL_ParseServerMessage: Invalid cmdNum value received: %i\n", cmdNum );
					return;
				}
				if( cmdNum <= cls.lastExecutedServerCommand ) {
					MSG_ReadString( msg ); // read but ignore
					break;
				}
				cls.lastExecutedServerCommand = cmdNum;
				CL_ParseServerCommand( msg );
			} break;

			case svc_unreliable:
				CL_ParseServerCommand( msg );
				break;

			case svc_serverdata:
				if( cls.state == CA_HANDSHAKE ) {
					CL_ParseServerData( msg );
				} else {
					return; // ignore rest of the packet (serverdata is always sent alone)
				}
				break;

			case svc_spawnbaseline:
				CL_ParseBaseline( msg );
				break;

			case svc_clcack:
				cls.reliableAcknowledge = MSG_ReadUintBase128( msg );
				cls.ucmdAcknowledged = MSG_ReadUintBase128( msg );
				if( cl_debug_serverCmd->integer & 4 ) {
					Com_GGPrint( "svc_clcack:reliable cmd ack:{} ucmdack:{}", cls.reliableAcknowledge, cls.ucmdAcknowledged );
				}
				break;

			case svc_frame:
				CL_ParseFrame( msg );
				break;

			case svc_playerinfo:
			case svc_packetentities:
			case svc_match:
				Com_Error( "Out of place frame data" );
				break;
		}
	}

	if( msg->readcount > msg->cursize ) {
		Assert( is_public_build );
		Com_Error( "CL_ParseServerMessage: Bad server message" );
		return;
	}

	if( cl_debug_serverCmd->integer & 4 ) {
		Com_Printf( "%3li:CMD %i %s\n", msg->readcount, -1, "EOF" );
	}
	SHOWNET( msg, "END OF MESSAGE" );

	CL_AddNetgraph();

	CL_WriteDemoMessage( *msg, msg_header_offset );
}
