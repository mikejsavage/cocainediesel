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

constexpr MinMax3 playerbox_stand = MinMax3( Vec3( -16, -16, -24 ), Vec3( 16, 16, 40 ) );
constexpr int playerbox_stand_viewheight = 30;

constexpr MinMax3 playerbox_gib = MinMax3( Vec3( -16, -16, 0 ), Vec3( 16, 16, 16 ) );
constexpr int playerbox_gib_viewheight = 8;

constexpr float GRAVITY = 850.0f;

constexpr int PLAYER_MASS = 200;

#define ZOOMTIME 60
#define PROJECTILE_PRESTEP 100

//==================================================================

enum {
	GS_MODULE_GAME = 1,
	GS_MODULE_CGAME,
};

#define MAXTOUCH    32

struct pmove_t {
	// state (in / out)
	SyncPlayerState * playerState;

	// command (in)
	UserCommand cmd;
	Vec3 scale;
	Team team;

	// results (out)
	int numtouch;
	int touchents[MAXTOUCH];
	float step;                 // used for smoothing the player view

	MinMax3 bounds;

	int groundentity;
	SolidBits solid_mask;
};

struct gs_module_api_t {
	trace_t ( *Trace )( Vec3 start, MinMax3 bounds, Vec3 end, int ignore, SolidBits solid_mask, int timeDelta );
	void ( *PredictedEvent )( int entNum, int ev, u64 parm );
	void ( *PredictedFireWeapon )( int entNum, u64 parm );
	void ( *PredictedAltFireWeapon )( int entNum, u64 parm );
	void ( *PredictedUseGadget )( int entNum, GadgetType gadget, u64 parm, bool dead );
	void ( *PMoveTouchTriggers )( const pmove_t * pm, Vec3 previous_origin );
};

struct gs_state_t {
	int module;
	int maxclients;
	SyncGameState gameState;
	gs_module_api_t api;
};

//==================================================================

#define ATTN_NONE               0.0f    // full volume the entire level
#define ATTN_DISTANT            0.5f    // distant sound (most likely explosions)
#define ATTN_NORM               1.0f    // players, weapons, etc
#define ATTN_IDLE               2.5f    // stuff around you
#define ATTN_STATIC             5.0f    // diminish very rapidly with distance

//==================================================================

constexpr bool ISWALKABLEPLANE( Vec3 normal ) { return normal.z >= 0.7f; }

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

#define STEPSIZE 18

void Pmove( const gs_state_t * gs, pmove_t *pmove );

//===============================================================

#define HEALTH_TO_INT( x )    ( ( x ) < 1.0f ? (int)ceilf( ( x ) ) : (int)floorf( ( x ) + 0.5f ) )

// teams
const char * GS_TeamName( Team team );
Team GS_TeamFromName( const char * name );

//===============================================================

// gs_misc.c
Vec3 GS_EvaluateJumppad( const SyncEntityState * jumppad, Vec3 velocity );
void GS_TouchPushTrigger( const gs_state_t * gs, SyncPlayerState * playerState, const SyncEntityState * pusher );

//===============================================================

// pmove->pm_features
#define PMFEAT_ABILITIES        ( 1 << 0 )
#define PMFEAT_SCOPE            ( 1 << 1 )

#define PMFEAT_ALL              ( 0xFFFF )

DamageCategory DecodeDamageType( DamageType type, WeaponType * weapon, GadgetType * gadget, WorldDamage * world );

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

// SyncEntityState->event values
#define PREDICTABLE_EVENTS_MAX 32
enum EventType {
	EV_NONE,

	// predictable events

	EV_WEAPONACTIVATE,
	EV_FIREWEAPON,
	EV_ALTFIREWEAPON,
	EV_USEGADGET,
	EV_SMOOTHREFIREWEAPON,
	EV_NOAMMOCLICK,
	EV_RELOAD,
	EV_ZOOM_IN,
	EV_ZOOM_OUT,

	EV_DASH,

	EV_WALLJUMP,
	EV_CHARGEJUMP,
	EV_JETPACK,
	EV_JUMP,
	EV_JUMP_PAD,
	EV_FALL,

	EV_MARTYR_EXPLODE,

	// non predictable events

	EV_WEAPONDROP = PREDICTABLE_EVENTS_MAX,

	EV_PAIN,
	EV_DIE,
	EV_RESPAWN,

	EV_BLOOD,
	EV_GIB,

	EV_FX,

	EV_LAUNCHER_BOUNCE,
	EV_RAIL_ALTENT,
	EV_RAIL_ALTFIRE,
	EV_RAIL_EXPLOSION,
	EV_STICKY_EXPLOSION,

	EV_BOMB_EXPLOSION,

	EV_VSAY,

	EV_SPRAY,

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

	EV_VFX,
	EV_SOUND_ORIGIN,
	EV_SOUND_ENT,

	EV_FLASH_WINDOW,

	MAX_EVENTS = 128
};

enum JumpType : u8 {
	JumpType_Normal,
	JumpType_WheelDash,
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

// SyncEntityState->effects
#define EF_CARRIER                  ( 1 << 0 )
#define EF_HAT                      ( 1 << 1 )
#define EF_TEAM_SILHOUETTE          ( 1 << 2 )
