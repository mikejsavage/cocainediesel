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

#include "gameshared/gs_qrespath.h"
#include "gameshared/q_comref.h"
#include "gameshared/q_collision.h"
#include "gameshared/q_math.h"

//===============================================================
//		WARSOW player AAboxes sizes

constexpr vec3_t playerbox_stand_mins = { -16, -16, -24 };
constexpr vec3_t playerbox_stand_maxs = { 16, 16, 40 };
constexpr int playerbox_stand_viewheight = 30;

constexpr vec3_t playerbox_crouch_mins = { -16, -16, -24 };
constexpr vec3_t playerbox_crouch_maxs = { 16, 16, 16 };
constexpr int playerbox_crouch_viewheight = 12;

constexpr vec3_t playerbox_gib_mins = { -16, -16, 0 };
constexpr vec3_t playerbox_gib_maxs = { 16, 16, 16 };
constexpr int playerbox_gib_viewheight = 8;

#define BASEGRAVITY 800
#define GRAVITY 850
#define GRAVITY_COMPENSATE ( (float)GRAVITY / (float)BASEGRAVITY )

#define ZOOMTIME 60
#define CROUCHTIME 100
#define DEFAULT_PLAYERSPEED 320.0f
#define DEFAULT_JUMPSPEED 280.0f
#define DEFAULT_DASHSPEED 450.0f
#define PROJECTILE_PRESTEP 100
#define ELECTROBOLT_RANGE 9001

#define MIN_FOV             60
#define MAX_FOV             140

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
	Weapon_Knife,
	Weapon_MachineGun,
	Weapon_Shotgun,
	Weapon_GrenadeLauncher,
	Weapon_RocketLauncher,
	Weapon_Plasma,
	Weapon_Laser,
	Weapon_Railgun,

	Weapon_Count
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

	SyncBombGameState bomb;
};

struct SyncEntityState {
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
	int damage;                     // EV_BLOOD
	int targetNum;                  // ET_EVENT specific
	int colorRGBA;                  // ET_BEAM, ET_EVENT specific
	RGBA8 silhouetteColor;
	int radius;                     // ET_GLADIATOR always extended, ET_HUD type, ...

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
};

// SyncPlayerState is the information needed in addition to pmove_state_t
// to rendered a view.  There will only be 10 SyncPlayerState sent each second,
// but the number of pmove_state_t changes will be relative to client
// frame rates
#define PS_MAX_STATS 64
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
};

typedef struct {
	int pm_type;

	float origin[3];
	float velocity[3];

	int pm_flags;               // ducked, jump_held, etc
	int pm_time;                // each unit = 8 ms
	short stats[MAX_PM_STATS];  // Kurim : timers for knockback, doublejump, walljump
	int gravity;
	short delta_angles[3];      // add to command angles to get view direction
	                            // changed by spawns, rotating objects, and teleporters
} pmove_state_t;

struct SyncPlayerState {
	pmove_state_t pmove;        // for prediction

	// these fields do not need to be communicated bit-precise

	vec3_t viewangles;          // for fixed views

	int event[2], eventParm[2];
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
		bool owned;
		int ammo;
	};

	WeaponInfo weapons[ Weapon_Count ];
	bool items[ Item_Count ];

	uint32_t plrkeys;           // infos on the pressed keys of chased player (self if not chasing)

	bool show_scoreboard;
	bool ready;
	bool voted;
	bool can_change_loadout;
	bool carrying_bomb;
	bool can_plant;

	s16 health;

	uint8_t weapon_state;
	WeaponType weapon;
	WeaponType pending_weapon;
	s16 weapon_time;

	int team;
	int real_team;

	u8 progress_type; // enum ProgressType
	s16 progress;

	int last_killer;
	int pointed_player;
	int pointed_health;
};

// usercmd_t is sent to the server each client frame
typedef struct usercmd_s {
	uint8_t msec;
	uint32_t buttons;
	int64_t serverTimeStamp;
	int16_t angles[3];
	int8_t forwardmove, sidemove, upmove;
} usercmd_t;

#define MAXTOUCH    32

