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

#include "gs_ref.h"

// shared callbacks

typedef struct {
#ifndef _MSC_VER
	void ( *Printf )( const char *format, ... ) __attribute__( ( format( printf, 1, 2 ) ) );
	void ( *Error )( const char *format, ... ) __attribute__( ( format( printf, 1, 2 ) ) ) __attribute__( ( noreturn ) );
#else
	void ( *Printf )( _Printf_format_string_ const char *format, ... );
	void ( *Error )( _Printf_format_string_ const char *format, ... );
#endif

	void ( *Trace )( trace_t *t, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int ignore, int contentmask, int timeDelta );
	entity_state_t *( *GetEntityState )( int entNum, int deltaTime );
	int ( *PointContents )( const vec3_t point, int timeDelta );
	void ( *PredictedEvent )( int entNum, int ev, int parm );
	void ( *PMoveTouchTriggers )( pmove_t *pm, vec3_t previous_origin );
	const char *( *GetConfigString )( int index );
} gs_module_api_t;

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

// item box
constexpr vec3_t item_box_mins = { -16.0f, -16.0f, -16.0f };
constexpr vec3_t item_box_maxs = { 16.0f, 16.0f, 40.0f };

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

enum {
	MATCH_STATE_NONE,
	MATCH_STATE_WARMUP,
	MATCH_STATE_COUNTDOWN,
	MATCH_STATE_PLAYTIME,
	MATCH_STATE_POSTMATCH,
	MATCH_STATE_WAITEXIT,

	MATCH_STATE_TOTAL
};

enum {
	GS_MODULE_GAME = 1,
	GS_MODULE_CGAME,
};

enum {
	GAMESTAT_FLAGS,
	GAMESTAT_MATCHSTATE,
	GAMESTAT_MATCHSTART,
	GAMESTAT_MATCHDURATION,
	GAMESTAT_CLOCKOVERRIDE,
	GAMESTAT_MAXPLAYERSINTEAM,
};

#define GAMESTAT_FLAG_PAUSED ( 1 << 0LL )
#define GAMESTAT_FLAG_WAITING ( 1 << 1LL )
#define GAMESTAT_FLAG_HASCHALLENGERS ( 1 << 2LL )
#define GAMESTAT_FLAG_INHIBITSHOOTING ( 1 << 3LL )
#define GAMESTAT_FLAG_ISTEAMBASED ( 1 << 4LL )
#define GAMESTAT_FLAG_ISRACE ( 1 << 5LL )
#define GAMESTAT_FLAG_COUNTDOWN ( 1 << 6LL )
#define GAMESTAT_FLAG_SELFDAMAGE ( 1 << 7LL )
#define GAMESTAT_FLAG_INFINITEAMMO ( 1 << 8LL )
#define GAMESTAT_FLAG_CANFORCEMODELS ( 1 << 9LL )

typedef struct {
	int module;
	int maxclients;
	game_state_t gameState;
	gs_module_api_t api;
} gs_state_t;

extern gs_state_t gs;

#define GS_GamestatSetFlag( flag, b ) ( b ? ( gs.gameState.stats[GAMESTAT_FLAGS] |= flag ) : ( gs.gameState.stats[GAMESTAT_FLAGS] &= ~flag ) )
#define GS_ShootingDisabled() ( ( gs.gameState.stats[GAMESTAT_FLAGS] & GAMESTAT_FLAG_INHIBITSHOOTING ) ? true : false )
#define GS_HasChallengers() ( ( gs.gameState.stats[GAMESTAT_FLAGS] & GAMESTAT_FLAG_HASCHALLENGERS ) ? true : false )
#define GS_TeamBasedGametype() ( ( gs.gameState.stats[GAMESTAT_FLAGS] & GAMESTAT_FLAG_ISTEAMBASED ) ? true : false )
#define GS_RaceGametype() ( ( gs.gameState.stats[GAMESTAT_FLAGS] & GAMESTAT_FLAG_ISRACE ) ? true : false )
#define GS_MatchPaused() ( ( gs.gameState.stats[GAMESTAT_FLAGS] & GAMESTAT_FLAG_PAUSED ) ? true : false )
#define GS_MatchWaiting() ( ( gs.gameState.stats[GAMESTAT_FLAGS] & GAMESTAT_FLAG_WAITING ) ? true : false )
#define GS_Countdown() ( ( gs.gameState.stats[GAMESTAT_FLAGS] & GAMESTAT_FLAG_COUNTDOWN ) ? true : false )
#define GS_InfiniteAmmo() ( ( gs.gameState.stats[GAMESTAT_FLAGS] & GAMESTAT_FLAG_INFINITEAMMO ) ? true : false )
#define GS_CanForceModels() ( ( gs.gameState.stats[GAMESTAT_FLAGS] & GAMESTAT_FLAG_CANFORCEMODELS ) ? true : false )

