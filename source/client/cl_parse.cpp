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
#include "qcommon/fs.h"
#include "qcommon/string.h"
#include "qcommon/version.h"

struct DownloadInProgress {
	char * path;
	DownloadCompleteCallback callback;
};

static DownloadInProgress download;

static void OnDownloadDone( int http_status, Span< const u8 > data ) {
	Com_Printf( "Download %s: %s (%i)\n", data.ptr != NULL ? "successful" : "failed", download.path, http_status );

	download.callback( download.path, data );

	FREE( sys_allocator, download.path );
	download.path = NULL;
}

bool CL_DownloadFile( const char * filename, DownloadCompleteCallback callback ) {
	if( download.path ) {
		Com_Printf( "Already downloading something.\n" );
		return false;
	}

	if( !COM_ValidateRelativeFilename( filename ) ) {
		Com_Printf( "Can't download: %s. Invalid filename.\n", filename );
		return false;
	}

	if( cls.socket->type == SOCKET_LOOPBACK ) {
		Com_Printf( "Can't download from a local server.\n" );
		return false;
	}

	Com_Printf( "Asking to download: %s\n", filename );

	download = { };
	download.path = CopyString( sys_allocator, filename );
	download.callback = callback;

	TempAllocator temp = cls.frame_arena.temp();

	const char * url = temp( "{}/{}", cls.download_url, filename );
	if( cls.download_url_is_game_server ) {
		const char * headers[] = {
			temp( "X-Client: {}", cl.playernum ),
			temp( "X-Session: {}", cls.session ),
		};
		StartDownload( url, OnDownloadDone, headers, ARRAY_COUNT( headers ) );
	}
	else {
		StartDownload( url, OnDownloadDone, NULL, 0 );
	}

	Com_Printf( "Downloading %s\n", url );

	return true;
}

/*
=====================================================================

SERVER CONNECTING MESSAGES

=====================================================================
*/

