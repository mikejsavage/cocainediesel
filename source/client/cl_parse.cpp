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
#include "qcommon/array.h"
#include "qcommon/version.h"

struct DownloadInProgress {
	char * requestname;
	int64_t timeout;

	char * name; // name of the file in download, relative to base path
	char * tempname; // temporary location, relative to base path
	size_t expected_size;
	u32 expected_checksum;

	size_t bytes_downloaded;
	u32 checksum;

	bool cancelled; // player pressed escape, disconnect after
	bool reconnect; // server has changed maps while downloading

	bool save_to_disk;
	int filenum;
};

static DownloadInProgress download;
static NonRAIIDynamicArray< u8 > download_data;

static void DownloadRejected() {
	FREE( sys_allocator, download.requestname );
	download.requestname = NULL;
	download.timeout = 0;
}

bool CL_DownloadFile( const char * filename, bool save_to_disk ) {
	if( download.requestname ) {
		Com_Printf( "Can't download: %s. Download already in progress.\n", filename );
		return false;
	}

	if( !COM_ValidateRelativeFilename( filename ) ) {
		Com_Printf( "Can't download: %s. Invalid filename.\n", filename );
		return false;
	}

	if( save_to_disk && FS_FOpenFile( filename, NULL, FS_READ ) != -1 ) {
		Com_Printf( "Can't download: %s. File already exists.\n", filename );
		return false;
	}

	if( cls.socket->type == SOCKET_LOOPBACK ) {
		Com_DPrintf( "Can't download: %s. Loopback server.\n", filename );
		return false;
	}

	Com_Printf( "Asking to download: %s\n", filename );

	download = { };
	download.requestname = CopyString( sys_allocator, filename );
	download.timeout = Sys_Milliseconds() + 5000;
	download.save_to_disk = save_to_disk;

	CL_AddReliableCommand( va( "download \"%s\"", filename ) );

	return true;
}

bool CL_IsDownloading() {
	return download.requestname != NULL;
}

void CL_CancelDownload() {
	download.cancelled = true;
}

void CL_ReconnectAfterDownload() {
	download.reconnect = true;
}

static void FinishDownload() {
	if( download.bytes_downloaded != download.expected_size || download.checksum != download.expected_checksum ) {
		Com_Printf( "Downloaded file doesn't match what we expected.\n" );
		if( download.save_to_disk ) {
			FS_RemoveBaseFile( download.tempname );
		}
		return;
	}

	if( !download.save_to_disk ) {
		if( !AddMap( download_data.span(), download.requestname ) ) {
			Com_Printf( "Downloaded map is corrupt.\n" );
		}

		return;
	}

	FS_FCloseFile( download.filenum );

	if( !FS_MoveBaseFile( download.tempname, download.name ) ) {
		Com_Printf( "Failed to rename the downloaded file\n" );
	}
}

static void OnDownloadDone( bool ok, int http_status ) {
	Com_Printf( "Download %s: %s (%i)\n", ok ? "successful" : "failed", download.name, http_status );

	if( ok ) {
		FinishDownload();
	}

	FREE( sys_allocator, download.requestname );
	FREE( sys_allocator, download.name );
	FREE( sys_allocator, download.tempname );
	download.requestname = NULL;

	if( !download.save_to_disk ) {
		download_data.shutdown();
	}

	if( download.cancelled ) {
		CL_Disconnect( NULL );
	}
	else if( download.reconnect ) {
		CL_ServerReconnect_f();
	}
	else if( !download.save_to_disk ) {
		// map download
		if( ok ) {
			CL_FinishConnect();
		}
		else {
			CL_Disconnect( NULL );
		}
	}

	Cvar_ForceSet( "cl_download_name", "" );
	Cvar_ForceSet( "cl_download_percent", "0" );
}

static bool OnDownloadData( const void * data, size_t n ) {
	if( download.cancelled ) {
		return false;
	}

	if( download.save_to_disk ) {
		FS_Write( data, n, download.filenum );
	}
	else {
		size_t old_size = download_data.extend( n );
		memcpy( download_data.ptr() + old_size, data, n );
	}

	download.checksum = Hash32( data, n, download.checksum );
	download.bytes_downloaded += n;

	float progress = double( download.bytes_downloaded ) / double( download.expected_size );
	Cvar_ForceSet( "cl_download_percent", va( "%.1f", progress ) );

	download.timeout = 0;

	return true;
}