typedef struct {
	// state (in / out)
	SyncPlayerState *playerState;

	// command (in)
	usercmd_t cmd;

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

typedef struct {
	void ( *Trace )( trace_t *t, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int ignore, int contentmask, int timeDelta );
	SyncEntityState *( *GetEntityState )( int entNum, int deltaTime );
	int ( *PointContents )( const vec3_t point, int timeDelta );
	void ( *PredictedEvent )( int entNum, int ev, int parm );
	void ( *PMoveTouchTriggers )( pmove_t *pm, vec3_t previous_origin );
	const char *( *GetConfigString )( int index );
} gs_module_api_t;

typedef struct {
	int module;
	int maxclients;
	SyncGameState gameState;
	gs_module_api_t api;
} gs_state_t;

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

#define ISWALKABLEPLANE( x ) ( ( (cplane_t *)x )->normal[2] >= 0.7 )

// box slide movement code (not used for player)
#define MAX_SLIDEMOVE_CLIP_PLANES   16

#define SLIDEMOVE_PLANEINTERACT_EPSILON 0.05
#define SLIDEMOVEFLAG_PLANE_TOUCHED 16
#define SLIDEMOVEFLAG_WALL_BLOCKED  8
#define SLIDEMOVEFLAG_TRAPPED       4
#define SLIDEMOVEFLAG_BLOCKED       2   // it was blocked at some point, doesn't mean it didn't slide along the blocking object
#define SLIDEMOVEFLAG_MOVED     1

typedef struct {
	vec3_t velocity;
	vec3_t origin;
	vec3_t mins, maxs;
	float remainingTime;

	vec3_t gravityDir;
	float slideBounce;
	int groundEntity;

	int passent, contentmask;

	int numClipPlanes;
	vec3_t clipPlaneNormals[MAX_SLIDEMOVE_CLIP_PLANES];

	int numtouch;
	int touchents[MAXTOUCH];
} move_t;

int GS_SlideMove( const gs_state_t * gs, move_t *move );
void GS_ClipVelocity( vec3_t in, vec3_t normal, vec3_t out, float overbounce );

int GS_LinearMovement( const SyncEntityState *ent, int64_t time, vec3_t dest );
void GS_LinearMovementDelta( const SyncEntityState *ent, int64_t oldTime, int64_t curTime, vec3_t dest );

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

const Item *GS_FindItemByName( const char *name );
const Item *GS_Cmd_UseItem( const gs_state_t * gs, SyncPlayerState *playerState, const char *string, int typeMask );
const Item *GS_Cmd_NextWeapon_f( const gs_state_t * gs, SyncPlayerState *playerState, int predictedWeaponSwitch );
const Item *GS_Cmd_PrevWeapon_f( const gs_state_t * gs, SyncPlayerState *playerState, int predictedWeaponSwitch );

const Item * GS_FindItemByType( ItemType type );
const Item * GS_FindItemByName( const char * name );
bool GS_CanEquip( const SyncPlayerState * player, WeaponType weapon );

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

struct TeamColor {
	const char * name;
	RGB8 rgb;
};

constexpr TeamColor TEAM_COLORS[] = {
	{ "Blue", RGB8( 0, 160, 255 ) },
	{ "Red", RGB8( 255, 20, 60 ) },
	{ "Green", RGB8( 82, 252, 95 ) },
	{ "Yellow", RGB8( 255, 199, 30 ) },
};

// teams
const char *GS_TeamName( int team );
int GS_TeamFromName( const char *teamname );

//===============================================================

// gs_misc.c
void GS_Obituary( void *victim, void *attacker, int mod, char *message, char *message2 );
void GS_TouchPushTrigger( const gs_state_t * gs, SyncPlayerState *playerState, SyncEntityState *pusher );
int GS_WaterLevel( const gs_state_t * gs, SyncEntityState *state, vec3_t mins, vec3_t maxs );

//===============================================================

// pmove->pm_features
#define PMFEAT_CROUCH           ( 1 << 0 )
#define PMFEAT_WALK             ( 1 << 1 )
#define PMFEAT_JUMP             ( 1 << 2 )
#define PMFEAT_DASH             ( 1 << 3 )
#define PMFEAT_WALLJUMP         ( 1 << 4 )
#define PMFEAT_ZOOM             ( 1 << 5 )
#define PMFEAT_GHOSTMOVE        ( 1 << 6 )
#define PMFEAT_WEAPONSWITCH     ( 1 << 7 )
#define PMFEAT_TEAMGHOST        ( 1 << 8 )

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

//===============================================================

// means of death
#define MOD_UNKNOWN 0

typedef enum {
	MOD_GUNBLADE = 36,
	MOD_MACHINEGUN,
	MOD_RIOTGUN,
	MOD_GRENADE,
	MOD_ROCKET,
	MOD_PLASMA,
	MOD_ELECTROBOLT,
	MOD_LASERGUN,
	MOD_GRENADE_SPLASH,
	MOD_ROCKET_SPLASH,
	MOD_PLASMA_SPLASH,

	MOD_SLIME,
	MOD_LAVA,
	MOD_CRUSH, // moving item blocked by player
	MOD_TELEFRAG,
	MOD_SUICIDE,
	MOD_EXPLOSIVE,

	MOD_TRIGGER_HURT,

	MOD_LASER,
	MOD_SPIKES,
} mod_damage_t;

//===============================================================

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
	VSAY_GENERIC,
	VSAY_AFFIRMATIVE,
	VSAY_NEGATIVE,
	VSAY_YES,
	VSAY_NO,
	VSAY_ONDEFENSE,
	VSAY_ONOFFENSE,
	VSAY_OOPS,
	VSAY_SORRY,
	VSAY_THANKS,
	VSAY_NOPROBLEM,
	VSAY_YEEHAA,
	VSAY_GOODGAME,
	VSAY_DEFEND,
	VSAY_ATTACK,
	VSAY_NEEDBACKUP,
	VSAY_BOOO,
	VSAY_NEEDDEFENSE,
	VSAY_NEEDOFFENSE,
	VSAY_NEEDHELP,
	VSAY_ROGER,
	VSAY_AREASECURED,
	VSAY_SHUTUP,
	VSAY_BOOMSTICK,
	VSAY_OK,

	VSAY_TOTAL = 128
};

