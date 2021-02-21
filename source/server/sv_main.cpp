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

#include "server/server.h"
#include "qcommon/version.h"
#include "qcommon/csprng.h"

static bool sv_initialized = false;

mempool_t *sv_mempool;

// IPv4
cvar_t *sv_ip;
cvar_t *sv_port;

// IPv6
cvar_t *sv_ip6;
cvar_t *sv_port6;

cvar_t *sv_timeout;            // seconds without any message
cvar_t *sv_zombietime;         // seconds to sink messages after disconnect

cvar_t *rcon_password;         // password for remote server commands

cvar_t *sv_uploads_http;
cvar_t *sv_uploads_baseurl;
cvar_t *sv_uploads_demos;
cvar_t *sv_uploads_demos_baseurl;

cvar_t *sv_maxclients;

cvar_t *sv_http;
cvar_t *sv_http_ip;
cvar_t *sv_http_ipv6;
cvar_t *sv_http_port;
cvar_t *sv_http_upstream_baseurl;
cvar_t *sv_http_upstream_ip;
cvar_t *sv_http_upstream_realip_header;

cvar_t *sv_showRcon;
cvar_t *sv_showChallenge;
cvar_t *sv_showInfoQueries;

cvar_t *sv_hostname;
cvar_t *sv_public;         // should heartbeats be sent
cvar_t *sv_defaultmap;

cvar_t *sv_iplimit;

cvar_t *sv_reconnectlimit; // minimum seconds between connect messages

// wsw : debug netcode
cvar_t *sv_debug_serverCmd;

cvar_t *sv_demodir;

//============================================================================

/*
* SV_CalcPings
*
* Updates the cl->ping variables
*/
static void SV_CalcPings() {
	unsigned int i, j;
	client_t *cl;
	unsigned int total, count, lat, best;

	for( i = 0; i < (unsigned int)sv_maxclients->integer; i++ ) {
		cl = &svs.clients[i];
		if( cl->state != CS_SPAWNED ) {
			continue;
		}
		if( cl->edict && ( cl->edict->r.svflags & SVF_FAKECLIENT ) ) {
			continue;
		}

		total = 0;
		count = 0;
		best = 9999;
		for( j = 0; j < LATENCY_COUNTS; j++ ) {
			if( cl->frame_latency[j] > 0 ) {
				lat = (unsigned)cl->frame_latency[j];
				if( lat < best ) {
					best = lat;
				}

				total += lat;
				count++;
			}
		}

		if( !count ) {
			cl->ping = 0;
		}
		else {
			cl->ping = ( best + ( total / count ) ) * 0.5f;
		}
		// let the game dll know about the ping
		cl->edict->r.client->r.ping = cl->ping;
	}
}

/*
* SV_ProcessPacket
*/
static bool SV_ProcessPacket( netchan_t *netchan, msg_t *msg ) {
	int zerror;

	if( !Netchan_Process( netchan, msg ) ) {
		return false; // wasn't accepted for some reason

	}
	// now if compressed, expand it
	MSG_BeginReading( msg );
	MSG_ReadInt32( msg ); // sequence
	MSG_ReadInt32( msg ); // sequence_ack
	MSG_ReadInt16( msg ); // game_port
	if( msg->compressed ) {
		zerror = Netchan_DecompressMessage( msg );
		if( zerror < 0 ) {
			// compression error. Drop the packet
			Com_DPrintf( "SV_ProcessPacket: Compression error %i. Dropping packet\n", zerror );
			return false;
		}
	}

	return true;
}