#define GS_MatchState() ( gs.gameState.stats[GAMESTAT_MATCHSTATE] )
#define GS_MaxPlayersInTeam() ( gs.gameState.stats[GAMESTAT_MAXPLAYERSINTEAM] )
#define GS_InvidualGameType() ( GS_MaxPlayersInTeam() == 1 )

#define GS_MatchDuration() ( gs.gameState.stats[GAMESTAT_MATCHDURATION] )
#define GS_MatchStartTime() ( gs.gameState.stats[GAMESTAT_MATCHSTART] )
#define GS_MatchEndTime() ( gs.gameState.stats[GAMESTAT_MATCHDURATION] ? gs.gameState.stats[GAMESTAT_MATCHSTART] + gs.gameState.stats[GAMESTAT_MATCHDURATION] : 0 )
#define GS_MatchClockOverride() ( gs.gameState.stats[GAMESTAT_CLOCKOVERRIDE] )

//==================================================================

// S_DEFAULT_ATTENUATION_MODEL	"3"
#define ATTN_NONE               0       // full volume the entire level
#define ATTN_DISTANT            0.5     // distant sound (most likely explosions)
#define ATTN_NORM               1       // players, weapons, etc
#define ATTN_IDLE               2.5     // stuff around you
#define ATTN_STATIC             5       // diminish very rapidly with distance
#define ATTN_WHISPER            10      // must be very close to hear it

// sound channels
// channel 0 never willingly overrides
// other channels (1-7) always override a playing sound on that channel
enum {
	CHAN_AUTO,
	CHAN_PAIN,
	CHAN_VOICE,
	CHAN_ITEM,
	CHAN_BODY,
	CHAN_MUZZLEFLASH,
	CHAN_ANNOUNCER,

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

int GS_SlideMove( move_t *move );
void GS_ClipVelocity( vec3_t in, vec3_t normal, vec3_t out, float overbounce );

int GS_LinearMovement( const entity_state_t *ent, int64_t time, vec3_t dest );
void GS_LinearMovementDelta( const entity_state_t *ent, int64_t oldTime, int64_t curTime, vec3_t dest );

//==============================================================
//
//PLAYER MOVEMENT CODE
//
//Common between server and client so prediction matches
//
//==============================================================

#define STEPSIZE 18

void Pmove( pmove_t *pmove );

//===============================================================

//==================
//SPLITMODELS
//==================

// The parts must be listed in draw order
enum {
	LOWER = 0,
	UPPER,
	HEAD,

	PMODEL_PARTS
};

// -Torso DEATH frames and Legs DEATH frames must be the same.

// ANIMATIONS

enum {
	ANIM_NONE,
	BOTH_DEATH1,      //Death animation
	BOTH_DEAD1,       //corpse on the ground
	BOTH_DEATH2,
	BOTH_DEAD2,
	BOTH_DEATH3,
	BOTH_DEAD3,

	LEGS_STAND_IDLE,
	LEGS_WALK_FORWARD,
	LEGS_WALK_BACK,
	LEGS_WALK_LEFT,
	LEGS_WALK_RIGHT,

	LEGS_RUN_FORWARD,
	LEGS_RUN_BACK,
	LEGS_RUN_LEFT,
	LEGS_RUN_RIGHT,

	LEGS_JUMP_LEG1,
	LEGS_JUMP_LEG2,
	LEGS_JUMP_NEUTRAL,
	LEGS_LAND,

	LEGS_CROUCH_IDLE,
	LEGS_CROUCH_WALK,

	LEGS_SWIM_FORWARD,
	LEGS_SWIM_NEUTRAL,

	LEGS_WALLJUMP,
	LEGS_WALLJUMP_LEFT,
	LEGS_WALLJUMP_RIGHT,
	LEGS_WALLJUMP_BACK,

	LEGS_DASH,
	LEGS_DASH_LEFT,
	LEGS_DASH_RIGHT,
	LEGS_DASH_BACK,

	TORSO_HOLD_BLADE,
	TORSO_HOLD_PISTOL,
	TORSO_HOLD_LIGHTWEAPON,
	TORSO_HOLD_HEAVYWEAPON,
	TORSO_HOLD_AIMWEAPON,

