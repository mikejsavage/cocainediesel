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
#include "gameshared/gs_qrespath.h"
#include "gameshared/q_comref.h"
#include "gameshared/q_collision.h"
#include "gameshared/q_math.h"

//===============================================================
//		WARSOW player AAboxes sizes

constexpr Vec3 playerbox_stand_mins = Vec3( -16, -16, -24 );
constexpr Vec3 playerbox_stand_maxs = Vec3( 16, 16, 40 );
constexpr int playerbox_stand_viewheight = 30;

// TODO: crouch temp disabled
// constexpr Vec3 playerbox_crouch_mins = Vec3( -16, -16, -24 );
// constexpr Vec3 playerbox_crouch_maxs = Vec3( 16, 16, 16 );
// constexpr int playerbox_crouch_viewheight = 12;

constexpr Vec3 playerbox_crouch_mins = Vec3( -16, -16, -24 );
constexpr Vec3 playerbox_crouch_maxs = Vec3( 16, 16, 39 );
constexpr int playerbox_crouch_viewheight = 30;

constexpr Vec3 playerbox_gib_mins = Vec3( -16, -16, 0 );
constexpr Vec3 playerbox_gib_maxs = Vec3( 16, 16, 16 );
constexpr int playerbox_gib_viewheight = 8;

#define BASEGRAVITY 800
#define GRAVITY 850
#define GRAVITY_COMPENSATE ( (float)GRAVITY / (float)BASEGRAVITY )

#define ZOOMTIME 60
#define CROUCHTIME 100
#define DEFAULT_PLAYERSPEED 320.0f
#define DEFAULT_JUMPSPEED 260.0f
#define DEFAULT_DASHSPEED 550.0f
#define PROJECTILE_PRESTEP 100
#define HITSCAN_RANGE 9001

//==================================================================

enum MatchState {
	MATCH_STATE_NONE,
	MATCH_STATE_WARMUP,
	MATCH_STATE_COUNTDOWN,
	MATCH_STATE_PLAYTIME,
	MATCH_STATE_POSTMATCH,
	MATCH_STATE_WAITEXIT,

	MATCH_STATE_TOTAL
};

typedef u8 WeaponType;
enum WeaponType_ : WeaponType {
	Weapon_None,

	Weapon_Knife,
	Weapon_Pistol,
	Weapon_MachineGun,
	Weapon_Deagle,
	Weapon_Shotgun,
	Weapon_AssaultRifle,
	Weapon_GrenadeLauncher,
	Weapon_RocketLauncher,
	Weapon_Plasma,
	Weapon_BubbleGun,
	Weapon_Laser,
	Weapon_Railgun,
	Weapon_Sniper,
	Weapon_Rifle,

	Weapon_Count
};

typedef u8 WeaponState;
enum WeaponState_ : WeaponState {
	WeaponState_Ready,
	WeaponState_SwitchingIn,
	WeaponState_SwitchingOut,
	WeaponState_Firing,
	WeaponState_FiringSemiAuto,
	WeaponState_Reloading,
};

enum FiringMode {
	FiringMode_Auto,
	FiringMode_Smooth,
	FiringMode_SemiAuto,
};

enum ItemType {
	Item_Bomb,
	Item_FakeBomb,

	Item_Count
};

typedef u8 RoundType;
enum RoundType_ {
	RoundType_Normal,
	RoundType_MatchPoint,
	RoundType_Overtime,
	RoundType_OvertimeMatchPoint,
};

enum BombDown {
	BombDown_Dropped,
	BombDown_Planting,
};

enum BombProgress {
	BombProgress_Nothing,
	BombProgress_Planting,
	BombProgress_Defusing,
};

enum {
	GS_MODULE_GAME = 1,
	GS_MODULE_CGAME,
};