/*
* SV_ReadPackets
*/
static void SV_ReadPackets() {
	ZoneScoped;

	int i, ret;
	client_t *cl;
	int game_port;
	socket_t *socket;
	netadr_t address;

	static msg_t msg;
	static uint8_t msgData[MAX_MSGLEN];

	socket_t* sockets [] =
	{
		&svs.socket_loopback,
		&svs.socket_udp,
		&svs.socket_udp6,
	};

	MSG_Init( &msg, msgData, sizeof( msgData ) );

	for( size_t socketind = 0; socketind < ARRAY_COUNT( sockets ); socketind++ ) {
		socket = sockets[socketind];

		if( !socket->open ) {
			continue;
		}

		while( ( ret = NET_GetPacket( socket, &address, &msg ) ) != 0 ) {
			if( ret == -1 ) {
				Com_Printf( "NET_GetPacket: Error: %s\n", NET_ErrorString() );
				continue;
			}

			// check for connectionless packet (0xffffffff) first
			if( *(int *)msg.data == -1 ) {
				SV_ConnectionlessPacket( socket, &address, &msg );
				continue;
			}

			// read the game port out of the message so we can fix up
			// stupid address translating routers
			MSG_BeginReading( &msg );
			MSG_ReadInt32( &msg ); // sequence number
			MSG_ReadInt32( &msg ); // sequence number
			game_port = MSG_ReadInt16( &msg ) & 0xffff;
			// data follows

			// check for packets from connected clients
			for( i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++ ) {
				unsigned short addr_port;

				if( cl->state == CS_FREE || cl->state == CS_ZOMBIE ) {
					continue;
				}
				if( cl->edict && ( cl->edict->r.svflags & SVF_FAKECLIENT ) ) {
					continue;
				}
				// originally compare base address, todo conduct testing to verify that this doesn't break everything
				// if it does, then match msg -> client a smarter way!
				if( !NET_CompareAddress( &address, &cl->netchan.remoteAddress ) ) {
					continue;
				}
				if( cl->netchan.game_port != game_port ) {
					continue;
				}

				// NOTE(scoot) see above
				// addr_port = NET_GetAddressPort( &address );
				// if( NET_GetAddressPort( &cl->netchan.remoteAddress ) != addr_port ) {
				// 	Com_Printf( "SV_ReadPackets: fixing up a translated port\n" );
				// 	NET_SetAddressPort( &cl->netchan.remoteAddress, addr_port );
				// }

				if( SV_ProcessPacket( &cl->netchan, &msg ) ) { // this is a valid, sequenced packet, so process it
					cl->lastPacketReceivedTime = svs.realtime;
					SV_ParseClientMessage( cl, &msg );
				}
				break;
			}
		}
	}

	// handle clients with individual sockets
	for( i = 0; i < sv_maxclients->integer; i++ ) {
		cl = &svs.clients[i];

		if( cl->state == CS_ZOMBIE || cl->state == CS_FREE ) {
			continue;
		}

		if( !cl->individual_socket ) {
			continue;
		}

		// not while, we only handle one packet per client at a time here
		if( ( ret = NET_GetPacket( cl->netchan.socket, &address, &msg ) ) != 0 ) {
			if( ret == -1 ) {
				Com_Printf( "Error receiving packet from %s: %s\n", NET_AddressToString( &cl->netchan.remoteAddress ),
							NET_ErrorString() );
				if( cl->reliable ) {
					SV_DropClient( cl, DROP_TYPE_GENERAL, "Error receiving packet: %s", NET_ErrorString() );
				}
			} else {
				if( SV_ProcessPacket( &cl->netchan, &msg ) ) {
					// this is a valid, sequenced packet, so process it
					cl->lastPacketReceivedTime = svs.realtime;
					SV_ParseClientMessage( cl, &msg );
				}
			}
		}
	}
}