static void CL_ParseServerData( msg_t *msg ) {
	Com_DPrintf( "Serverdata packet received.\n" );

	// wipe the client_state_t struct

	CL_ClearState();
	CL_SetClientState( CA_CONNECTED );

	// parse protocol version number
	int i = MSG_ReadInt32( msg );

	if( i != APP_PROTOCOL_VERSION ) {
		if( cls.demo.playing ) {
			Com_Printf( S_COLOR_YELLOW "This demo was recorded with an old version of the game and may be broken!\n" );
			if( !cls.demo.yolo ) {
				Com_Error( "Run yolodemo %s to force load it", cls.demo.name );
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
	Cvar_ForceSet( "cl_extrapolationTime", temp( "{}", cl.snapFrameTime / 2 ) );
	cl_extrapolationTime->modified = false;

	// parse player entity number
	cl.playernum = MSG_ReadInt16( msg );

	const char * download_url = MSG_ReadString( msg );
	if( strlen( download_url ) > 0 ) {
		cls.download_url = CopyString( sys_allocator, download_url );
		cls.download_url_is_game_server = false;
	}
	else if( cls.serveraddress.type != NA_LOOPBACK ) {
		cls.download_url = CopyString( sys_allocator, temp( "http://{}", NET_AddressToString( &cls.serveraddress ) ) );
		cls.download_url_is_game_server = true;
	}

	// get the configstrings request
	CL_AddReliableCommand( temp( "configstrings {} 0", cl.servercount ) );
}

static void CL_ParseBaseline( msg_t *msg ) {
	SNAP_ParseBaseline( msg, cl_baselines );
}

static void CL_ParseFrame( msg_t *msg ) {
	snapshot_t *snap, *oldSnap;
	int delta;

	oldSnap = ( cl.receivedSnapNum > 0 ) ? &cl.snapShots[cl.receivedSnapNum & UPDATE_MASK] : NULL;

	snap = SNAP_ParseFrame( msg, oldSnap, cl.snapShots, cl_baselines, cl_shownet->integer );
	if( snap->valid ) {
		cl.receivedSnapNum = snap->serverFrame;

		if( cls.demo.recording ) {
			if( cls.demo.waiting && !snap->delta ) {
				cls.demo.waiting = false; // we can start recording now
				cls.demo.basetime = snap->serverTime;
				cls.demo.localtime = time( NULL );

				memset( cls.demo.meta_data, 0, sizeof( cls.demo.meta_data ) );
				cls.demo.meta_data_realsize = 0;

				// write out messages to hold the startup information
				TempAllocator temp = cls.frame_arena.temp();
				SNAP_BeginDemoRecording( &temp, cls.demo.file, 0x10000 + cl.servercount, cl.snapFrameTime,
										 cl.configstrings[0], cl_baselines );

				// the rest of the demo file will be individual frames
			}

			if( !cls.demo.waiting ) {
				cls.demo.duration = snap->serverTime - cls.demo.basetime;
			}
			cls.demo.time = cls.demo.duration;
		}

		if( cl_debug_timeDelta->integer ) {
			if( oldSnap != NULL && ( oldSnap->serverFrame + 1 != snap->serverFrame ) ) {
				Com_Printf( S_COLOR_RED "***** SnapShot lost\n" );
			}
		}

		// the first snap, fill all the timeDeltas with the same value
		// don't let delta add big jumps to the smoothing ( a stable connection produces jumps inside +-3 range)
		delta = ( snap->serverTime - cl.snapFrameTime ) - cls.gametime;
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

			cl.serverTimeDeltas[cl.receivedSnapNum & MASK_TIMEDELTAS_BACKUP] = delta;
		}
	}
}

static void CL_UpdateConfigString( int idx, const char *s ) {
	if( cl_debug_serverCmd->integer && ( cls.state >= CA_ACTIVE || cls.demo.playing ) ) {
		Com_Printf( "CL_ParseConfigstringCommand(%i): \"%s\"\n", idx, s );
	}

	if( idx < 0 || idx >= MAX_CONFIGSTRINGS ) {
		Com_Error( "configstring > MAX_CONFIGSTRINGS" );
	}

	// wsw : jal : warn if configstring overflow
	if( strlen( s ) >= MAX_CONFIGSTRING_CHARS ) {
		Com_Printf( "%sWARNING:%s Configstring %i overflowed\n", S_COLOR_YELLOW, S_COLOR_WHITE, idx );
		Com_Printf( "%s%s\n", S_COLOR_WHITE, s );
	}

	Q_strncpyz( cl.configstrings[idx], s, sizeof( cl.configstrings[idx] ) );

	// allow cgame to update it too
	CL_GameModule_ConfigString( idx );
}

static void CL_ForwardToServer_f() {
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

static void CL_ParseConfigstringCommand() {
	int i, argc, idx;
	const char *s;

	if( Cmd_Argc() < 3 ) {
		return;
	}

	// ch : configstrings may come batched now, so lets loop through them
	argc = Cmd_Argc();
	for( i = 1; i < argc - 1; i += 2 ) {
		idx = atoi( Cmd_Argv( i ) );
		s = Cmd_Argv( i + 1 );

		CL_UpdateConfigString( idx, s );
	}
}

typedef struct {
	const char *name;
	void ( *func )();
} svcmd_t;

static svcmd_t svcmds[] = {
	{ "forcereconnect", CL_Reconnect_f },
	{ "reconnect", CL_ServerReconnect_f },
	{ "changing", CL_Changing_f },
	{ "precache", CL_Precache_f },
	{ "cmd", CL_ForwardToServer_f },
	{ "cs", CL_ParseConfigstringCommand },
	{ "disconnect", CL_ServerDisconnect_f },

	{ NULL, NULL }
};

static void CL_ParseServerCommand( msg_t *msg ) {
	const char *s;
	char *text;
	svcmd_t *cmd;

	text = MSG_ReadString( msg );

	Cmd_TokenizeString( text );
	s = Cmd_Argv( 0 );

	if( cl_debug_serverCmd->integer && ( cls.state < CA_ACTIVE || cls.demo.playing ) ) {
		Com_Printf( "CL_ParseServerCommand: \"%s\"\n", text );
	}

	// filter out these server commands to be called from the client
	for( cmd = svcmds; cmd->name; cmd++ ) {
		if( StrEqual( s, cmd->name ) ) {
			cmd->func();
			return;
		}
	}

	Com_Printf( "Unknown server command: %s\n", s );
}

/*
=====================================================================

ACTION MESSAGES

=====================================================================
*/

void CL_ParseServerMessage( msg_t *msg ) {
	if( cl_shownet->integer == 1 ) {
		Com_Printf( "%" PRIuPTR " ", (uintptr_t)msg->cursize );
	} else if( cl_shownet->integer >= 2 ) {
		Com_Printf( "------------------\n" );
	}

	// parse the message
	while( msg->readcount < msg->cursize ) {
		int cmd;
		size_t meta_data_maxsize;

		cmd = MSG_ReadUint8( msg );
		if( cl_debug_serverCmd->integer & 4 ) {
			Com_Printf( "%3" PRIi64 ":CMD %i %s\n", (int64_t)(msg->readcount - 1), cmd, !svc_strings[cmd] ? "bad" : svc_strings[cmd] );
		}

		if( cl_shownet->integer >= 2 ) {
			if( !svc_strings[cmd] ) {
				Com_Printf( "%3" PRIi64 ":BAD CMD %i\n", (int64_t)(msg->readcount - 1), cmd );
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
			}

			// fall through
			case svc_servercs: // configstrings from demo files. they don't have acknowledge
				CL_ParseServerCommand( msg );
				break;

			case svc_serverdata:
				if( cls.state == CA_HANDSHAKE ) {
					Cbuf_Execute(); // make sure any stuffed commands are done
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
					Com_Printf( "svc_clcack:reliable cmd ack:%" PRIi64 " ucmdack:%" PRIi64 "\n", cls.reliableAcknowledge, cls.ucmdAcknowledged );
				}
				break;

			case svc_frame:
				CL_ParseFrame( msg );
				break;

			case svc_demoinfo:
				assert( cls.demo.playing );

				MSG_ReadInt32( msg );
				MSG_ReadInt32( msg );
				cls.demo.meta_data_realsize = (size_t)MSG_ReadInt32( msg );
				meta_data_maxsize = (size_t)MSG_ReadInt32( msg );

				// sanity check
				if( cls.demo.meta_data_realsize > meta_data_maxsize ) {
					cls.demo.meta_data_realsize = meta_data_maxsize;
				}
				if( cls.demo.meta_data_realsize > sizeof( cls.demo.meta_data ) ) {
					cls.demo.meta_data_realsize = sizeof( cls.demo.meta_data );
				}

				MSG_ReadData( msg, cls.demo.meta_data, cls.demo.meta_data_realsize );
				MSG_SkipData( msg, meta_data_maxsize - cls.demo.meta_data_realsize );
				break;

			case svc_playerinfo:
			case svc_packetentities:
			case svc_match:
				Com_Error( "Out of place frame data" );
				break;
		}
	}

	if( msg->readcount > msg->cursize ) {
		Com_Error( "CL_ParseServerMessage: Bad server message" );
		return;
	}

	if( cl_debug_serverCmd->integer & 4 ) {
		Com_Printf( "%3li:CMD %i %s\n", msg->readcount, -1, "EOF" );
	}
	SHOWNET( msg, "END OF MESSAGE" );

	CL_AddNetgraph();

	//
	// if recording demos, copy the message out
	//
	//
	// we don't know if it is ok to save a demo message until
	// after we have parsed the frame
	//
	if( cls.demo.recording && !cls.demo.waiting ) {
		CL_WriteDemoMessage( msg );
	}
}
