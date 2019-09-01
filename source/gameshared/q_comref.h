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

#include "q_arch.h"

//
// button bits
//
#define BUTTON_NONE                 0
#define BUTTON_ATTACK               1
#define BUTTON_WALK                 2
#define BUTTON_SPECIAL              4
#define BUTTON_ZOOM                 8

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

// usercmd_t is sent to the server each client frame
typedef struct usercmd_s {
	uint8_t msec;
	uint32_t buttons;
	int64_t serverTimeStamp;
	int16_t angles[3];
	int8_t forwardmove, sidemove, upmove;
} usercmd_t;

#define MAX_PM_STATS 16

enum {
	PM_STAT_FEATURES,
	PM_STAT_NOUSERCONTROL,
	PM_STAT_KNOCKBACK,
	PM_STAT_CROUCHTIME,
	PM_STAT_ZOOMTIME,
	PM_STAT_DASHTIME,
	PM_STAT_WJTIME,
	PM_STAT_MAXSPEED,
	PM_STAT_JUMPSPEED,
	PM_STAT_DASHSPEED,

	PM_STAT_SIZE = MAX_PM_STATS
};

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

typedef struct {
	int pm_type;

	float origin[3];
	float velocity[3];

	int pm_flags;               // ducked, jump_held, etc
	int pm_time;                // each unit = 8 ms
	short stats[PM_STAT_SIZE];  // Kurim : timers for knockback, doublejump, walljump
	int gravity;
	short delta_angles[3];      // add to command angles to get view direction
	                            // changed by spawns, rotating objects, and teleporters
} pmove_state_t;

#define MAXTOUCH    32


//==========================================================
//
//  ELEMENTS COMMUNICATED ACROSS THE NET
//
//==========================================================


// note that Q_rint was causing problems here
// (spawn looking straight up\down at delta_angles wrapping)

#define ANGLE2SHORT( x )    ( (int)( ( x ) * 65536 / 360 ) & 65535 )
#define SHORT2ANGLE( x )    ( ( x ) * ( 360.0 / 65536 ) )

#define ANGLE2BYTE( x )     ( (int)( ( x ) * 256 / 360 ) & 255 )
#define BYTE2ANGLE( x )     ( ( x ) * ( 360.0 / 256 ) )

#define MAX_GAMECOMMANDS    256     // command names for command completion
#define MAX_WEAPONDEFS      MAX_ITEMS

//
// config strings are a general means of communication from
// the server to all connected clients.
// Each config string can be at most MAX_QPATH characters.
//
#define CS_HOSTNAME         0
#define CS_MAXCLIENTS       1

#define SERVER_PROTECTED_CONFIGSTRINGS 4

#define CS_MAPNAME          4
#define CS_STATNUMS         5
#define CS_GAMETYPENAME     6
#define CS_AUTORECORDSTATE  7

#define CS_TEAM_SPECTATOR_NAME 8
#define CS_TEAM_PLAYERS_NAME 9
#define CS_TEAM_ALPHA_NAME  10
#define CS_TEAM_BETA_NAME   11

#define CS_MATCHNAME        12
#define CS_MATCHSCORE       13

#define CS_ACTIVE_CALLVOTE  14
#define CS_ACTIVE_CALLVOTE_VOTES 15

#define CS_WORLDMODEL       30
#define CS_MAPCHECKSUM      31      // for catching cheater maps

//precache stuff begins here
#define CS_MODELS           32
#define CS_SOUNDS           ( CS_MODELS + MAX_MODELS )
#define CS_IMAGES           ( CS_SOUNDS + MAX_SOUNDS )
#define CS_ITEMS            ( CS_IMAGES + MAX_IMAGES )
#define CS_PLAYERINFOS      ( CS_ITEMS + MAX_ITEMS )
#define CS_GAMECOMMANDS     ( CS_PLAYERINFOS + MAX_CLIENTS )
#define CS_WEAPONDEFS       ( CS_GAMECOMMANDS + MAX_GAMECOMMANDS )
#define CS_GENERAL          ( CS_WEAPONDEFS + MAX_WEAPONDEFS )

#define MAX_CONFIGSTRINGS   ( CS_GENERAL + MAX_GENERAL )

//==============================================

constexpr const char * MASTER_SERVERS[] = { "dpmaster.deathmask.net", "ghdigital.com", "excalibur.nvg.ntnu.no" };
#define DEFAULT_MASTER_SERVERS_IPS          "dpmaster.deathmask.net ghdigital.com excalibur.nvg.ntnu.no"
#define SERVER_PINGING_TIMEOUT              50
#define LAN_SERVER_PINGING_TIMEOUT          20
#define DEFAULT_PLAYERMODEL                 "bigvic"

// entity_state_t is the information conveyed from the server
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

// entity_state_t->event values
// entity events are for effects that take place relative
// to an existing entities origin. Very network efficient.

