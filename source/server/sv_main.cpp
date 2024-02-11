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

Cvar *sv_port;

Cvar *sv_downloadurl;

Cvar *sv_timeout;            // seconds without any message
Cvar *sv_zombietime;         // seconds to sink messages after disconnect

Cvar *sv_maxclients;

Cvar *sv_showChallenge;
Cvar *sv_showInfoQueries;

Cvar *sv_hostname;
Cvar *sv_public;         // should heartbeats be sent
Cvar *sv_defaultmap;

Cvar *sv_iplimit;

// wsw : debug netcode
Cvar *sv_debug_serverCmd;

Cvar *sv_demodir;

//============================================================================

static void SV_CalcPings() {
	unsigned int i, j;
	client_t *cl;
	unsigned int total, count, lat, best;

	for( i = 0; i < (unsigned int)sv_maxclients->integer; i++ ) {
		cl = &svs.clients[i];
		if( cl->state != CS_SPAWNED ) {
			continue;
		}
		if( cl->edict && ( cl->edict->s.svflags & SVF_FAKECLIENT ) ) {
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
	}
}

static bool SV_ProcessPacket( netchan_t *netchan, msg_t *msg ) {
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

static void SV_ReadPackets() {
	TracyZoneScoped;

	uint8_t data[ MAX_MSGLEN ];
	NetAddress source;
	size_t bytes_received = UDPReceive( svs.socket, &source, data, sizeof( data ) );
	if( bytes_received == 0 ) {
		return;
	}

	msg_t msg = NewMSGReader( data, bytes_received, sizeof( data ) );

	// check for connectionless packet (0xffffffff) first
	if( MSG_ReadInt32( &msg ) == -1 ) {
		SV_ConnectionlessPacket( source, &msg );
		return;
	}

	MSG_BeginReading( &msg );
	MSG_ReadInt32( &msg ); // sequence number
	MSG_ReadInt32( &msg ); // sequence number
	u64 session_id = MSG_ReadUint64( &msg );

	for( int i = 0; i < sv_maxclients->integer; i++ ) {
		client_t * cl = &svs.clients[ i ];

		if( cl->state == CS_FREE || cl->state == CS_ZOMBIE ) {
			continue;
		}
		if( cl->edict && ( cl->edict->s.svflags & SVF_FAKECLIENT ) ) {
			continue;
		}

		if( cl->netchan.session_id != session_id ) {
			continue;
		}

		cl->netchan.remoteAddress = source;

		if( SV_ProcessPacket( &cl->netchan, &msg ) ) { // this is a valid, sequenced packet, so process it
			cl->lastPacketReceivedTime = svs.realtime;
			SV_ParseClientMessage( cl, &msg );
		}

		break;
	}
}

/*
* SV_CheckTimeouts
*
* If a packet has not been received from a client for timeout->number
* seconds, drop the conneciton.  Server frames are used instead of
* realtime to avoid dropping the local client while debugging.
*
* When a client is normally dropped, the client_t goes into a zombie state
* for a few seconds to make sure any final reliable message gets resent
* if necessary
*/
static void SV_CheckTimeouts() {
	TracyZoneScoped;

	client_t *cl;
	int i;

	// timeout clients
	for( i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++ ) {
		// fake clients do not timeout
		if( cl->edict && ( cl->edict->s.svflags & SVF_FAKECLIENT ) ) {
			cl->lastPacketReceivedTime = svs.realtime;
		}
		// message times may be wrong across a changelevel
		else if( cl->lastPacketReceivedTime > svs.realtime ) {
			cl->lastPacketReceivedTime = svs.realtime;
		}

		if( cl->state == CS_ZOMBIE && cl->lastPacketReceivedTime + 1000 * sv_zombietime->number < svs.realtime ) {
			cl->state = CS_FREE; // can now be reused
			continue;
		}

		if( cl->state != CS_FREE && cl->state != CS_ZOMBIE &&
			cl->lastPacketReceivedTime + 1000 * sv_timeout->number < svs.realtime ) {
			SV_DropClient( cl, "%s", "Error: Connection timed out" );
			cl->state = CS_FREE; // don't bother with zombie state
		}
	}
}

#define WORLDFRAMETIME 16 // 62.5fps
static bool SV_RunGameFrame( int msec ) {
	TracyZoneScoped;

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
			TracyZoneScopedN( "WaitForSockets" );
			TempAllocator temp = svs.frame_arena.temp();
			WaitForSockets( &temp, &svs.socket, 1, sleeptime, WaitForSocketWriteable_No, NULL );
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

void SV_Frame( unsigned realmsec, unsigned gamemsec ) {
	TracyZoneScoped;

	TracyCPlot( "Server frame arena max utilisation", svs.frame_arena.max_utilisation() );
	svs.frame_arena.clear();

	u64 entropy[ 2 ];
	CSPRNG( entropy, sizeof( entropy ) );
	svs.rng = NewRNG( entropy[ 0 ], entropy[ 1 ] );

	// if server is not active, do nothing
	if( !svs.initialized ) {
		return;
	}

	svs.realtime += realmsec;
	svs.gametime += gamemsec;

	// check timeouts
	SV_CheckTimeouts();

	// get packets from clients
	SV_ReadPackets();

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

void SV_Init() {
	TracyZoneScoped;

	Assert( !sv_initialized );

	memset( &sv, 0, sizeof( sv ) );
	memset( &svs, 0, sizeof( svs ) );
	memset( &svc, 0, sizeof( svc ) );

	constexpr size_t frame_arena_size = 1024 * 1024 * 32; // 32MB
	void * frame_arena_memory = sys_allocator->allocate( frame_arena_size, 16 );
	svs.frame_arena = ArenaAllocator( frame_arena_memory, frame_arena_size );

	u64 entropy[ 2 ];
	CSPRNG( entropy, sizeof( entropy ) );
	svs.rng = NewRNG( entropy[ 0 ], entropy[ 1 ] );

	TempAllocator temp = svs.frame_arena.temp();

	SV_InitOperatorCommands();

	NewCvar( "protocol", temp.sv( "{}", s32( APP_PROTOCOL_VERSION ) ), CvarFlag_ServerInfo | CvarFlag_ReadOnly );
	NewCvar( "serverid", temp.sv( "{}", Random64( &svs.rng ) ), CvarFlag_ServerInfo | CvarFlag_ReadOnly );

	sv_port = NewCvar( "sv_port", temp.sv( "{}", PORT_SERVER ), CvarFlag_Archive | CvarFlag_ServerReadOnly );

	sv_downloadurl = NewCvar( "sv_downloadurl", "", CvarFlag_Archive | CvarFlag_ServerReadOnly );

	sv_hostname = NewCvar( "sv_hostname", APPLICATION " server", CvarFlag_ServerInfo | CvarFlag_Archive );
	sv_timeout = NewCvar( "sv_timeout", "15" );
	sv_zombietime = NewCvar( "sv_zombietime", "2" );
	sv_showChallenge = NewCvar( "sv_showChallenge", "0" );
	sv_showInfoQueries = NewCvar( "sv_showInfoQueries", "0" );

	sv_public = NewCvar( "sv_public", is_public_build && is_dedicated_server ? "1" : "0", CvarFlag_ServerReadOnly );

	sv_iplimit = NewCvar( "sv_iplimit", "3", CvarFlag_Archive );

	sv_defaultmap = NewCvar( "sv_defaultmap", "carfentanil", CvarFlag_Archive );
	NewCvar( "mapname", "", CvarFlag_ServerInfo | CvarFlag_ReadOnly );
	sv_maxclients = NewCvar( "sv_maxclients", "16", CvarFlag_Archive | CvarFlag_ServerInfo | CvarFlag_ServerReadOnly );

	// fix invalid sv_maxclients values
	if( sv_maxclients->integer < 1 ) {
		Cvar_ForceSet( "sv_maxclients", "1" );
	} else if( sv_maxclients->integer > MAX_CLIENTS ) {
		Cvar_ForceSet( "sv_maxclients", temp.sv( "{}", MAX_CLIENTS ) );
	}

	sv_demodir = NewCvar( "sv_demodir", "server", CvarFlag_ServerReadOnly );

	g_autorecord = NewCvar( "g_autorecord", is_dedicated_server ? "1" : "0", CvarFlag_Archive );
	g_autorecord_maxdemos = NewCvar( "g_autorecord_maxdemos", "200", CvarFlag_Archive );

	sv_debug_serverCmd = NewCvar( "sv_debug_serverCmd", "0" );

	// this is a message holder for shared use
	tmpMessage = NewMSGWriter( tmpMessageData, sizeof( tmpMessageData ) );

	// init server updates ratio
	constexpr float pps = 20.0f;
	constexpr float fps = 62.0f;
	STATIC_ASSERT( fps >= pps );
	svc.snapFrameTime = int( 1000.0f / pps );
	svc.gameFrameTime = int( 1000.0f / fps );

	//init the master servers list
	SV_InitMaster();

	sv_initialized = true;

	if( is_dedicated_server ) {
		SV_Map( sv_defaultmap->value, false );
	}
}

void SV_Shutdown( const char *finalmsg ) {
	TracyZoneScoped;

	if( !sv_initialized ) {
		return;
	}
	sv_initialized = false;

	SV_ShutdownGame( finalmsg, false );

	SV_ShutdownOperatorCommands();

	Free( sys_allocator, svs.frame_arena.get_memory() );
}
