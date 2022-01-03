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
#define GRAVITY_COMPENSATE ( (float)GRAVITY / (float)BASEGRAVITY )

constexpr int PLAYER_MASS = 200;

#define ZOOMTIME 60
#define CROUCHTIME 100
#define PROJECTILE_PRESTEP 100

//==================================================================

enum {
	GS_MODULE_GAME = 1,
	GS_MODULE_CGAME,
};

#define MAXTOUCH    32

struct pmove_t {
	// state (in / out)
	SyncPlayerState *playerState;

	// command (in)
	UserCommand cmd;
	Vec3 scale;

	// results (out)
	int numtouch;
	int touchents[MAXTOUCH];
	float step;                 // used for smoothing the player view

	Vec3 mins, maxs;          // bounding box size

	int groundentity;
	int watertype;
	int waterlevel;

	int contentmask;
};

struct gs_module_api_t {
	void ( *Trace )( trace_t *t, Vec3 start, Vec3 mins, Vec3 maxs, Vec3 end, int ignore, int contentmask, int timeDelta );
	SyncEntityState *( *GetEntityState )( int entNum, int deltaTime );
	int ( *PointContents )( Vec3 point, int timeDelta );
	void ( *PredictedEvent )( int entNum, int ev, u64 parm );
	void ( *PredictedFireWeapon )( int entNum, u64 weapon_and_entropy );
	void ( *PredictedUseGadget )( int entNum, GadgetType gadget, u64 parm );
	void ( *PMoveTouchTriggers )( pmove_t *pm, Vec3 previous_origin );
};

struct gs_state_t {
	int module;
	int maxclients;
	SyncGameState gameState;
	gs_module_api_t api;
};

#define GS_TeamBasedGametype( gs ) ( ( ( gs )->gameState.flags & GAMESTAT_FLAG_ISTEAMBASED ) != 0 )
#define GS_MatchPaused( gs ) ( ( ( gs )->gameState.flags & GAMESTAT_FLAG_PAUSED ) != 0 )
#define GS_MatchWaiting( gs ) ( ( ( gs )->gameState.flags & GAMESTAT_FLAG_WAITING ) != 0 )

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

#define ISWALKABLEPLANE( x ) ( ( (Plane *)x )->normal.z >= 0.7f )

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
const char *GS_TeamName( int team );
int GS_TeamFromName( const char *teamname );

//===============================================================

// gs_misc.c
Vec3 GS_EvaluateJumppad( const SyncEntityState * jumppad, Vec3 velocity );
void GS_TouchPushTrigger( const gs_state_t * gs, SyncPlayerState * playerState, const SyncEntityState * pusher );
int GS_WaterLevel( const gs_state_t * gs, SyncEntityState *state, Vec3 mins, Vec3 maxs );

//===============================================================

// pmove->pm_features
#define PMFEAT_CROUCH           ( 1 << 0 )
#define PMFEAT_JUMP             ( 1 << 1 )
#define PMFEAT_SPECIAL          ( 1 << 2 )
#define PMFEAT_SCOPE            ( 1 << 3 )
#define PMFEAT_GHOSTMOVE        ( 1 << 4 )
#define PMFEAT_WEAPONSWITCH     ( 1 << 5 )
#define PMFEAT_TEAMGHOST        ( 1 << 6 )

#define PMFEAT_ALL              ( 0xFFFF )
#define PMFEAT_DEFAULT          ( PMFEAT_ALL & ~PMFEAT_GHOSTMOVE & ~PMFEAT_TEAMGHOST )

enum DamageCategory {
	DamageCategory_Weapon,
	DamageCategory_Gadget,
	DamageCategory_World,
};

enum WorldDamage : u8 {
	WorldDamage_Slime,
	WorldDamage_Lava,
	WorldDamage_Crush,
	WorldDamage_Telefrag,
	WorldDamage_Suicide,
	WorldDamage_Explosion,

	WorldDamage_Trigger,

	WorldDamage_Laser,
	WorldDamage_Spike,
	WorldDamage_Void,
};

struct DamageType {
	u8 encoded;

	DamageType() = default;
	DamageType( WeaponType weapon );
	DamageType( GadgetType gadget );
	DamageType( WorldDamage world );
};

bool operator==( DamageType a, DamageType b );
bool operator!=( DamageType a, DamageType b );

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

// vsay tokens list
enum {
	Vsay_Sorry,
	Vsay_Thanks,
	Vsay_GoodGame,
	Vsay_BoomStick,

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
	EV_USEGADGET,
	EV_SMOOTHREFIREWEAPON,
	EV_NOAMMOCLICK,
	EV_ZOOM_IN,
	EV_ZOOM_OUT,

	EV_DASH,

	EV_WALLJUMP,
	EV_JUMP,
	EV_JUMP_PAD,
	EV_FALL,

	EV_SUICIDE_BOMB_ANNOUNCEMENT,
	EV_SUICIDE_BOMB_BEEP,
	EV_SUICIDE_BOMB_EXPLODE,

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
	EV_ARBULLET_EXPLOSION,
	EV_BUBBLE_EXPLOSION,
	EV_BOLT_EXPLOSION,
	EV_RIFLEBULLET_IMPACT,
	EV_STAKE_IMPALE,
	EV_STAKE_IMPACT,
	EV_BLAST_BOUNCE,
	EV_BLAST_IMPACT,
	EV_STICKY_EXPLOSION,
	EV_STICKY_IMPACT,

	EV_STUN_GRENADE_EXPLOSION,

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

// SyncEntityState->effects
#define EF_CARRIER                  ( 1 << 0 )
#define EF_HAT                      ( 1 << 1 )
#define EF_TEAM_SILHOUETTE          ( 1 << 2 )
#define EF_WORLD_MODEL              ( 1 << 3 )