static void CL_InitDownload_f( void ) {
	// ignore download commands coming from demo files
	if( cls.demo.playing ) {
		return;
	}

	// read the data
	const char * filename = Cmd_Argv( 1 );
	int size = atoi( Cmd_Argv( 2 ) );
	u32 checksum = strtoul( Cmd_Argv( 3 ), NULL, 10 );
	bool not_external_server = atoi( Cmd_Argv( 4 ) ) != 0 && cls.httpbaseurl != NULL;
	const char * url = Cmd_Argv( 5 );

	if( download.requestname == NULL ) {
		Com_Printf( "Got init download message without request\n" );
		return;
	}

	if( download.name != NULL ) {
		Com_Printf( "Got init download message while already downloading\n" );
		return;
	}

	if( size == -1 ) {
		// means that download was refused
		Com_Printf( "Server refused download request: %s\n", url ); // if it's refused, url field holds the reason
		DownloadRejected();
		return;
	}

	if( size <= 0 ) {
		Com_Printf( "Server gave invalid size, not downloading\n" );
		DownloadRejected();
		return;
	}

	if( checksum == 0 ) {
		Com_Printf( "Server didn't provide checksum, not downloading\n" );
		DownloadRejected();
		return;
	}

	if( !COM_ValidateRelativeFilename( filename ) ) {
		Com_Printf( "Not downloading, invalid filename: %s\n", filename );
		DownloadRejected();
		return;
	}

	if( strchr( filename, '/' ) == NULL || strcmp( download.requestname, strchr( filename, '/' ) + 1 ) ) {
		Com_Printf( "Can't download, got different file than requested: %s\n", filename );
		DownloadRejected();
		return;
	}

	TempAllocator temp = cls.frame_arena.temp();
	char * tempname = temp( "{}.tmp", filename );

	if( download.save_to_disk ) {
		FS_FOpenBaseFile( tempname, &download.filenum, FS_WRITE );

		if( !download.filenum ) {
			Com_Printf( "Can't download, couldn't open %s for writing\n", tempname );
			DownloadRejected();
			return;
		}

		download.tempname = CopyString( sys_allocator, tempname );
	}
	else {
		download_data.init( sys_allocator );
	}

	download.timeout = 0;
	download.name = CopyString( sys_allocator, filename );
	download.expected_size = size;
	download.expected_checksum = checksum;
	download.checksum = Hash32( "" );

	Cvar_ForceSet( "cl_download_name", COM_FileBase( filename ) );
	Cvar_ForceSet( "cl_download_percent", "0" );

	const char * fullurl;
	if( not_external_server ) {
		fullurl = temp( "{}/{}", cls.httpbaseurl, url );
		const char * headers[] = {
			temp( "X-Client: {}", cl.playernum ),
			temp( "X-Session: {}", cls.session ),
		};

		StartDownload( fullurl, OnDownloadData, OnDownloadDone, headers, ARRAY_COUNT( headers ) );
	}
	else {
		fullurl = temp( "{}/{}", url, filename );
		StartDownload( fullurl, OnDownloadData, OnDownloadDone, NULL, 0 );
	}

	Com_Printf( "Downloading %s\n", fullurl );
}

void CL_CheckDownloadTimeout( void ) {
	if( download.timeout == 0 || download.timeout > Sys_Milliseconds() ) {
		return;
	}

	Com_Printf( "Download request timed out.\n" );
	DownloadRejected();
}

/*
=====================================================================

SERVER CONNECTING MESSAGES

=====================================================================
*/

