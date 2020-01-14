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
#define BUTTON_ATTACK               1
#define BUTTON_WALK                 2
#define BUTTON_SPECIAL              4
#define BUTTON_ZOOM                 8
#define BUTTON_RELOAD               16

enum {
	KEYICON_FORWARD = 0,
	KEYICON_BACKWARD,
	KEYICON_LEFT,
	KEYICON_RIGHT,
	KEYICON_FIRE,
	KEYICON_JUMP,
	KEYICON_CROUCH,
	KEYICON_SPECIAL,
	KEYICON_TOTAL
};

// user command communications
#define CMD_BACKUP  64  // allow a lot of command backups for very fast systems
#define CMD_MASK    ( CMD_BACKUP - 1 )

// pmove_state_t is the information necessary for client side movement
// prediction
typedef enum {
	// can accelerate and turn
	PM_NORMAL,
	PM_SPECTATOR,

	// no acceleration or turning
	PM_GIB,         // different bounding box
	PM_FREEZE,
	PM_CHASECAM     // same as freeze, but so client knows it's in chasecam
} pmtype_t;

// pmove->pm_flags
#define PMF_WALLJUMPCOUNT   ( 1 << 0 )
#define PMF_ON_GROUND       ( 1 << 1 )
#define PMF_TIME_WATERJUMP  ( 1 << 2 )  // pm_time is waterjump
#define PMF_TIME_LAND       ( 1 << 3 )  // pm_time is time before rejump
#define PMF_TIME_TELEPORT   ( 1 << 4 )  // pm_time is non-moving time
#define PMF_NO_PREDICTION   ( 1 << 5 )  // temporarily disables prediction (used for grappling hook)
#define PMF_DASHING         ( 1 << 6 )  // Dashing flag
#define PMF_SPECIAL_HELD    ( 1 << 7 )  // Special flag
#define PMF_WALLJUMPING     ( 1 << 8 )  // WJ starting flag
#define PMF_DOUBLEJUMPED    ( 1 << 9 )  // DJ stat flag
#define PMF_JUMPPAD_TIME    ( 1 << 10 ) // temporarily disables fall damage

// note that Q_rint was causing problems here
// (spawn looking straight up\down at delta_angles wrapping)

#define ANGLE2SHORT( x )    ( (int)( ( x ) * 65536 / 360 ) & 65535 )
#define SHORT2ANGLE( x )    ( ( x ) * ( 360.0 / 65536 ) )

#define MAX_GAMECOMMANDS    256     // command names for command completion

//
// config strings are a general means of communication from
// the server to all connected clients.
// Each config string can be at most MAX_QPATH characters.
//
#define CS_HOSTNAME         0
#define CS_MAXCLIENTS       1

#define SERVER_PROTECTED_CONFIGSTRINGS 4

#define CS_MAPNAME          4
#define CS_GAMETYPENAME     5
#define CS_AUTORECORDSTATE  6

#define CS_MATCHSCORE       7

#define CS_CALLVOTE 8
#define CS_CALLVOTE_YES_VOTES 9
#define CS_CALLVOTE_NO_VOTES 10
#define CS_CALLVOTE_REQUIRED_VOTES 11

#define CS_WORLDMODEL       30
#define CS_MAPCHECKSUM      31      // for catching cheater maps

//precache stuff begins here
#define CS_MODELS           32
#define CS_SOUNDS           ( CS_MODELS + MAX_MODELS )
#define CS_IMAGES           ( CS_SOUNDS + MAX_SOUNDS )
#define CS_PLAYERINFOS      ( CS_IMAGES + MAX_IMAGES )
#define CS_GAMECOMMANDS     ( CS_PLAYERINFOS + MAX_CLIENTS )
#define MAX_CONFIGSTRINGS   ( CS_GAMECOMMANDS + MAX_GAMECOMMANDS )

//==============================================

constexpr const char * MASTER_SERVERS[] = { "dpmaster.deathmask.net", "ghdigital.com", "excalibur.nvg.ntnu.no" };
#define DEFAULT_MASTER_SERVERS_IPS          "dpmaster.deathmask.net ghdigital.com excalibur.nvg.ntnu.no"
#define SERVER_PINGING_TIMEOUT              50
#define LAN_SERVER_PINGING_TIMEOUT          20
#define DEFAULT_PLAYERMODEL                 "bigvic"

// SyncEntityState is the information conveyed from the server
// in an update message about entities that the client will
// need to render in some way

// edict->svflags
#define SVF_NOCLIENT            0x00000001      // don't send entity to clients, even if it has effects
#define SVF_PORTAL              0x00000002      // merge PVS at old_origin
#define SVF_TRANSMITORIGIN2     0x00000008      // always send old_origin (beams, etc)
#define SVF_SOUNDCULL           0x00000010      // distance culling
#define SVF_FAKECLIENT          0x00000020      // do not try to send anything to this client
#define SVF_BROADCAST           0x00000040      // always transmit
#define SVF_CORPSE              0x00000080      // treat as CONTENTS_CORPSE for collision
#define SVF_PROJECTILE          0x00000100      // sets s.solid to SOLID_NOT for prediction
#define SVF_ONLYTEAM            0x00000200      // this entity is only transmited to clients with the same ent->s.team value
#define SVF_FORCEOWNER          0x00000400      // this entity forces the entity at s.ownerNum to be included in the snapshot
#define SVF_ONLYOWNER           0x00000800      // this entity is only transmitted to its owner
#define SVF_FORCETEAM           0x00001000      // this entity is always transmitted to clients with the same ent->s.team value

// edict->solid values
typedef enum {
	SOLID_NOT,              // no interaction with other objects
	SOLID_TRIGGER,          // only touch when inside, after moving
	SOLID_YES               // touch on edge
} solid_t;

#define SOLID_BMODEL    31  // special value for bmodel

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

enum {
	DROP_TYPE_GENERAL,
	DROP_TYPE_PASSWORD,
	DROP_TYPE_NORECONNECT,
	DROP_TYPE_TOTAL
};

enum {
	DROP_REASON_CONNFAILED,
	DROP_REASON_CONNTERMINATED,
	DROP_REASON_CONNERROR
};

#define DROP_FLAG_AUTORECONNECT 1       // it's okay try reconnectting automatically

typedef enum {
	DOWNLOADTYPE_NONE,
	DOWNLOADTYPE_SERVER,
	DOWNLOADTYPE_WEB
} downloadtype_t;

//==============================================

typedef enum {
	HTTP_METHOD_BAD = -1,
	HTTP_METHOD_NONE = 0,
	HTTP_METHOD_GET  = 1,
	HTTP_METHOD_POST = 2,
	HTTP_METHOD_PUT  = 3,
	HTTP_METHOD_HEAD = 4,
} http_query_method_t;

typedef enum {
	HTTP_RESP_NONE = 0,
	HTTP_RESP_OK = 200,
	HTTP_RESP_PARTIAL_CONTENT = 206,
	HTTP_RESP_BAD_REQUEST = 400,
	HTTP_RESP_FORBIDDEN = 403,
	HTTP_RESP_NOT_FOUND = 404,
	HTTP_RESP_REQUEST_TOO_LARGE = 413,
	HTTP_RESP_REQUESTED_RANGE_NOT_SATISFIABLE = 416,
	HTTP_RESP_SERVICE_UNAVAILABLE = 503,
} http_response_code_t;