#define GAMESTAT_FLAG_PAUSED ( 1 << 0LL )
#define GAMESTAT_FLAG_WAITING ( 1 << 1LL )
#define GAMESTAT_FLAG_HASCHALLENGERS ( 1 << 2LL )
#define GAMESTAT_FLAG_INHIBITSHOOTING ( 1 << 3LL )
#define GAMESTAT_FLAG_ISTEAMBASED ( 1 << 4LL )
#define GAMESTAT_FLAG_ISRACE ( 1 << 5LL )
#define GAMESTAT_FLAG_COUNTDOWN ( 1 << 6LL )
#define GAMESTAT_FLAG_SELFDAMAGE ( 1 << 7LL )

struct SyncBombGameState {
	u8 alpha_score;
	u8 beta_score;
	u8 alpha_players_alive;
	u8 alpha_players_total;
	u8 beta_players_alive;
	u8 beta_players_total;
};

struct SyncGameState {
	u16 flags;
	int match_state;
	int64_t match_start;
	int64_t match_duration;
	int64_t clock_override;
	RoundType round_type;
	u8 max_team_players;

	StringHash map;
	u32 map_checksum;

	SyncBombGameState bomb;
};

struct SyncEvent {
	u64 parm;
	s8 type;
};

struct SyncEntityState {
	int number;                         // edict index

	unsigned int svflags;

	int type;                           // ET_GENERIC, ET_BEAM, etc

	// for client side prediction, 8*(bits 0-4) is x/y radius
	// 8*(bits 5-9) is z down distance, 8(bits10-15) is z up
	// GClip_LinkEntity sets this properly
	int solid;

	Vec3 origin;
	Vec3 angles;
	Vec3 origin2; // velocity for players/corpses. often used for endpoints, e.g. ET_BEAM and some events

	StringHash model;
	StringHash model2;

	StringHash material;
	RGBA8 color;

	int channel;                    // ET_SOUNDEVENT

	int ownerNum;                   // ET_EVENT specific

	unsigned int effects;

	// impulse events -- muzzle flashes, footsteps, etc
	// events only go out for a single frame, they
	// are automatically cleared each frame
	SyncEvent events[ 2 ];

	int counterNum;                 // ET_GENERIC
	int targetNum;                  // ET_EVENT specific
	RGBA8 silhouetteColor;
	int radius;                     // ET_GLADIATOR always extended, ET_BOMB state, EV_BLOOD damage, ...

	bool linearMovement;
	Vec3 linearMovementVelocity;      // this is transmitted instead of origin when linearProjectile is true
	Vec3 linearMovementEnd;           // the end movement point for brush models
	Vec3 linearMovementBegin;			// the starting movement point for brush models
	unsigned int linearMovementDuration;
	int64_t linearMovementTimeStamp;
	int linearMovementTimeDelta;

	WeaponType weapon;                  // WEAP_ for players
	bool teleported;

	StringHash sound;                          // for looping sounds, to guarantee shutoff

	int team;                           // team in the game
};

// SyncPlayerState is the information needed in addition to pmove_state_t
// to rendered a view.  There will only be 10 SyncPlayerState sent each second,
// but the number of pmove_state_t changes will be relative to client
// frame rates
struct pmove_state_t {
	int pm_type;

	Vec3 origin;
	Vec3 velocity;
	short delta_angles[3];      // add to command angles to get view direction
	                            // changed by spawns, rotating objects, and teleporters

	int pm_flags;               // ducked, jump_held, etc
	int pm_time;

	u16 features;

	s16 no_control_time;
	s16 knockback_time;
	s16 crouch_time;
	s16 tbag_time;
	s16 dash_time;
	s16 walljump_time;

	s16 max_speed;
	s16 jump_speed;
	s16 dash_speed;
};

struct SyncPlayerState {
	pmove_state_t pmove;        // for prediction

	// these fields do not need to be communicated bit-precise

	Vec3 viewangles;          // for fixed views

