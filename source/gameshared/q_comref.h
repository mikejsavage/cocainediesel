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

//
// button bits
//
#define BUTTON_ATTACK1	( 1 << 0 )
#define BUTTON_ATTACK2	( 1 << 1 )
#define BUTTON_ABILITY1	( 1 << 2 )
#define BUTTON_ABILITY2	( 1 << 3 )
#define BUTTON_RELOAD	( 1 << 4 )
#define BUTTON_GADGET	( 1 << 5 )
#define BUTTON_PLANT	( 1 << 6 )

// user command communications
#define CMD_BACKUP  64  // allow a lot of command backups for very fast systems
#define CMD_MASK    ( CMD_BACKUP - 1 )

// pmove_state_t is the information necessary for client side movement
// prediction
enum pmtype_t {
	// can accelerate and turn
	PM_NORMAL,
	PM_SPECTATOR,

	// no acceleration or turning
	PM_FREEZE,
	PM_CHASECAM     // same as freeze, but so client knows it's in chasecam
};

// pmove->pm_flags
#define PMF_ON_GROUND       ( 1 << 0 )
#define PMF_TIME_WATERJUMP  ( 1 << 1 )  // pm_time is waterjump
#define PMF_TIME_TELEPORT   ( 1 << 2 )  // pm_time is non-moving time
#define PMF_NO_PREDICTION   ( 1 << 3 )  // temporarily disables prediction (used for grappling hook)
#define PMF_ABILITY1_HELD   ( 1 << 4 )  // Special held flag
#define PMF_ABILITY2_HELD   ( 1 << 5 )  // Jump held flag

// note that Q_rint was causing problems here
// (spawn looking straight up\down at delta_angles wrapping)

#define ANGLE2SHORT( x )    ( (int)( ( x ) * 65536 / 360 ) & 65535 )
#define SHORT2ANGLE( x )    ( ( x ) * ( 360.0 / 65536 ) )

#define MAX_GAMECOMMANDS    256     // command names for command completion

//
// config strings are a general means of communication from
// the server to all connected clients.
//

#define CS_HOSTNAME         0
#define CS_MAXCLIENTS       1

#define CS_AUTORECORDSTATE  2

#define CS_MATCHSCORE       5

#define CS_CALLVOTE 6

//precache stuff begins here
#define CS_PLAYERINFOS      32
#define CS_GAMECOMMANDS     ( CS_PLAYERINFOS + MAX_CLIENTS )
#define MAX_CONFIGSTRINGS   ( CS_GAMECOMMANDS + MAX_GAMECOMMANDS )

//==============================================

constexpr const char * MASTER_SERVERS[] = { "dpmaster.deathmask.net", "excalibur.nvg.ntnu.no" };
#define SERVER_PINGING_TIMEOUT              50
#define LAN_SERVER_PINGING_TIMEOUT          20

// SyncEntityState is the information conveyed from the server
// in an update message about entities that the client will
// need to render in some way

// edict->svflags
#define SVF_NOCLIENT         ( 1 << 0 )      // don't send entity to clients, even if it has effects
#define SVF_SOUNDCULL        ( 1 << 1 )      // distance culling
#define SVF_FAKECLIENT       ( 1 << 2 )      // do not try to send anything to this client
#define SVF_BROADCAST        ( 1 << 3 )      // always transmit
#define SVF_CORPSE           ( 1 << 4 )      // treat as CONTENTS_CORPSE for collision
#define SVF_PROJECTILE       ( 1 << 5 )      // sets s.solid to SOLID_NOT for prediction
#define SVF_ONLYTEAM         ( 1 << 6 )      // this entity is only transmited to clients with the same ent->s.team value
#define SVF_FORCEOWNER       ( 1 << 7 )      // this entity forces the entity at s.ownerNum to be included in the snapshot
#define SVF_ONLYOWNER        ( 1 << 8 )      // this entity is only transmitted to its owner
#define SVF_OWNERANDCHASERS  ( 1 << 9 )      // this entity is only transmitted to its owner and people spectating them
#define SVF_FORCETEAM        ( 1 << 10 )      // this entity is always transmitted to clients with the same ent->s.team value
#define SVF_NEVEROWNER       ( 1 << 11 )      // this entity is tramitted to everyone but its owner

// edict->solid values
enum solid_t {
	SOLID_NOT,              // no interaction with other objects
	SOLID_TRIGGER,          // only touch when inside, after moving
	SOLID_YES               // touch on edge
};

// SyncEntityState->event values
// entity events are for effects that take place relative
// to an existing entities origin. Very network efficient.

#define ISEVENTENTITY( x ) ( ( (SyncEntityState *)x )->type >= EVENT_ENTITIES_START )

//==============================================

enum connstate_t {
	CA_UNINITIALIZED,
	CA_DISCONNECTED,                    // not talking to a server
	CA_CONNECTING,                      // sending request packets to the server
	CA_HANDSHAKE,                       // netchan_t established, waiting for svc_serverdata
	CA_CONNECTED,                       // connection established, game module not loaded
	CA_ACTIVE,                          // game views should be displayed
};

enum server_state_t {
	ss_dead,        // no map loaded
	ss_loading,     // spawning level edicts
	ss_game         // actively running
};

#define DROP_FLAG_AUTORECONNECT 1       // it's okay try reconnectting automatically