/*
* SV_CheckTimeouts
*
* If a packet has not been received from a client for timeout->value
* seconds, drop the conneciton.  Server frames are used instead of
* realtime to avoid dropping the local client while debugging.
*
* When a client is normally dropped, the client_t goes into a zombie state
* for a few seconds to make sure any final reliable message gets resent
* if necessary
*/
static void SV_CheckTimeouts() {
	ZoneScoped;

	client_t *cl;
	int i;

	// timeout clients
	for( i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++ ) {
		// fake clients do not timeout
		if( cl->edict && ( cl->edict->r.svflags & SVF_FAKECLIENT ) ) {
			cl->lastPacketReceivedTime = svs.realtime;
		}
		// message times may be wrong across a changelevel
		else if( cl->lastPacketReceivedTime > svs.realtime ) {
			cl->lastPacketReceivedTime = svs.realtime;
		}

		if( cl->state == CS_ZOMBIE && cl->lastPacketReceivedTime + 1000 * sv_zombietime->value < svs.realtime ) {
			cl->state = CS_FREE; // can now be reused
			if( cl->individual_socket ) {
				NET_CloseSocket( &cl->socket );
			}
			continue;
		}

		if( cl->state != CS_FREE && cl->state != CS_ZOMBIE &&
			cl->lastPacketReceivedTime + 1000 * sv_timeout->value < svs.realtime ) {
			SV_DropClient( cl, DROP_TYPE_GENERAL, "%s", "Error: Connection timed out" );
			cl->state = CS_FREE; // don't bother with zombie state
			if( cl->socket.open ) {
				NET_CloseSocket( &cl->socket );
			}
		}

		// timeout downloads left open
		if( ( cl->state != CS_FREE && cl->state != CS_ZOMBIE ) &&
			( cl->download.name && cl->download.timeout < svs.realtime ) ) {
			Com_Printf( "Download of %s to %s%s timed out\n", cl->download.name, cl->name, S_COLOR_WHITE );
			SV_ClientCloseDownload( cl );
		}
	}
}

/*
* SV_CheckLatchedUserinfoChanges
*
* To prevent flooding other players, consecutive userinfo updates are delayed,
* and only the last one is applied.
* Applies latched userinfo updates if the timeout is over.
*/
static void SV_CheckLatchedUserinfoChanges() {
	ZoneScoped;

	client_t *cl;
	int i;
	int64_t time = Sys_Milliseconds();

	for( i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++ ) {
		if( cl->state == CS_FREE || cl->state == CS_ZOMBIE ) {
			continue;
		}

		if( cl->userinfoLatched[0] && cl->userinfoLatchTimeout <= time ) {
			Q_strncpyz( cl->userinfo, cl->userinfoLatched, sizeof( cl->userinfo ) );

			cl->userinfoLatched[0] = '\0';

			SV_UserinfoChanged( cl );
		}
	}
}

//#define WORLDFRAMETIME 25 // 40fps
//#define WORLDFRAMETIME 20 // 50fps
#define WORLDFRAMETIME 16 // 62.5fps
/*
* SV_RunGameFrame
*/
static bool SV_RunGameFrame( int msec ) {
	ZoneScoped;

	static int64_t accTime = 0;
	bool refreshSnapshot;
	bool refreshGameModule;
	bool sentFragments;

	accTime += msec;

	refreshSnapshot = false;
	refreshGameModule = false;

	sentFragments = SV_SendClientsFragments();

	// see if it's time to run a new game frame
	if( accTime >= WORLDFRAMETIME ) {
		refreshGameModule = true;
	}

	// see if it's time for a new snapshot
	if( !sentFragments && svs.gametime >= sv.nextSnapTime ) {
		refreshSnapshot = true;
		refreshGameModule = true;
	}

	// if there aren't pending packets to be sent, we can sleep
	if( is_dedicated_server && !sentFragments && !refreshSnapshot ) {
		int sleeptime = Min2( WORLDFRAMETIME - ( accTime + 1 ), sv.nextSnapTime - ( svs.gametime + 1 ) );

		if( sleeptime > 0 ) {
			socket_t *sockets[] = { &svs.socket_udp, &svs.socket_udp6 };
			socket_t *opened_sockets[ARRAY_COUNT( sockets ) + 1];
			size_t sock_ind, open_ind;

			// Pass only the opened sockets to the sleep function
			open_ind = 0;
			for( sock_ind = 0; sock_ind < ARRAY_COUNT( sockets ); sock_ind++ ) {
				socket_t *sock = sockets[sock_ind];
				if( sock->open ) {
					opened_sockets[open_ind] = sock;
					open_ind++;
				}
			}
			opened_sockets[open_ind] = NULL;

			NET_Sleep( sleeptime, opened_sockets );
		}
	}

	if( refreshGameModule ) {
		int64_t moduleTime;

		// update ping based on the last known frame from all clients
		SV_CalcPings();

		if( accTime >= WORLDFRAMETIME ) {
			moduleTime = WORLDFRAMETIME;
			accTime -= WORLDFRAMETIME;
			if( accTime >= WORLDFRAMETIME ) { // don't let it accumulate more than 1 frame
				accTime = WORLDFRAMETIME - 1;
			}
		} else {
			moduleTime = accTime;
			accTime = 0;
		}

		G_RunFrame( moduleTime );
	}

	// if we don't have to send a snapshot we are done here
	if( refreshSnapshot ) {
		int extraSnapTime;

		// set up for sending a snapshot
		sv.framenum++;
		G_SnapFrame();

		// set time for next snapshot
		extraSnapTime = (int)( svs.gametime - sv.nextSnapTime );
		if( extraSnapTime > svc.snapFrameTime * 0.5 ) { // don't let too much time be accumulated
			extraSnapTime = svc.snapFrameTime * 0.5;
		}

		sv.nextSnapTime = svs.gametime + ( svc.snapFrameTime - extraSnapTime );

		return true;
	}

	return false;
}