static void CL_ParseServerData( msg_t *msg ) {
	const char *str;
	int i, sv_bitflags;
	int http_portnum;

	Com_DPrintf( "Serverdata packet received.\n" );

	// wipe the client_state_t struct

	CL_ClearState();
	CL_SetClientState( CA_CONNECTED );

	// parse protocol version number
	i = MSG_ReadInt32( msg );

	if( i != APP_PROTOCOL_VERSION ) {
		Com_GGError( ERR_DROP, "Server returned version {}, not {}", i, APP_PROTOCOL_VERSION );
	}

	cl.servercount = MSG_ReadInt32( msg );
	cl.snapFrameTime = (unsigned int)MSG_ReadInt16( msg );

	// set extrapolation time to half snapshot time
	Cvar_ForceSet( "cl_extrapolationTime", va( "%i", (unsigned int)( cl.snapFrameTime * 0.5 ) ) );
	cl_extrapolationTime->modified = false;

	// base game directory
	str = MSG_ReadString( msg );
	if( !str || !str[0] ) {
		Com_Error( ERR_DROP, "Server sent an empty base game directory" );
	}
	if( !COM_ValidateRelativeFilename( str ) || strchr( str, '/' ) ) {
		Com_Error( ERR_DROP, "Server sent an invalid base game directory: %s", str );
	}
	if( strcmp( FS_BaseGameDirectory(), str ) ) {
		Com_Error( ERR_DROP, "Server has different base game directory (%s) than the client (%s)", str,
				   FS_BaseGameDirectory() );
	}

	// parse player entity number
	cl.playernum = MSG_ReadInt16( msg );

	sv_bitflags = MSG_ReadUint8( msg );

	if( cls.demo.playing ) {
		cls.reliable = ( sv_bitflags & SV_BITFLAGS_RELIABLE );
	} else {
		if( cls.reliable != ( ( sv_bitflags & SV_BITFLAGS_RELIABLE ) != 0 ) ) {
			Com_Error( ERR_DROP, "Server and client disagree about connection reliability" );
		}
	}

	// builting HTTP server port
	if( cls.httpbaseurl ) {
		Mem_Free( cls.httpbaseurl );
		cls.httpbaseurl = NULL;
	}

	if( ( sv_bitflags & SV_BITFLAGS_HTTP ) != 0 ) {
		if( ( sv_bitflags & SV_BITFLAGS_HTTP_BASEURL ) != 0 ) {
			// read base upstream url
			cls.httpbaseurl = ZoneCopyString( MSG_ReadString( msg ) );
		} else {
			http_portnum = MSG_ReadInt16( msg ) & 0xffff;
			cls.httpaddress = cls.serveraddress;
			if( cls.httpaddress.type == NA_IP6 ) {
				cls.httpaddress.address.ipv6.port = BigShort( http_portnum );
			} else {
				cls.httpaddress.address.ipv4.port = BigShort( http_portnum );
			}
			if( http_portnum ) {
				if( cls.httpaddress.type == NA_LOOPBACK ) {
					cls.httpbaseurl = ZoneCopyString( va( "http://localhost:%hu/", http_portnum ) );
				} else {
					cls.httpbaseurl = ZoneCopyString( va( "http://%s/", NET_AddressToString( &cls.httpaddress ) ) );
				}
			}
		}
	}

	// get the configstrings request
	CL_AddReliableCommand( va( "configstrings %i 0", cl.servercount ) );
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

				// clear demo meta data, we'll write some keys later
				cls.demo.meta_data_realsize = SNAP_ClearDemoMeta( cls.demo.meta_data, sizeof( cls.demo.meta_data ) );

				// write out messages to hold the startup information
				SNAP_BeginDemoRecording( cls.demo.file, 0x10000 + cl.servercount, cl.snapFrameTime,
										 cls.reliable ? SV_BITFLAGS_RELIABLE : 0,
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
	if( !s ) {
		return;
	}

	if( cl_debug_serverCmd->integer && ( cls.state >= CA_ACTIVE || cls.demo.playing ) ) {
		Com_Printf( "CL_ParseConfigstringCommand(%i): \"%s\"\n", idx, s );
	}

	if( idx < 0 || idx >= MAX_CONFIGSTRINGS ) {
		Com_Error( ERR_DROP, "configstring > MAX_CONFIGSTRINGS" );
	}

	// wsw : jal : warn if configstring overflow
	if( strlen( s ) >= MAX_CONFIGSTRING_CHARS ) {
		Com_Printf( "%sWARNING:%s Configstring %i overflowed\n", S_COLOR_YELLOW, S_COLOR_WHITE, idx );
		Com_Printf( "%s%s\n", S_COLOR_WHITE, s );
	}

	if( !COM_ValidateConfigstring( s ) ) {
		Com_Printf( "%sWARNING:%s Invalid Configstring (%i): %s\n", S_COLOR_YELLOW, S_COLOR_WHITE, idx, s );
		return;
	}

	Q_strncpyz( cl.configstrings[idx], s, sizeof( cl.configstrings[idx] ) );

	// allow cgame to update it too
	CL_GameModule_ConfigString( idx, s );
}

static void CL_ParseConfigstringCommand( void ) {
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
	void ( *func )( void );
} svcmd_t;

static svcmd_t svcmds[] = {
	{ "forcereconnect", CL_Reconnect_f },
	{ "reconnect", CL_ServerReconnect_f },
	{ "changing", CL_Changing_f },
	{ "precache", CL_Precache_f },
	{ "cmd", CL_ForwardToServer_f },
	{ "cs", CL_ParseConfigstringCommand },
	{ "disconnect", CL_ServerDisconnect_f },
	{ "initdownload", CL_InitDownload_f },

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
		if( !strcmp( s, cmd->name ) ) {
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
				Com_Error( ERR_DROP, "CL_ParseServerMessage: Illegible server message" );
				break;

			case svc_servercmd:
				if( !cls.reliable ) {
					int cmdNum = MSG_ReadInt32( msg );
					if( cmdNum < 0 ) {
						Com_Error( ERR_DROP, "CL_ParseServerMessage: Invalid cmdNum value received: %i\n",
								   cmdNum );
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
				if( cls.reliable ) {
					Com_Error( ERR_DROP, "CL_ParseServerMessage: clack message for reliable client\n" );
					return;
				}
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
				Com_Error( ERR_DROP, "Out of place frame data" );
				break;
		}
	}

	if( msg->readcount > msg->cursize ) {
		Com_Error( ERR_DROP, "CL_ParseServerMessage: Bad server message" );
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