	SyncEvent events[ 2 ];
	unsigned int POVnum;        // entity number of the player in POV
	unsigned int playerNum;     // client number
	float viewheight;
	float fov;                  // horizontal field of view (unused)

	// BitArray items;
	//
	// struct GrenadeInfo { short count; };
	// struct RechargableCloakingDeviceInfo { float energy; };
	//
	// WeaponInfo weapons[ Weapon_Count ];

	struct WeaponInfo {
		WeaponType weapon;
		int ammo;
	};

	WeaponInfo weapons[ Weapon_Count - 1 ];
	bool items[ Item_Count ];

	uint32_t plrkeys;           // infos on the pressed keys of chased player (self if not chasing)

	bool show_scoreboard;
	bool ready;
	bool voted;
	bool can_change_loadout;
	bool carrying_bomb;
	bool can_plant;

	s16 health;

	WeaponState weapon_state;
	WeaponType weapon;
	WeaponType pending_weapon;
	s16 weapon_time;
	s16 zoom_time;

	int team;
	int real_team;

	u8 progress_type; // enum BombProgress
	u8 progress;

	int pointed_player;
	int pointed_health;
};

// usercmd_t is sent to the server each client frame
struct usercmd_t {
	u8 msec;
	u32 buttons;
	s64 serverTimeStamp;
	s16 angles[3];
	s8 forwardmove, sidemove, upmove;
	WeaponType weaponSwitch;
};

#define MAXTOUCH    32

struct pmove_t {
	// state (in / out)
	SyncPlayerState *playerState;

	// command (in)
	usercmd_t cmd;

	// results (out)
	int numtouch;
	int touchents[MAXTOUCH];
	float step;                 // used for smoothing the player view

	Vec3 mins, maxs;          // bounding box size

	int groundentity;
	int watertype;
	int waterlevel;

	int contentmask;

	bool ladder;
};

struct gs_module_api_t {
	void ( *Trace )( trace_t *t, Vec3 start, Vec3 mins, Vec3 maxs, Vec3 end, int ignore, int contentmask, int timeDelta );
	SyncEntityState *( *GetEntityState )( int entNum, int deltaTime );
	int ( *PointContents )( Vec3 point, int timeDelta );
	void ( *PredictedEvent )( int entNum, int ev, u64 parm );
	void ( *PredictedFireWeapon )( int entNum, WeaponType weapon );
	void ( *PMoveTouchTriggers )( pmove_t *pm, Vec3 previous_origin );
};

struct gs_state_t {
	int module;
	int maxclients;
	SyncGameState gameState;
	gs_module_api_t api;
};

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

#define STEPSIZE 18

void Pmove( const gs_state_t * gs, pmove_t *pmove );

//===============================================================

#define HEALTH_TO_INT( x )    ( ( x ) < 1.0f ? (int)ceilf( ( x ) ) : (int)floorf( ( x ) + 0.5f ) )

// gs_items - shared items definitions

//==================
//	ITEM TAGS
//==================

struct Item {
	ItemType type;

	const char * name;
	const char * short_name;
	RGB8 color;
	const char * description;
	int cost;
};

//===================
//	GAMETYPES
//===================

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
void GS_Obituary( void *victim, void *attacker, int mod, char *message, char *message2 );
Vec3 GS_EvaluateJumppad( const SyncEntityState * jumppad, Vec3 velocity );
void GS_TouchPushTrigger( const gs_state_t * gs, SyncPlayerState * playerState, const SyncEntityState * pusher );
int GS_WaterLevel( const gs_state_t * gs, SyncEntityState *state, Vec3 mins, Vec3 maxs );

//===============================================================

// pmove->pm_features
#define PMFEAT_CROUCH           ( 1 << 0 )
#define PMFEAT_WALK             ( 1 << 1 )
#define PMFEAT_JUMP             ( 1 << 2 )
#define PMFEAT_SPECIAL          ( 1 << 3 )
#define PMFEAT_SCOPE            ( 1 << 4 )
#define PMFEAT_GHOSTMOVE        ( 1 << 5 )
#define PMFEAT_WEAPONSWITCH     ( 1 << 6 )
#define PMFEAT_TEAMGHOST        ( 1 << 7 )