#define EVENT_ENTITIES_START    96 // entity types above this index will get event treatment
#define ISEVENTENTITY( x ) ( ( (entity_state_t *)x )->type >= EVENT_ENTITIES_START )

//==============================================

// primitive encoding types for network messages

typedef enum {
	WIRE_BOOL,					// a of value of 'true' is represented by a single bit in the header

	WIRE_FIXED_INT8,				// 8-bit integer
	WIRE_FIXED_INT16,			// 16-bit integer
	WIRE_FIXED_INT32,			// 32-bit integer
	WIRE_FIXED_INT64,			// 64-bit integer

	WIRE_FLOAT,					// 32-bit floating point value
	WIRE_HALF_FLOAT,				// 16-bit floating point value

	WIRE_ANGLE,					// 32-bit float angle value, normalized to [0..360], transmitted at half-precision

	WIRE_BASE128,				// base-128 encoded unsigned integer
	WIRE_UBASE128				// base-128 encoded signed integer
} wireType_t;

//==============================================

typedef struct entity_state_s {
	int number;                         // edict index

	unsigned int svflags;

	int type;                           // ET_GENERIC, ET_BEAM, etc

	// for client side prediction, 8*(bits 0-4) is x/y radius
	// 8*(bits 5-9) is z down distance, 8(bits10-15) is z up
	// GClip_LinkEntity sets this properly
	int solid;

	vec3_t origin;
	vec3_t angles;
	vec3_t origin2;                 // ET_BEAM, ET_EVENT specific

	unsigned int modelindex;
	unsigned int modelindex2;

	int bodyOwner;                  // ET_PLAYER specific, for dead bodies
	int channel;                    // ET_SOUNDEVENT

	int ownerNum;                   // ET_EVENT specific

	unsigned int effects;

	// impulse events -- muzzle flashes, footsteps, etc
	// events only go out for a single frame, they
	// are automatically cleared each frame
	int events[2];
	int eventParms[2];

	int counterNum;                 // ET_GENERIC
	int itemNum;                    // for ET_ITEM
	int damage;                     // EV_BLOOD
	int targetNum;                  // ET_EVENT specific
	int colorRGBA;                  // ET_BEAM, ET_EVENT specific
	int radius;                     // ET_SPRITE/ET_DECAL radius, ET_GLADIATOR always extended, ET_HUD type, ...

	bool linearMovement;
	vec3_t linearMovementVelocity;      // this is transmitted instead of origin when linearProjectile is true
	vec3_t linearMovementEnd;           // the end movement point for brush models
	vec3_t linearMovementBegin;			// the starting movement point for brush models
	unsigned int linearMovementDuration;
	int64_t linearMovementTimeStamp;

	float attenuation;                  // should be <= 255/16.0 as this is sent as byte

	// server will use this for sound culling in case
	// the entity has an event attached to it (along with
	// PVS culling)

	int weapon;                         // WEAP_ for players
	bool teleported;

	int sound;                          // for looping sounds, to guarantee shutoff

	int light;							// constant light glow

	int team;                           // team in the game
} entity_state_t;

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

//==============================================

#define MAX_GAME_STATS  64

typedef struct {
	int64_t stats[MAX_GAME_STATS];
} game_state_t;

//==============================================

#define MAX_PARSE_GAMECOMMANDS  256

typedef struct {
	bool all;
	uint8_t targets[MAX_CLIENTS / 8];
	size_t commandOffset;           // offset of the data in gamecommandsData
} gcommand_t;

//==============================================

// player_state_t is the information needed in addition to pmove_state_t
// to rendered a view.  There will only be 10 player_state_t sent each second,
// but the number of pmove_state_t changes will be relative to client
// frame rates
#define PS_MAX_STATS            64

typedef struct {
	pmove_state_t pmove;        // for prediction

	// these fields do not need to be communicated bit-precise

	vec3_t viewangles;          // for fixed views

	int event[2], eventParm[2];
	unsigned int POVnum;        // entity number of the player in POV
	unsigned int playerNum;     // client number
	float viewheight;
	float fov;                  // horizontal field of view (unused)

	int inventory[MAX_ITEMS];
	short stats[PS_MAX_STATS];  // fast status bar updates
	uint32_t plrkeys;           // infos on the pressed keys of chased player (self if not chasing)
	uint8_t weaponState;
} player_state_t;

typedef struct {
	// state (in / out)
	player_state_t *playerState;

	// command (in)
	usercmd_t cmd;

	// A hint (in)
	bool skipCollision;

	// results (out)
	int numtouch;
	int touchents[MAXTOUCH];
	float step;                 // used for smoothing the player view

	vec3_t mins, maxs;          // bounding box size

	int groundentity;
	cplane_t groundplane;       // valid if groundentity >= 0
	int groundsurfFlags;        // valid if groundentity >= 0
	int groundcontents;         // valid if groundentity >= 0
	int watertype;
	int waterlevel;

	int contentmask;

	bool ladder;
} pmove_t;
