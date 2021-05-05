/*
Copyright (C) 2002-2003 Victor Luchits

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

#include "qcommon/hash.h"
#include "gameshared/q_comref.h"
#include "gameshared/q_collision.h"
#include "gameshared/q_math.h"
#include "gameshared/gs_synctypes.h"
#include "gameshared/gs_pmove.h"

//===============================================================
//		WARSOW player AAboxes sizes

constexpr Vec3 playerbox_stand_mins = Vec3( -16, -16, -24 );
constexpr Vec3 playerbox_stand_maxs = Vec3( 16, 16, 40 );
constexpr int playerbox_stand_viewheight = 30;

constexpr Vec3 playerbox_crouch_mins = Vec3( -16, -16, -24 );
constexpr Vec3 playerbox_crouch_maxs = Vec3( 16, 16, 39 );
constexpr int playerbox_crouch_viewheight = 30;

constexpr Vec3 playerbox_gib_mins = Vec3( -16, -16, 0 );
constexpr Vec3 playerbox_gib_maxs = Vec3( 16, 16, 16 );
constexpr int playerbox_gib_viewheight = 8;

#define BASEGRAVITY 800
#define GRAVITY 850

#define ZOOMTIME 60
#define PROJECTILE_PRESTEP 100

//==================================================================

enum {
	GS_MODULE_GAME = 1,
	GS_MODULE_CGAME,
};

#define MAXTOUCH    32

#define GS_ShootingDisabled( gs ) ( ( ( gs )->gameState.flags & GAMESTAT_FLAG_INHIBITSHOOTING ) != 0 )
#define GS_HasChallengers( gs ) ( ( ( gs )->gameState.flags & GAMESTAT_FLAG_HASCHALLENGERS ) != 0 )
#define GS_TeamBasedGametype( gs ) ( ( ( gs )->gameState.flags & GAMESTAT_FLAG_ISTEAMBASED ) != 0 )
#define GS_RaceGametype( gs ) ( ( ( gs )->gameState.flags & GAMESTAT_FLAG_ISRACE ) != 0 )
#define GS_MatchPaused( gs ) ( ( ( gs )->gameState.flags & GAMESTAT_FLAG_PAUSED ) != 0 )
#define GS_MatchWaiting( gs ) ( ( ( gs )->gameState.flags & GAMESTAT_FLAG_WAITING ) != 0 )
#define GS_Countdown( gs ) ( ( ( gs )->gameState.flags & GAMESTAT_FLAG_COUNTDOWN ) != 0 )

#define GS_MatchState( gs ) ( ( gs )->gameState.match_state )
#define GS_MaxPlayersInTeam( gs ) ( ( gs )->gameState.max_team_players )
#define GS_IndividualGameType( gs ) ( GS_MaxPlayersInTeam( gs ) == 1 )

#define GS_MatchDuration( gs ) ( ( gs )->gameState.match_duration )
#define GS_MatchStartTime( gs ) ( ( gs )->gameState.match_start )
#define GS_MatchEndTime( gs ) ( ( gs )->gameState.match_duration ? ( gs )->gameState.match_start + ( gs )->gameState.match_duration : 0 )
#define GS_MatchClockOverride( gs ) ( ( gs )->gameState.clock_override )

//==================================================================

#define ATTN_NONE               0       // full volume the entire level
#define ATTN_DISTANT            0.5     // distant sound (most likely explosions)
#define ATTN_NORM               1       // players, weapons, etc
#define ATTN_IDLE               2.5     // stuff around you
#define ATTN_STATIC             5       // diminish very rapidly with distance

// sound channels
// CHAN_AUTO never willingly overrides
enum {
	CHAN_AUTO,
	CHAN_BODY,

	CHAN_TOTAL,

	CHAN_FIXED = 128
};

//==================================================================

#define ISWALKABLEPLANE( x ) ( ( (cplane_t *)x )->normal.z >= 0.7f )

#define SLIDEMOVE_PLANEINTERACT_EPSILON 0.05
#define SLIDEMOVEFLAG_WALL_BLOCKED  8
#define SLIDEMOVEFLAG_TRAPPED       4
#define SLIDEMOVEFLAG_BLOCKED       2   // it was blocked at some point, doesn't mean it didn't slide along the blocking object

Vec3 GS_ClipVelocity( Vec3 in, Vec3 normal, float overbounce );

int GS_LinearMovement( const SyncEntityState *ent, int64_t time, Vec3 * dest );
void GS_LinearMovementDelta( const SyncEntityState *ent, int64_t oldTime, int64_t curTime, Vec3 * dest );

//==============================================================
//
//PLAYER MOVEMENT CODE
//
//Common between server and client so prediction matches
//
//==============================================================

struct pmove_t {
	// state (in / out)
	SyncPlayerState * playerState;

	// command (in)
	usercmd_t cmd;

	// results (out)
	int numtouch;
	int touchents[ MAXTOUCH ];
	float step;                 // used for smoothing the player view

	Vec3 mins, maxs;          // bounding box size

	int groundentity;
	int watertype;
	int waterlevel;

	int contentmask;

	bool ladder;
};

void Pmove( const gs_state_t *, pmove_t * );

#define STEPSIZE 18

struct gs_module_api_t {
	void ( *Trace )( trace_t *t, Vec3 start, Vec3 mins, Vec3 maxs, Vec3 end, int ignore, int contentmask, int timeDelta );
	SyncEntityState *( *GetEntityState )( int entNum, int deltaTime );
	int ( *PointContents )( Vec3 point, int timeDelta );
	void ( *PredictedEvent )( int entNum, int ev, u64 parm );
	void ( *PredictedFireWeapon )( int entNum, u64 weapon_and_entropy );
	void ( *PMoveTouchTriggers )( pmove_t *pm, Vec3 previous_origin );
};

struct gs_state_t {
	int module;
	int maxclients;
	SyncGameState gameState;
	gs_module_api_t api;
};

//===============================================================

#define HEALTH_TO_INT( x )    ( ( x ) < 1.0f ? (int)ceilf( ( x ) ) : (int)floorf( ( x ) + 0.5f ) )

// gs_items - shared items definitions

struct Item {
	ItemType type;

	const char * name;
	const char * short_name;
	RGB8 color;
	const char * description;
	int cost;
};

enum {
	TEAM_SPECTATOR,
	TEAM_PLAYERS,
	TEAM_ALPHA,
	TEAM_BETA,

	GS_MAX_TEAMS,

	TEAM_ALLY,
	TEAM_ENEMY,
};

// teams
const char *GS_TeamName( int team );
int GS_TeamFromName( const char *teamname );

//===============================================================

// gs_misc.c
Vec3 GS_EvaluateJumppad( const SyncEntityState * jumppad, Vec3 velocity );
void GS_TouchPushTrigger( const gs_state_t * gs, SyncPlayerState * playerState, const SyncEntityState * pusher );
int GS_WaterLevel( const gs_state_t * gs, SyncEntityState *state, Vec3 mins, Vec3 maxs );

//===============================================================

#define ISGAMETYPESTAT( x ) ( ( x >= GS_GAMETYPE_STATS_START ) && ( x < GS_GAMETYPE_STATS_END ) )

static constexpr const char *gs_keyicon_names[] = {
	"forward",
	"backward",
	"left",
	"right",
	"fire",
	"jump",
	"crouch",
	"special"
};

enum MeanOfDeath {
	// implicit WeaponType enum at the start

	MeanOfDeath_Slime = Weapon_Count,
	MeanOfDeath_Lava,
	MeanOfDeath_Crush, // moving item blocked by player
	MeanOfDeath_Telefrag,
	MeanOfDeath_Suicide,
	MeanOfDeath_Explosion,

	MeanOfDeath_Trigger,

	MeanOfDeath_Laser,
	MeanOfDeath_Spike,
	MeanOfDeath_Void,
};

//
// events, event parms
//

enum {
	PAIN_20,
	PAIN_35,
	PAIN_60,
	PAIN_100,

	PAIN_TOTAL
};

// vsay tokens list
enum {
	Vsay_Sorry,
	Vsay_Thanks,
	Vsay_GoodGame,
	Vsay_ShutUp,
	Vsay_BoomStick,

	Vsay_Bruh,
	Vsay_Cya,
	Vsay_GetGood,
	Vsay_HitTheShowers,
	Vsay_Lads,
	Vsay_SheDoesntEvenGoHere,
	Vsay_ShitSon,
	Vsay_TrashSmash,
	Vsay_WhatTheShit,
	Vsay_WowYourTerrible,
	Vsay_Acne,
	Vsay_Valley,
	Vsay_Mike,
	Vsay_User,
	Vsay_Guyman,
	Vsay_Helena,

	Vsay_Total
};

// SyncEntityState->event values
#define PREDICTABLE_EVENTS_MAX 32
enum EventType {
	EV_NONE,

	// predictable events

	EV_WEAPONACTIVATE,
	EV_FIREWEAPON,
	EV_SMOOTHREFIREWEAPON,
	EV_NOAMMOCLICK,
	EV_ZOOM_IN,
	EV_ZOOM_OUT,

	EV_DASH,

	EV_WALLJUMP,
	EV_JUMP,
	EV_JUMP_PAD,
	EV_FALL,

	// non predictable events

	EV_WEAPONDROP = PREDICTABLE_EVENTS_MAX,

	EV_PAIN,
	EV_DIE,

	EV_PLAYER_RESPAWN,
	EV_PLAYER_TELEPORT_IN,
	EV_PLAYER_TELEPORT_OUT,

	EV_BLOOD,
	EV_GIB,

	EV_BLADE_IMPACT,
	EV_GRENADE_BOUNCE,
	EV_GRENADE_EXPLOSION,
	EV_ROCKET_EXPLOSION,
	EV_PLASMA_EXPLOSION,
	EV_BUBBLE_EXPLOSION,
	EV_BOLT_EXPLOSION,
	EV_RIFLEBULLET_IMPACT,
	EV_STAKE_IMPALE,
	EV_STAKE_IMPACT,
	EV_BLAST_BOUNCE,
	EV_BLAST_IMPACT,

	EV_EXPLOSION1,
	EV_EXPLOSION2,

	EV_SPARKS,

	EV_VSAY,

	EV_TBAG,
	EV_SPRAY,

	EV_LASER_SPARKS,

	// func movers
	EV_PLAT_HIT_TOP,
	EV_PLAT_HIT_BOTTOM,
	EV_PLAT_START_MOVING,

	EV_DOOR_HIT_TOP,
	EV_DOOR_HIT_BOTTOM,
	EV_DOOR_START_MOVING,

	EV_BUTTON_FIRE,

	EV_TRAIN_STOP,
	EV_TRAIN_START,

	EV_DAMAGE,
	EV_HEADSHOT,

	EV_VFX,

	MAX_EVENTS = 128
};

enum playerstate_event_t {
	PSEV_NONE = 0,
	PSEV_HIT,
	PSEV_DAMAGE_10,
	PSEV_DAMAGE_20,
	PSEV_DAMAGE_30,
	PSEV_DAMAGE_40,
	PSEV_ANNOUNCER,
	PSEV_ANNOUNCER_QUEUED,

	PSEV_MAX_EVENTS = 0xFF
};

//===============================================================

#define EVENT_ENTITIES_START    96 // entity types above this index will get event treatment

// SyncEntityState->type values
enum EntityType {
	ET_GENERIC,
	ET_PLAYER,
	ET_CORPSE,
	ET_GHOST,
	ET_JUMPPAD,
	ET_PAINKILLER_JUMPPAD,

	ET_ROCKET,      // redlight + trail
	ET_GRENADE,
	ET_PLASMA,
	ET_BUBBLE,
	ET_RIFLEBULLET,
	ET_STAKE,
	ET_BLAST,

	ET_LASERBEAM,   // for continuous beams

	ET_DECAL,

	ET_BOMB,
	ET_BOMB_SITE,

	ET_LASER,
	ET_SPIKES,
	ET_SPEAKER,

	// eventual entities: types below this will get event treatment
	ET_EVENT = EVENT_ENTITIES_START,
	ET_SOUNDEVENT,

	ET_TOTAL_TYPES // current count
};

// SyncEntityState->effects
#define EF_CARRIER                  ( 1 << 0 )
#define EF_TAKEDAMAGE               ( 1 << 1 )
#define EF_GODMODE                  ( 1 << 2 )
#define EF_HAT                      ( 1 << 3 )
#define EF_TEAM_SILHOUETTE          ( 1 << 4 )
#define EF_WORLD_MODEL              ( 1 << 5 )
