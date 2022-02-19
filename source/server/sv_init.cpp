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
#include "qcommon/cmodel.h"
#include "qcommon/csprng.h"
#include "qcommon/hash.h"

server_constant_t svc;              // constant server info (trully persistant since sv_init)
server_static_t svs;                // persistant server info
server_t sv;                 // local server

/*
* SV_CreateBaseline
*
* Entity baselines are used to compress the update messages
* to the clients -- only the fields that differ from the
* baseline will be transmitted
*/
static void SV_CreateBaseline() {
	for( int entnum = 1; entnum < sv.gi.num_edicts; entnum++ ) {
		edict_t * svent = EDICT_NUM( entnum );
		if( !svent->r.inuse ) {
			continue;
		}

		sv.baselines[entnum] = svent->s;
	}
}

void SV_SetServerConfigStrings() {
	snprintf( sv.configstrings[CS_MAXCLIENTS], sizeof( sv.configstrings[CS_MAXCLIENTS] ), "%i", sv_maxclients->integer );
	Q_strncpyz( sv.configstrings[CS_HOSTNAME], Cvar_String( "sv_hostname" ), sizeof( sv.configstrings[CS_HOSTNAME] ) );
}

/*
* SV_SpawnServer
* Change the server to a new map, taking all connected clients along with it.
*/
static void SV_SpawnServer( const char *mapname, bool devmap ) {
	if( devmap ) {
		Cvar_ForceSet( "sv_cheats", "1" );
	}
	ResetCheatCvars();

	Com_Printf( "SpawnServer: %s\n", mapname );

	svs.spawncount++;   // any partially connected client will be restarted

	Com_SetServerState( ss_dead );

	// wipe the entire per-level structure
	memset( &sv, 0, sizeof( sv ) );

	SV_ResetClientFrameCounters();
	svs.realtime = Sys_Milliseconds();
	svs.gametime = 0;

	SV_SetServerConfigStrings();

	sv.nextSnapTime = 1000;

	G_LoadMap( mapname );

	Cvar_ForceSet( "mapname", sv.mapname );

	//
	// spawn the rest of the entities on the map
	//

	// precache and static commands can be issued during
	// map initialization
	sv.state = ss_loading;
	Com_SetServerState( sv.state );

	// load and spawn all other entities
	G_InitLevel( sv.mapname, 0 );
	G_CallVotes_Init();

	// run two frames to allow everything to settle
	G_RunFrame( svc.snapFrameTime );
	G_RunFrame( svc.snapFrameTime );

	SV_CreateBaseline(); // create a baseline for more efficient communications

	// all precaches are complete
	sv.state = ss_game;
	Com_SetServerState( sv.state );
}

/*
* SV_InitGame
* A brand new game has been started
*/
void SV_InitGame() {
	int i;
	edict_t *ent;
	netadr_t address, ipv6_address;
	bool socket_opened = false;

	// make sure the client is down
	CL_Disconnect( NULL );

	if( svs.initialized ) {
		// cause any connected clients to reconnect
		SV_ShutdownGame( "Server restarted", true );
	}

	u64 entropy[ 2 ];
	CSPRNG( entropy, sizeof( entropy ) );
	svs.rng = NewRNG( entropy[ 0 ], entropy[ 1 ] );

	svs.initialized = true;

	if( sv_maxclients->integer < 1 || sv_maxclients->integer > MAX_CLIENTS ) {
		Cvar_ForceSet( "sv_maxclients", sv_maxclients->default_value );
	}

	svs.spawncount = RandomUniform( &svs.rng, 0, S16_MAX );

	svs.clients = ALLOC_MANY( sys_allocator, client_t, sv_maxclients->integer );
	memset( svs.clients, 0, sizeof( svs.clients[ 0 ] ) * sv_maxclients->integer );

	svs.client_entities.num_entities = sv_maxclients->integer * UPDATE_BACKUP * MAX_SNAP_ENTITIES;
	svs.client_entities.entities = ALLOC_MANY( sys_allocator, SyncEntityState, svs.client_entities.num_entities );
	memset( svs.client_entities.entities, 0, sizeof( svs.client_entities.entities[ 0 ] ) * svs.client_entities.num_entities );

	// init network stuff

	address.type = NA_NOTRANSMIT;
	ipv6_address.type = NA_NOTRANSMIT;

	if( !is_dedicated_server ) {
		NET_InitAddress( &address, NA_LOOPBACK );
		if( !NET_OpenSocket( &svs.socket_loopback, SOCKET_LOOPBACK, &address, true ) ) {
			Fatal( "Couldn't open loopback socket: %s\n", NET_ErrorString() );
		}
	}

	if( is_dedicated_server || sv_maxclients->integer > 1 ) {
		// IPv4
		NET_StringToAddress( sv_ip->value, &address );
		NET_SetAddressPort( &address, sv_port->integer );
		if( !NET_OpenSocket( &svs.socket_udp, SOCKET_UDP, &address, true ) ) {
			Com_Printf( "Error: Couldn't open UDP socket: %s\n", NET_ErrorString() );
		} else {
			socket_opened = true;
		}

		// IPv6
		NET_StringToAddress( sv_ip6->value, &ipv6_address );
		if( ipv6_address.type == NA_IPv6 ) {
			NET_SetAddressPort( &ipv6_address, sv_port6->integer );
			if( !NET_OpenSocket( &svs.socket_udp6, SOCKET_UDP, &ipv6_address, true ) ) {
				Com_Printf( "Error: Couldn't open UDP6 socket: %s\n", NET_ErrorString() );
			} else {
				socket_opened = true;
			}
		} else {
			Com_Printf( "Error: invalid IPv6 address: %s\n", sv_ip6->value );
		}
	}

	if( is_dedicated_server && !socket_opened ) {
		Fatal( "Couldn't open any socket\n" );
	}

	// init game
	SV_InitGameProgs();
	for( i = 0; i < sv_maxclients->integer; i++ ) {
		ent = EDICT_NUM( i + 1 );
		ent->s.number = i + 1;
		svs.clients[i].edict = ent;
	}
}