	TORSO_SHOOT_BLADE,
	TORSO_SHOOT_PISTOL,
	TORSO_SHOOT_LIGHTWEAPON,
	TORSO_SHOOT_HEAVYWEAPON,
	TORSO_SHOOT_AIMWEAPON,

	TORSO_WEAPON_SWITCHOUT,
	TORSO_WEAPON_SWITCHIN,

	TORSO_DROPHOLD,
	TORSO_DROP,

	TORSO_SWIM,

	TORSO_PAIN1,
	TORSO_PAIN2,
	TORSO_PAIN3,

	PMODEL_TOTAL_ANIMATIONS,
};

//===============================================================

#define HEALTH_TO_INT( x )    ( ( x ) < 1.0f ? (int)ceil( ( x ) ) : (int)floor( ( x ) + 0.5f ) )

// gs_items - shared items definitions

//==================
//	ITEM TAGS
//==================
// max weapons allowed by the net protocol are 128
typedef enum {
	WEAP_NONE = 0,
	WEAP_GUNBLADE,
	WEAP_MACHINEGUN,
	WEAP_RIOTGUN,
	WEAP_GRENADELAUNCHER,
	WEAP_ROCKETLAUNCHER,
	WEAP_PLASMAGUN,
	WEAP_LASERGUN,
	WEAP_ELECTROBOLT,

	WEAP_TOTAL
} weapon_tag_t;

typedef enum {
	AMMO_NONE = 0,
	AMMO_GUNBLADE = WEAP_TOTAL,
	AMMO_BULLETS,
	AMMO_SHELLS,
	AMMO_GRENADES,
	AMMO_ROCKETS,
	AMMO_PLASMA,
	AMMO_LASERS,
	AMMO_BOLTS,

	AMMO_TOTAL,
	ITEMS_TOTAL = AMMO_TOTAL,
} ammo_tag_t;

#define GS_MAX_ITEM_TAGS ITEMS_TOTAL

#define MAX_ITEM_MODELS 2

// gsitem_t->type
// define as bitflags values so they can be masked
typedef enum {
	IT_WEAPON = 1,
	IT_AMMO = 2,
} itemtype_t;

typedef struct gitem_s {
	int tag;
	itemtype_t type;

	const char *name;      // for printing on pickup
	const char *shortname; // for printing on messages
	const char *color;     // for printing on messages

	int ammo_tag;          // uses this ammo, for weapons

	// space separated string of stuff to precache that's not mentioned above
	const char *precache_models;
	const char *precache_sounds;
	const char *precache_images;
} gsitem_t;

extern const gsitem_t itemdefs[];

const gsitem_t *GS_FindItemByTag( const int tag );
const gsitem_t *GS_FindItemByName( const char *name );
const gsitem_t *GS_Cmd_UseItem( player_state_t *playerState, const char *string, int typeMask );
const gsitem_t *GS_Cmd_NextWeapon_f( player_state_t *playerState, int predictedWeaponSwitch );
const gsitem_t *GS_Cmd_PrevWeapon_f( player_state_t *playerState, int predictedWeaponSwitch );

//===============================================================

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
const char *GS_DefaultTeamName( int team );
int GS_TeamFromName( const char *teamname );
bool GS_IsTeamDamage( entity_state_t *targ, entity_state_t *attacker );

//===============================================================

// gs_misc.c
void GS_Obituary( void *victim, void *attacker, int mod, char *message, char *message2 );
void GS_TouchPushTrigger( player_state_t *playerState, entity_state_t *pusher );
int GS_WaterLevel( entity_state_t *state, vec3_t mins, vec3_t maxs );
void GS_BBoxForEntityState( entity_state_t *state, vec3_t mins, vec3_t maxs );

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

enum {
	STAT_LAYOUTS = 0,
	STAT_HEALTH,
	STAT_WEAPON,
	STAT_WEAPON_TIME,
	STAT_PENDING_WEAPON,

	STAT_SCORE,
	STAT_TEAM,
	STAT_REALTEAM,
	STAT_NEXT_RESPAWN,

	STAT_POINTED_PLAYER,
	STAT_POINTED_TEAMPLAYER,

	STAT_TEAM_ALPHA_SCORE,
	STAT_TEAM_BETA_SCORE,

	STAT_LAST_KILLER,

	// the stats below this point are set by the gametype scripts
	GS_GAMETYPE_STATS_START = 32,

	STAT_PROGRESS = GS_GAMETYPE_STATS_START,
	STAT_PROGRESS_TYPE,

	STAT_ROUND_TYPE,

	STAT_CARRYING_BOMB,
	STAT_CAN_PLANT_BOMB,
	STAT_CAN_CHANGE_LOADOUT,