static void SV_CheckDefaultMap() {
	if( svc.autostarted ) {
		return;
	}

	svc.autostarted = true;
	if( is_dedicated_server ) {
		if( ( sv.state == ss_dead ) && sv_defaultmap && strlen( sv_defaultmap->string ) && !strlen( sv.mapname ) ) {
			Cbuf_ExecuteText( EXEC_APPEND, va( "map %s\n", sv_defaultmap->string ) );
		}
	}
}

/*
* SV_Frame
*/
void SV_Frame( unsigned realmsec, unsigned gamemsec ) {
	ZoneScoped;

	TracyPlot( "Server frame arena max utilisation", svs.frame_arena.max_utilisation() );
	svs.frame_arena.clear();

	u64 entropy[ 2 ];
	CSPRNG_Bytes( entropy, sizeof( entropy ) );
	svs.rng = new_rng( entropy[ 0 ], entropy[ 1 ] );

	// if server is not active, do nothing
	if( !svs.initialized ) {
		SV_CheckDefaultMap();
		return;
	}

	svs.realtime += realmsec;
	svs.gametime += gamemsec;

	// check timeouts
	SV_CheckTimeouts();

	// get packets from clients
	SV_ReadPackets();

	// apply latched userinfo changes
	SV_CheckLatchedUserinfoChanges();

	// let everything in the world think and move
	if( SV_RunGameFrame( gamemsec ) ) {
		// send messages back to the clients that had packets read this frame
		SV_SendClientMessages();

		// write snap to server demo file
		SV_Demo_WriteSnap();

		// send a heartbeat to the master if needed
		SV_MasterHeartbeat();

		// clear teleport flags, etc for next frame
		G_ClearSnap();
	}
}

//============================================================================