// SyncEntityState->event values
#define PREDICTABLE_EVENTS_MAX 32
typedef enum {
	EV_NONE,

	// predictable events

	EV_WEAPONACTIVATE,
	EV_FIREWEAPON,
	EV_ELECTROTRAIL,
	EV_FIRE_RIOTGUN,
	EV_FIRE_MG,
	EV_SMOOTHREFIREWEAPON,
	EV_NOAMMOCLICK,

	EV_DASH,

	EV_WALLJUMP,
	EV_DOUBLEJUMP,
	EV_JUMP,
	EV_JUMP_PAD,
	EV_FALL,

	// non predictable events

	EV_WEAPONDROP = PREDICTABLE_EVENTS_MAX,

	EV_ITEM_RESPAWN,
	EV_PAIN,
	EV_DIE,

	EV_PLAYER_RESPAWN,
	EV_PLAYER_TELEPORT_IN,
	EV_PLAYER_TELEPORT_OUT,

	EV_SPOG,

	EV_BLOOD,

	EV_BLADE_IMPACT,
	EV_GRENADE_BOUNCE,
	EV_GRENADE_EXPLOSION,
	EV_ROCKET_EXPLOSION,
	EV_PLASMA_EXPLOSION,
	EV_BOLT_EXPLOSION,

	EV_EXPLOSION1,
	EV_EXPLOSION2,

	EV_SPARKS,

	EV_VSAY,

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

	MAX_EVENTS = 128
} entity_event_t;

typedef enum {
	PSEV_NONE = 0,
	PSEV_HIT,
	PSEV_DAMAGE_10,
	PSEV_DAMAGE_20,
	PSEV_DAMAGE_30,
	PSEV_DAMAGE_40,
	PSEV_INDEXEDSOUND,
	PSEV_ANNOUNCER,
	PSEV_ANNOUNCER_QUEUED,

	PSEV_MAX_EVENTS = 0xFF
} playerstate_event_t;

//===============================================================

#define EVENT_ENTITIES_START    96 // entity types above this index will get event treatment

// SyncEntityState->type values
enum {
	ET_GENERIC,
	ET_PLAYER,
	ET_CORPSE,
	ET_PUSH_TRIGGER,

	ET_GIB,         // leave a trail
	ET_ROCKET,      // redlight + trail
	ET_GRENADE,
	ET_PLASMA,

	ET_LASERBEAM,   // for continuous beams

	ET_DECAL,

	ET_HUD,

	ET_LASER,
	ET_SPIKES,

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
#define EF_TEAMCOLOR_TRANSITION     ( 1 << 2 )
#define EF_GODMODE                  ( 1 << 3 )
#define EF_RACEGHOST                ( 1 << 4 )
#define EF_HAT                      ( 1 << 5 )
#define EF_WORLD_MODEL              ( 1 << 6 )
#define EF_TEAM_SILHOUETTE          ( 1 << 7 )

//===============================================================
// gs_weapons.c

enum {
	WEAPON_STATE_READY,
	WEAPON_STATE_ACTIVATING,
	WEAPON_STATE_DROPPING,
	WEAPON_STATE_REFIRE,
	WEAPON_STATE_RELOADING,
};

struct WeaponDef {
	const char * name;
	const char * short_name;

	RGB8 color;
	const char * description;
	int cost;

	const char * precache_models;
	const char * precache_sounds;
	const char * precache_images;

	int projectile_count;
	int clip_size;
	unsigned int reload_time;

	unsigned int weaponup_time;
	unsigned int weapondown_time;
	unsigned int refire_time;
	unsigned int range;
	float recoil;
	bool smooth_refire;

	float damage;
	float selfdamage;
	int knockback;
	int splash_radius;
	int mindamage;
	int minknockback;

	int speed;
	int spread;
};

const WeaponDef * GS_GetWeaponDef( WeaponType weapon );
WeaponType GS_SelectBestWeapon( const SyncPlayerState * player );
int GS_ThinkPlayerWeapon( const gs_state_t * gs, SyncPlayerState *playerState, int buttons, int msecs, int timeDelta );
trace_t *GS_TraceBullet( const gs_state_t * gs, trace_t *trace, vec3_t start, vec3_t dir, vec3_t right, vec3_t up, float r, float u, int range, int ignore, int timeDelta );
void GS_TraceLaserBeam( const gs_state_t * gs, trace_t *trace, vec3_t origin, vec3_t angles, float range, int ignore, int timeDelta, void ( *impact )( trace_t *tr, vec3_t dir ) );