/*
* SV_FinalMessage
*
* Used by SV_ShutdownGame to send a final message to all
* connected clients before the server goes down.  The messages are sent immediately,
* not just stuck on the outgoing message list, because the server is going
* to totally exit after returning from this function.
*/
static void SV_FinalMessage( const char *message, bool reconnect ) {
	for( int i = 0; i < sv_maxclients->integer; i++ ) {
		client_t * cl = &svs.clients[ i ];
		if( cl->edict && ( cl->edict->s.svflags & SVF_FAKECLIENT ) ) {
			continue;
		}
		if( cl->state >= CS_CONNECTING ) {
			if( reconnect ) {
				SV_SendServerCommand( cl, "forcereconnect \"%s\"", message );
			} else {
				SV_SendServerCommand( cl, "disconnect \"%s\"", message );
			}

			SV_InitClientMessage( cl, &tmpMessage, NULL, 0 );
			SV_AddReliableCommandsToMessage( cl, &tmpMessage );

			// send it twice
			for( int j = 0; j < 2; j++ ) {
				SV_SendMessageToClient( cl, &tmpMessage );
			}
		}
	}
}

/*
* SV_ShutdownGame
*
* Called when each game quits
*/
void SV_ShutdownGame( const char *finalmsg, bool reconnect ) {
	if( !svs.initialized ) {
		return;
	}

	if( svs.demo.file ) {
		SV_Demo_Stop_f();
	}

	SV_FinalMessage( finalmsg, reconnect );

	SV_ShutdownGameProgs();

	NET_CloseSocket( &svs.socket_loopback );
	NET_CloseSocket( &svs.socket_udp );
	NET_CloseSocket( &svs.socket_udp6 );

	for( int i = 0; i < sv_maxclients->integer; i++ ) {
		SNAP_FreeClientFrames( &svs.clients[ i ] );
	}

	FREE( sys_allocator, svs.clients );
	FREE( sys_allocator, svs.client_entities.entities );

	if( svs.cms ) {
		CM_Free( CM_Server, svs.cms );
		svs.cms = NULL;
	}

	Com_SetServerState( ss_dead );
	svs.initialized = false;
}

/*
* SV_Map
* command from the console or progs.
*/
void SV_Map( const char * map, bool devmap ) {
	TracyZoneScoped;

	if( svs.demo.file ) {
		SV_Demo_Stop_f();
	}

	if( sv.state == ss_dead ) {
		SV_InitGame(); // the game is just starting
	}

	// remove all bots before changing map
	for( int i = 0; i < sv_maxclients->integer; i++ ) {
		client_t * cl = &svs.clients[ i ];
		if( cl->state && cl->edict && ( cl->edict->s.svflags & SVF_FAKECLIENT ) ) {
			SV_DropClient( cl, NULL );
		}
	}

	// wsw : Medar : this used to be at SV_SpawnServer, but we need to do it before sending changing
	// so we don't send frames after sending changing command
	// leave slots at start for clients only
	for( int i = 0; i < sv_maxclients->integer; i++ ) {
		// needs to reconnect
		if( svs.clients[i].state > CS_CONNECTING ) {
			svs.clients[i].state = CS_CONNECTING;
		}

		svs.clients[i].lastframe = -1;
		memset( svs.clients[i].gameCommands, 0, sizeof( svs.clients[i].gameCommands ) );
	}

	SV_BroadcastCommand( "changing\n" );
	SV_SendClientMessages();
	SV_SpawnServer( map, devmap );
	SV_BroadcastCommand( "reconnect\n" );
}