/*
* SV_UserinfoChanged
*
* Pull specific info from a newly changed userinfo string
* into a more C friendly form.
*/
void SV_UserinfoChanged( client_t *client ) {
	char *val;

	assert( client );
	assert( Info_Validate( client->userinfo ) );

	if( !client->edict || !( client->edict->r.svflags & SVF_FAKECLIENT ) ) {
		// force the IP key/value pair so the game can filter based on ip
		if( !Info_SetValueForKey( client->userinfo, "socket", NET_SocketTypeToString( client->netchan.socket->type ) ) ) {
			SV_DropClient( client, DROP_TYPE_GENERAL, "%s", "Error: Couldn't set userinfo (socket)\n" );
			return;
		}
		if( !Info_SetValueForKey( client->userinfo, "ip", NET_AddressToString( &client->netchan.remoteAddress ) ) ) {
			SV_DropClient( client, DROP_TYPE_GENERAL, "%s", "Error: Couldn't set userinfo (ip)\n" );
			return;
		}
	}

	// call prog code to allow overrides
	ClientUserinfoChanged( client->edict, client->userinfo );

	if( !Info_Validate( client->userinfo ) ) {
		SV_DropClient( client, DROP_TYPE_GENERAL, "%s", "Error: Invalid userinfo (after game)" );
		return;
	}

	// we assume that game module deals with setting a correct name
	val = Info_ValueForKey( client->userinfo, "name" );
	if( !val || !val[0] ) {
		SV_DropClient( client, DROP_TYPE_GENERAL, "%s", "Error: No name set" );
		return;
	}
	Q_strncpyz( client->name, val, sizeof( client->name ) );

}


//============================================================================