#define PMFEAT_ALL              ( 0xFFFF )
#define PMFEAT_DEFAULT          ( PMFEAT_ALL & ~PMFEAT_GHOSTMOVE & ~PMFEAT_TEAMGHOST )

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

enum MeansOfDeath {
	MOD_UNKNOWN,
	MOD_GUNBLADE,
	MOD_PISTOL,
	MOD_MACHINEGUN,
	MOD_DEAGLE,
	MOD_SHOTGUN,
	MOD_ASSAULTRIFLE,
	MOD_GRENADE,
	MOD_ROCKET,
	MOD_PLASMA,
	MOD_BUBBLEGUN,
	MOD_RAILGUN,
	MOD_LASERGUN,
	MOD_SNIPER,
	MOD_RIFLE,

	MOD_SLIME,
	MOD_LAVA,
	MOD_CRUSH, // moving item blocked by player
	MOD_TELEFRAG,
	MOD_SUICIDE,
	MOD_EXPLOSIVE,

	MOD_TRIGGER_HURT,

	MOD_LASER,
	MOD_SPIKE,
	MOD_VOID,
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

	ET_TOTAL_TYPES, // current count
	MAX_ENTITY_TYPES = 128
};

// SyncEntityState->effects
// Effects are things handled on the client side (lights, particles, frame animations)
// that happen constantly on the given entity.
// An entity that has effects will be sent to the client
// even if it has a zero index model.
#define EF_CARRIER                  ( 1 << 0 )
#define EF_TAKEDAMAGE               ( 1 << 1 )
#define EF_GODMODE                  ( 1 << 2 )
#define EF_HAT                      ( 1 << 3 )
#define EF_WORLD_MODEL              ( 1 << 4 )
#define EF_TEAM_SILHOUETTE          ( 1 << 5 )

//===============================================================
// gs_weapons.c

enum WeaponCategory {
	WeaponCategory_Primary,
	WeaponCategory_Secondary,
	WeaponCategory_Backup,

	WeaponCategory_Count
};

struct WeaponDef {
	const char * name;
	const char * short_name;

	WeaponCategory category;

	int projectile_count;
	int clip_size;
	unsigned int reload_time;
	bool staged_reloading;

	unsigned int weaponup_time;
	unsigned int weapondown_time;
	unsigned int refire_time;
	unsigned int range;
	Vec2 recoil;
	Vec2 recoil_min;
	float recoil_recover;
	FiringMode mode;

	float zoom_fov;
	float zoom_spread;

	float damage;
	float selfdamage;
	int knockback;
	int splash_radius;
	int mindamage;
	int minknockback;

	int speed;
	float spread;
};

const WeaponDef * GS_GetWeaponDef( WeaponType weapon );
SyncPlayerState::WeaponInfo * GS_FindWeapon( SyncPlayerState * player, WeaponType weapon );
WeaponType MODToWeapon( int mod );
WeaponType GS_ThinkPlayerWeapon( const gs_state_t * gs, SyncPlayerState * player, const usercmd_t * cmd, int timeDelta );
void GS_TraceBullet( const gs_state_t * gs, trace_t * trace, trace_t * wallbang_trace, Vec3 start, Vec3 dir, Vec3 right, Vec3 up, float r, float u, int range, int ignore, int timeDelta );
void GS_TraceLaserBeam( const gs_state_t * gs, trace_t * trace, Vec3 origin, Vec3 angles, float range, int ignore, int timeDelta, void ( *impact )( const trace_t * tr, Vec3 dir ) );
bool GS_CanEquip( SyncPlayerState * player, WeaponType weapon );