	STAT_ALPHA_PLAYERS_ALIVE,
	STAT_ALPHA_PLAYERS_TOTAL,
	STAT_BETA_PLAYERS_ALIVE,
	STAT_BETA_PLAYERS_TOTAL,

	STAT_TIME_SELF,
	STAT_TIME_BEST,
	STAT_TIME_RECORD,
	STAT_TIME_ALPHA,
	STAT_TIME_BETA,

	GS_GAMETYPE_STATS_END = PS_MAX_STATS,

	MAX_STATS = PS_MAX_STATS, // 64
};

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

// STAT_LAYOUTS flag bits meanings
#define STAT_LAYOUT_INSTANTRESPAWN  ( 1 << 1 )
#define STAT_LAYOUT_SCOREBOARD      ( 1 << 2 )
#define STAT_LAYOUT_TEAMTAB         ( 1 << 3 )
#define STAT_LAYOUT_CHALLENGER      ( 1 << 4 ) // player is in challengers queue (used for ingame menu)
#define STAT_LAYOUT_READY           ( 1 << 5 ) // player is ready (used for ingame menu)
#define STAT_LAYOUT_SPECTEAMONLY    ( 1 << 6 )

#define STAT_NOTSET                 -9999 // used for stats that don't have meaningful value atm.

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
	PAIN_80,
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

// entity_state_t->event values
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

enum RoundType {
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

//===============================================================

// entity_state_t->type values
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
	ET_PARTICLES,

	ET_HUD,

	ET_LASER,
	ET_SPIKES,

	// eventual entities: types below this will get event treatment
	ET_EVENT = EVENT_ENTITIES_START,
	ET_SOUNDEVENT,

	ET_TOTAL_TYPES, // current count
	MAX_ENTITY_TYPES = 128
};

// entity_state_t->effects
// Effects are things handled on the client side (lights, particles, frame animations)
// that happen constantly on the given entity.
// An entity that has effects will be sent to the client
// even if it has a zero index model.
#define EF_ROTATE_AND_BOB           ( 1 << 0 )
#define EF_CARRIER                  ( 1 << 1 )
#define EF_TAKEDAMAGE               ( 1 << 2 )
#define EF_TEAMCOLOR_TRANSITION     ( 1 << 3 )
#define EF_GODMODE                  ( 1 << 4 )
#define EF_RACEGHOST                ( 1 << 5 )
#define EF_HAT                      ( 1 << 6 )
#define EF_WORLD_MODEL              ( 1 << 7 )

//===============================================================
// gs_weapons.c

enum {
	WEAPON_STATE_READY,
	WEAPON_STATE_ACTIVATING,
	WEAPON_STATE_DROPPING,
	WEAPON_STATE_POWERING,
	WEAPON_STATE_COOLDOWN,
	WEAPON_STATE_FIRING,
	WEAPON_STATE_NOAMMOCLICK,
	WEAPON_STATE_REFIRE,        // projectile loading
};

typedef struct firedef_s {
	//ammo def
	int ammo_id;
	int usage_count;
	int projectile_count;

	// timings
	unsigned int weaponup_time;
	unsigned int weapondown_time;
	unsigned int reload_time;
	unsigned int timeout;
	bool smooth_refire;

	// damages
	float damage;
	float selfdamage;
	int knockback;
	int splash_radius;
	int mindamage;
	int minknockback;

	// projectile def
	int speed;
	int spread;     // horizontal spread
	int v_spread;   // vertical spread
} firedef_t;

typedef struct {
	const char *name;
	int weapon_id;

	firedef_t firedef;
} gs_weapon_definition_t;

void GS_InitModule( int module, int maxClients, gs_module_api_t *api );
gs_weapon_definition_t *GS_GetWeaponDef( int weapon );
int GS_SelectBestWeapon( player_state_t *playerState );
bool GS_CheckAmmoInWeapon( player_state_t *playerState, int checkweapon );
firedef_t *GS_FiredefForPlayerState( player_state_t *playerState, int checkweapon );
int GS_ThinkPlayerWeapon( player_state_t *playerState, int buttons, int msecs, int timeDelta );
trace_t *GS_TraceBullet( trace_t *trace, vec3_t start, vec3_t dir, vec3_t right, vec3_t up, float r, float u, int range, int ignore, int timeDelta );
void GS_TraceLaserBeam( trace_t *trace, vec3_t origin, vec3_t angles, float range, int ignore, int timeDelta, void ( *impact )( trace_t *tr, vec3_t dir ) );

//===============================================================
// gs_weapondefs.c

extern firedef_t ammoFireDefs[];