/*
* SV_Init
*/
void SV_Init() {
	ZoneScoped;

	assert( !sv_initialized );

	memset( &sv, 0, sizeof( sv ) );
	memset( &svs, 0, sizeof( svs ) );
	memset( &svc, 0, sizeof( svc ) );

	constexpr size_t frame_arena_size = 1024 * 1024; // 1MB
	void * frame_arena_memory = ALLOC_SIZE( sys_allocator, frame_arena_size, 16 );
	svs.frame_arena = ArenaAllocator( frame_arena_memory, frame_arena_size );

	u64 entropy[ 2 ];
	CSPRNG_Bytes( entropy, sizeof( entropy ) );
	svs.rng = new_rng( entropy[ 0 ], entropy[ 1 ] );

	SV_InitOperatorCommands();

	sv_mempool = Mem_AllocPool( NULL, "Server" );

	Cvar_Get( "sv_cheats", "0", CVAR_SERVERINFO | CVAR_LATCH );
	Cvar_Get( "protocol", va( "%i", APP_PROTOCOL_VERSION ), CVAR_SERVERINFO | CVAR_NOSET );

	sv_ip =             Cvar_Get( "sv_ip", "", CVAR_ARCHIVE | CVAR_LATCH );
	sv_port =           Cvar_Get( "sv_port", va( "%i", PORT_SERVER ), CVAR_ARCHIVE | CVAR_LATCH );

	sv_ip6 =            Cvar_Get( "sv_ip6", "::", CVAR_ARCHIVE | CVAR_LATCH );
	sv_port6 =          Cvar_Get( "sv_port6", va( "%i", PORT_SERVER ), CVAR_ARCHIVE | CVAR_LATCH );

	sv_http =           Cvar_Get( "sv_http", "1", CVAR_SERVERINFO | CVAR_ARCHIVE | CVAR_LATCH );
	sv_http_port =      Cvar_Get( "sv_http_port", sv_port->string, CVAR_ARCHIVE | CVAR_LATCH );
	sv_http_ip =        Cvar_Get( "sv_http_ip", "", CVAR_ARCHIVE | CVAR_LATCH );
	sv_http_ipv6 =      Cvar_Get( "sv_http_ipv6", "", CVAR_ARCHIVE | CVAR_LATCH );
	sv_http_upstream_baseurl =  Cvar_Get( "sv_http_upstream_baseurl", "", CVAR_ARCHIVE | CVAR_LATCH );
	sv_http_upstream_realip_header = Cvar_Get( "sv_http_upstream_realip_header", "", CVAR_ARCHIVE );
	sv_http_upstream_ip = Cvar_Get( "sv_http_upstream_ip", "", CVAR_ARCHIVE );

	rcon_password =         Cvar_Get( "rcon_password", "", 0 );
	sv_hostname =           Cvar_Get( "sv_hostname", APPLICATION " server", CVAR_SERVERINFO | CVAR_ARCHIVE );
	sv_timeout =            Cvar_Get( "sv_timeout", "125", 0 );
	sv_zombietime =         Cvar_Get( "sv_zombietime", "2", 0 );
	sv_showRcon =           Cvar_Get( "sv_showRcon", "1", 0 );
	sv_showChallenge =      Cvar_Get( "sv_showChallenge", "0", 0 );
	sv_showInfoQueries =    Cvar_Get( "sv_showInfoQueries", "0", 0 );

	sv_uploads_http =       Cvar_Get( "sv_uploads_http", "1", CVAR_READONLY );
	sv_uploads_baseurl =    Cvar_Get( "sv_uploads_baseurl", "", CVAR_ARCHIVE );
	sv_uploads_demos =      Cvar_Get( "sv_uploads_demos", "1", CVAR_ARCHIVE );
	sv_uploads_demos_baseurl =  Cvar_Get( "sv_uploads_demos_baseurl", "", CVAR_ARCHIVE );
	if( is_dedicated_server ) {
#ifdef PUBLIC_BUILD
		sv_public =     Cvar_Get( "sv_public", "1", CVAR_LATCH );
#else
		sv_public =     Cvar_Get( "sv_public", "0", CVAR_LATCH );
#endif
	} else {
		sv_public =     Cvar_Get( "sv_public", "0", CVAR_LATCH );
	}

	sv_iplimit = Cvar_Get( "sv_iplimit", "3", CVAR_ARCHIVE );

	sv_defaultmap =         Cvar_Get( "sv_defaultmap", "carfentanil", CVAR_ARCHIVE );
	sv_reconnectlimit =     Cvar_Get( "sv_reconnectlimit", "3", CVAR_ARCHIVE );
	sv_maxclients =         Cvar_Get( "sv_maxclients", "16", CVAR_ARCHIVE | CVAR_SERVERINFO | CVAR_LATCH );

	// fix invalid sv_maxclients values
	if( sv_maxclients->integer < 1 ) {
		Cvar_FullSet( "sv_maxclients", "1", CVAR_ARCHIVE | CVAR_SERVERINFO | CVAR_LATCH, true );
	} else if( sv_maxclients->integer > MAX_CLIENTS ) {
		Cvar_FullSet( "sv_maxclients", va( "%i", MAX_CLIENTS ), CVAR_ARCHIVE | CVAR_SERVERINFO | CVAR_LATCH, true );
	}

	sv_demodir = Cvar_Get( "sv_demodir", "", CVAR_NOSET );
	if( sv_demodir->string[0] && Com_GlobMatch( "*[^0-9a-zA-Z_@]*", sv_demodir->string, false ) ) {
		Com_Printf( "Invalid demo prefix string: %s\n", sv_demodir->string );
		Cvar_ForceSet( "sv_demodir", "" );
	}

	sv_debug_serverCmd =        Cvar_Get( "sv_debug_serverCmd", "0", CVAR_ARCHIVE );

	// this is a message holder for shared use
	MSG_Init( &tmpMessage, tmpMessageData, sizeof( tmpMessageData ) );

	// init server updates ratio
	constexpr float pps = 20.0f;
	constexpr float fps = 62.0f;
	STATIC_ASSERT( fps >= pps );
	svc.snapFrameTime = int( 1000.0f / pps );
	svc.gameFrameTime = int( 1000.0f / fps );

	//init the master servers list
	SV_InitMaster();

	SV_Web_Init();

	sv_initialized = true;
}

/*
* SV_Shutdown
*
* Called once when the program is shutting down
*/
void SV_Shutdown( const char *finalmsg ) {
	if( !sv_initialized ) {
		return;
	}
	sv_initialized = false;

	SV_Web_Shutdown();
	SV_ShutdownGame( finalmsg, false );

	SV_ShutdownOperatorCommands();

	Mem_FreePool( &sv_mempool );

	FREE( sys_allocator, svs.frame_arena.get_memory() );
}
