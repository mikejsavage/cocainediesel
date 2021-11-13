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

#include "qcommon/qcommon.h"
#include "qcommon/hash.h"
#include "gameshared/gs_public.h"
#include "gameshared/gs_weapons.h"
#include "game/g_public.h"
#include "game/g_gametypes.h"
#include "game/g_ai.h"
#include "server/server.h"

//==================================================================

// FIXME: Medar: Remove the spectator test and just make sure they always have health
#define G_IsDead( ent )       ( ( !( ent )->r.client || ( ent )->s.team != TEAM_SPECTATOR ) && HEALTH_TO_INT( ( ent )->health ) <= 0 )

#define FRAMETIME ( (float)game.frametime * 0.001f )

#define MAX_FLOOD_MESSAGES 32

enum damage_t {
	DAMAGE_NO,
	DAMAGE_YES,     // will take damage if hit
	DAMAGE_AIM      // auto targeting recognizes this
};

// deadflag
#define DEAD_NO         0
#define DEAD_DEAD       1

// edict->movetype values
enum movetype_t {
	MOVETYPE_NONE,      // never moves
	MOVETYPE_PLAYER,    // never moves (but is moved by pmove)
	MOVETYPE_NOCLIP,    // like MOVETYPE_PLAYER, but not clipped
	MOVETYPE_PUSH,      // no clip to world, push on box contact
	MOVETYPE_STOP,      // no clip to world, stops on box contact
	MOVETYPE_FLY,
	MOVETYPE_TOSS,      // gravity
	MOVETYPE_LINEARPROJECTILE, // extra size to monsters
	MOVETYPE_BOUNCE,
	MOVETYPE_BOUNCEGRENADE,
};

#define TIMEOUT_TIME                    180000
#define TIMEIN_TIME                     5000

struct timeout_t {
	int64_t time;
	int64_t endtime;
	int caller;
	int used[MAX_CLIENTS];
};

//
// this structure is cleared as each map is entered
//
struct level_locals_t {
	int64_t framenum;
	int64_t time; // time in milliseconds
	int64_t spawnedTimeStamp; // time when map was restarted
	int64_t finalMatchDuration;

	char callvote_map[MAX_CONFIGSTRING_CHARS];
	char autorecord_name[128];

	bool canSpawnEntities; // security check to prevent entities being spawned before map entities

	// intermission state
	bool exitNow;
	bool hardReset;

	// gametype definition and execution
	Gametype gametype;

	bool ready[MAX_CLIENTS];
	bool forceExit;     // just exit, ignore extended time checks

	edict_t *current_entity;    // entity running from G_RunFrame

	timeout_t timeout;
};

// spawn_temp_t is only used to hold entity field values that
// can be set from the editor, but aren't actualy present
// in edict_t during gameplay
struct spawn_temp_t {
	Span< const char > classname;
	int lip;
	int distance;
	int height;
	float radius;
	StringHash noise;
	StringHash noise_start;
	StringHash noise_stop;
	float pausetime;
	int gameteam;
	int size;
	float spawn_probability;
};

struct score_stats_t {
	int kills;
	int deaths;
	int suicides;

	int score;
	bool ready;

	int accuracy_shots[ Weapon_Count ];
	int accuracy_hits[ Weapon_Count ];
	int accuracy_damage[ Weapon_Count ];
	int accuracy_frags[ Weapon_Count ];
	int total_damage_given;
	int total_damage_received;
};

extern gs_state_t server_gs;
extern level_locals_t level;
extern spawn_temp_t st;

extern mempool_t *gamepool;

extern Vec3 knockbackOfDeath;
extern int damageFlagsOfDeath;

extern cvar_t *sv_password;
extern cvar_t *g_operator_password;
extern cvar_t *developer;

extern cvar_t *filterban;

extern cvar_t *g_maxvelocity;

extern cvar_t *sv_cheats;

extern cvar_t *g_floodprotection_messages;
extern cvar_t *g_floodprotection_team;
extern cvar_t *g_floodprotection_seconds;
extern cvar_t *g_floodprotection_penalty;

extern cvar_t *g_inactivity_maxtime;

extern cvar_t *g_maplist;
extern cvar_t *g_maprotation;

extern cvar_t *g_scorelimit;

extern cvar_t *g_projectile_prestep;
extern cvar_t *g_numbots;
extern cvar_t *g_maxtimeouts;

extern cvar_t *g_deadbody_followkiller;
extern cvar_t *g_antilag_timenudge;
extern cvar_t *g_antilag_maxtimedelta;

extern cvar_t *g_teams_maxplayers;
extern cvar_t *g_teams_allow_uneven;

extern cvar_t *g_autorecord;
extern cvar_t *g_autorecord_maxdemos;
extern cvar_t *g_allow_spectator_voting;

extern cvar_t *g_asGC_stats;
extern cvar_t *g_asGC_interval;

void G_Teams_Join_Cmd( edict_t *ent );
bool G_Teams_JoinTeam( edict_t *ent, int team );
void G_Teams_UpdateMembersList();
bool G_Teams_JoinAnyTeam( edict_t *ent, bool silent );
void G_Teams_SetTeam( edict_t *ent, int team );

void Cmd_Say_f( edict_t *ent, bool arg0, bool checkflood );
void G_Say_Team( edict_t *who, const char *inmsg, bool checkflood );

void G_Match_Ready( edict_t *ent );
void G_Match_NotReady( edict_t *ent );
void G_Match_ToggleReady( edict_t *ent );
void G_Match_CheckReadys();
void G_EndMatch();

//
// g_spawnpoints.c
//
void DropSpawnToFloor( edict_t * ent );
void SelectSpawnPoint( edict_t *ent, edict_t **spawnpoint, Vec3 * origin, Vec3 * angles );
void SP_post_match_camera( edict_t *ent );

//
// g_func.c
//
void SP_func_rotating( edict_t *ent );
void SP_func_door( edict_t *ent );
void SP_func_door_rotating( edict_t *ent );
void SP_func_train( edict_t *ent );
void SP_func_wall( edict_t *self );
void SP_func_static( edict_t *ent );

//
// g_gladiator
//
void SP_spike( edict_t * ent );
void SP_spikes( edict_t * ent );

//
// g_speakers
//

void SP_speaker_wall( edict_t * ent );

// item spawnflags
#define ITEM_TRIGGER_SPAWN  0x00000001
#define ITEM_NO_TOUCH       0x00000002
// 6 bits reserved for editor flags
// 8 bits used as power cube id bits for coop games
#define DROPPED_ITEM        0x00010000
#define DROPPED_PLAYER_ITEM 0x00020000
#define ITEM_TARGETS_USED   0x00040000
#define ITEM_IGNORE_MAX     0x00080000

//
// g_cmds.c
//

typedef void ( *gamecommandfunc_t )( edict_t * );

bool CheckFlood( edict_t *ent, bool teamonly );
void G_InitGameCommands();
void G_PrecacheGameCommands();
void G_AddCommand( const char *name, gamecommandfunc_t cmdfunc );

//
// g_utils.c
//
#define G_LEVELPOOL_BASE_SIZE   45 * 1024 * 1024

bool KillBox( edict_t *ent, DamageType damage_type, Vec3 knockback );
float LookAtKillerYAW( edict_t *self, edict_t *inflictor, edict_t *attacker );
edict_t * G_Find( edict_t * cursor, StringHash edict_t::* field, StringHash value );
edict_t * G_PickRandomEnt( StringHash edict_t::* field, StringHash value );
edict_t * G_PickTarget( StringHash name );
void G_UseTargets( edict_t *ent, edict_t *activator );
void G_SetMovedir( Vec3 * angles, Vec3 * movedir );
void G_InitMover( edict_t *ent );

void G_InitEdict( edict_t *e );
edict_t *G_Spawn();
void G_FreeEdict( edict_t *e );

char *_G_CopyString( const char *in, const char *filename, int fileline );
#define G_CopyString( in ) _G_CopyString( in, __FILE__, __LINE__ )

void G_AddEvent( edict_t *ent, int event, u64 parm, bool highPriority );
edict_t *G_SpawnEvent( int event, u64 parm, const Vec3 * origin );
void G_MorphEntityIntoEvent( edict_t *ent, int event, u64 parm );

void G_CallThink( edict_t *ent );
void G_CallTouch( edict_t *self, edict_t *other, Plane *plane, int surfFlags );
void G_CallUse( edict_t *self, edict_t *other, edict_t *activator );
void G_CallStop( edict_t *self );
void G_CallPain( edict_t *ent, edict_t *attacker, float kick, float damage );
void G_CallDie( edict_t *ent, edict_t *inflictor, edict_t *attacker, int assistorNo, DamageType damage_type, int damage );

#ifndef _MSC_VER
void G_PrintMsg( edict_t *ent, const char *format, ... ) __attribute__( ( format( printf, 2, 3 ) ) );
void G_ChatMsg( edict_t *ent, edict_t *who, bool teamonly, const char *format, ... ) __attribute__( ( format( printf, 4, 5 ) ) );
void G_CenterPrintMsg( edict_t *ent, const char *format, ... ) __attribute__( ( format( printf, 2, 3 ) ) );
#else
void G_PrintMsg( edict_t *ent, _Printf_format_string_ const char *format, ... );
void G_ChatMsg( edict_t *ent, edict_t *who, bool teamonly, _Printf_format_string_ const char *format, ... );
void G_CenterPrintMsg( edict_t *ent, _Printf_format_string_ const char *format, ... );
#endif
void G_ClearCenterPrint( edict_t *ent );

void G_DebugPrint( const char * format, ... );

void G_Obituary( edict_t *victim, edict_t *attacker, int topAssistEntNo, DamageType mod, bool wallbang );

edict_t *G_Sound( edict_t *owner, int channel, StringHash sound );
edict_t *G_PositionedSound( Vec3 origin, int channel, StringHash sound );
void G_GlobalSound( int channel, StringHash sound );
void G_LocalSound( edict_t *owner, int channel, StringHash sound );

#define G_ISGHOSTING( x ) ( ( x )->r.solid == SOLID_NOT )

void G_TeleportEffect( edict_t *ent, bool in );
void G_RespawnEffect( edict_t *ent );
int G_SolidMaskForEnt( edict_t *ent );
void G_CheckGround( edict_t *ent );
void G_CategorizePosition( edict_t *ent );
void G_ReleaseClientPSEvent( gclient_t *client );
void G_AddPlayerStateEvent( gclient_t *client, int event, u64 parm );
void G_ClearPlayerStateEvents( gclient_t *client );

// announcer events
void G_AnnouncerSound( edict_t *targ, StringHash sound, int team, bool queued, edict_t *ignore );
edict_t *G_PlayerForText( const char *text );

void G_SetBoundsForSpanEntity( edict_t *ent, float size );

//
// g_callvotes.c
//
void G_CallVotes_Init();
void G_FreeCallvotes();
void G_CallVotes_ResetClient( int n );
void G_CallVotes_CmdVote( edict_t *ent );
void G_CallVotes_Think();
bool G_Callvotes_HasVoted( edict_t *ent );
void G_CallVote_Cmd( edict_t *ent );
void G_OperatorVote_Cmd( edict_t *ent );

//
// g_trigger.c
//
void SP_trigger_teleport( edict_t *ent );
void SP_trigger_always( edict_t *ent );
void SP_trigger_once( edict_t *ent );
void SP_trigger_multiple( edict_t *ent );
void SP_trigger_push( edict_t *ent );
void SP_trigger_hurt( edict_t *ent );
void SP_trigger_key( edict_t *ent );

//
// g_clip.c
//
#define MAX_ENT_AREAS 16

struct link_t {
	link_t *prev, *next;
	int entNum;
};

int G_PointContents( Vec3 p );
void G_Trace( trace_t *tr, Vec3 start, Vec3 mins, Vec3 maxs, Vec3 end, edict_t *passedict, int contentmask );
int G_PointContents4D( Vec3 p, int timeDelta );
void G_Trace4D( trace_t *tr, Vec3 start, Vec3 mins, Vec3 maxs, Vec3 end, edict_t *passedict, int contentmask, int timeDelta );
void GClip_BackUpCollisionFrame();
int GClip_FindInRadius4D( Vec3 org, float rad, int *list, int maxcount, int timeDelta );
void G_SplashFrac4D( const edict_t *ent, Vec3 hitpoint, float maxradius, Vec3 * pushdir, float *frac, int timeDelta, bool selfdamage );
void GClip_ClearWorld();
void GClip_SetBrushModel( edict_t * ent );
void GClip_SetAreaPortalState( edict_t *ent, bool open );
void GClip_LinkEntity( edict_t *ent );
void GClip_UnlinkEntity( edict_t *ent );
void GClip_TouchTriggers( edict_t *ent );
void G_PMoveTouchTriggers( pmove_t *pm, Vec3 previous_origin );
SyncEntityState *G_GetEntityStateForDeltaTime( int entNum, int deltaTime );
int GClip_FindInRadius( Vec3 org, float rad, int *list, int maxcount );

bool IsHeadshot( int entNum, Vec3 hit, int timeDelta );

// BoxEdicts() can return a list of either solid or trigger entities
// FIXME: eliminate AREA_ distinction?
#define AREA_ALL       -1
#define AREA_SOLID      1
#define AREA_TRIGGERS   2
int GClip_AreaEdicts( Vec3 mins, Vec3 maxs, int *list, int maxcount, int areatype, int timeDelta );
bool GClip_EntityContact( Vec3 mins, Vec3 maxs, edict_t *ent );

//
// g_combat.c
//
bool G_IsTeamDamage( SyncEntityState *targ, SyncEntityState *attacker );
void G_Killed( edict_t *targ, edict_t *inflictor, edict_t *attacker, int topAssistorNo, DamageType damage_type, int damage );
void G_SplashFrac( const SyncEntityState *s, const entity_shared_t *r, Vec3 point, float maxradius, Vec3 * pushdir, float *frac, bool selfdamage );
void G_Damage( edict_t *targ, edict_t *inflictor, edict_t *attacker, Vec3 pushdir, Vec3 dmgdir, Vec3 point, float damage, float knockback, int dflags, DamageType damage_type );
void SpawnDamageEvents( const edict_t * attacker, edict_t * victim, float damage, bool headshot, Vec3 pos, Vec3 dir );
void G_RadiusKnockback( const WeaponDef * def, edict_t *attacker, Vec3 pos, Plane *plane, DamageType damage_type, int timeDelta );
void G_RadiusDamage( edict_t *inflictor, edict_t *attacker, Plane *plane, edict_t *ignore, DamageType damage_type );

// damage flags
#define DAMAGE_RADIUS         ( 1 << 0 )  // damage was indirect
#define DAMAGE_KNOCKBACK_SOFT ( 1 << 1 )
#define DAMAGE_HEADSHOT       ( 1 << 2 )
#define DAMAGE_WALLBANG       ( 1 << 3 )

//
// g_misc.c
//
void ThrowSmallPileOfGibs( edict_t *self, Vec3 knockback, int damage );

void SP_path_corner( edict_t *self );

void SP_model( edict_t *ent );

void SP_decal( edict_t * ent );

//
// g_weapon.c
//
void G_FireWeapon( edict_t * ent, u64 parm );
void G_UseGadget( edict_t * ent, GadgetType gadget, u64 parm );

//
// g_chasecam	//newgametypes
//
void G_SpectatorMode( edict_t *ent );
void G_ChasePlayer( edict_t *ent, const char *name, bool teamonly, int followmode );
void G_ChaseStep( edict_t *ent, int step );
void Cmd_SwitchChaseCamMode_f( edict_t *ent );
void Cmd_ChaseCam_f( edict_t *ent );
void G_EndServerFrames_UpdateChaseCam();

//
// g_client.c
//
void ClientUserinfoChanged( edict_t *ent, char *userinfo );
void G_Client_UpdateActivity( gclient_t *client );
void G_Client_InactivityRemove( gclient_t *client );
void G_ClientRespawn( edict_t *self, bool ghost );
score_stats_t * G_ClientGetStats( edict_t * ent );
void G_ClientClearStats( edict_t *ent );
void G_GhostClient( edict_t *self );
void ClientThink( edict_t *ent, UserCommand *cmd, int timeDelta );
void G_ClientThink( edict_t *ent );
void G_CheckClientRespawnClick( edict_t *ent );
bool ClientConnect( edict_t *ent, char *userinfo, bool fakeClient );
void ClientDisconnect( edict_t *ent, const char *reason );
void ClientBegin( edict_t *ent );
void ClientCommand( edict_t *ent );
void G_PredictedEvent( int entNum, int ev, u64 parm );
void G_PredictedFireWeapon( int entNum, u64 weapon_and_entropy );
void G_PredictedUseGadget( int entNum, GadgetType gadget, u64 parm );
void G_SelectWeapon( edict_t * ent, int index );
void G_GiveWeapon( edict_t * ent, WeaponType weapon );
void G_TeleportPlayer( edict_t *player, edict_t *dest );
bool G_PlayerCanTeleport( edict_t *player );

//
// g_player.c
//
void player_pain( edict_t *self, edict_t *other, float kick, int damage );
void player_die( edict_t *self, edict_t *inflictor, edict_t *attacker, int topAssistorEntNo, DamageType damage_type, int damage );
void player_think( edict_t *self );

//
// g_target.c
//
void target_laser_start( edict_t *self );

void SP_target_laser( edict_t *self );
void SP_target_position( edict_t *self );
void SP_target_delay( edict_t *ent );

//
// g_svcmds.c
//
void SV_ResetPacketFiltersTimeouts();
bool SV_FilterPacket( char *from );
void G_AddServerCommands();
void G_RemoveCommands();
void SV_ReadIPList();
void SV_WriteIPList();

//
// p_view.c
//
void G_ClientEndSnapFrame( edict_t *ent );
void G_ClientAddDamageIndicatorImpact( gclient_t *client, int damage, Vec3 dir );
void G_ClientDamageFeedback( edict_t *ent );

//
// p_hud.c
//


void G_SetClientStats( edict_t *ent );
void G_Snap_UpdateWeaponListMessages();

//
// g_phys.c
//
void SV_Impact( edict_t *e1, trace_t *trace );
void G_RunEntity( edict_t *ent );
int G_BoxSlideMove( edict_t *ent, int contentmask, float slideBounce, float friction );

//
// g_main.c
//

// memory management
#define G_Malloc( size ) _Mem_AllocExt( gamepool, size, 16, 1, 0, 0, __FILE__, __LINE__ )
#define G_Free( mem ) Mem_Free( mem )

void G_Init( unsigned int framemsec );
void G_Shutdown();
void G_ExitLevel();
void G_GamestatSetFlag( int flag, bool b );
void G_Timeout_Reset();

//
// g_frame.c
//
void G_CheckCvars();
void G_RunFrame( unsigned int msec );
void G_SnapClients();
void G_ClearSnap();
void G_SnapFrame();


//
// g_spawn.c
//
void G_RespawnLevel();
void G_ResetLevel();
void G_InitLevel( const char *mapname, int64_t levelTime );
void G_LoadMap( const char * name );

//============================================================================

struct projectileinfo_t {
	int radius;
	float minDamage;
	float maxDamage;
	float minKnockback;
	float maxKnockback;
	DamageType damage_type;
};

struct chasecam_t {
	bool active;
	int target;
	int mode;                   //3rd or 1st person
	int range;
	bool teamonly;
	int64_t timeout;           //delay after loosing target
	int followmode;
};

#define MAX_ASSIST_INFO 4
struct assistinfo_t {
	int entno;
	int cumDamage;
	int64_t lastTime;
};

struct moveinfo_t {
	// fixed data
	Vec3 start_origin;
	Vec3 start_angles;
	Vec3 end_origin;
	Vec3 end_angles;

	StringHash sound_start;
	StringHash sound_middle;
	StringHash sound_end;

	Vec3 movedir;  // direction defined in the bsp

	float speed;
	float distance;    // used by binary movers

	float wait;

	// state data
	int state;
	float current_speed;    // used by func_rotating

	void ( *endfunc )( edict_t * );
	void ( *blocked )( edict_t *self, edict_t *other );

	Vec3 dest;
	Vec3 destangles;
};

#define MAX_CLIENT_EVENTS   16
#define MAX_CLIENT_EVENTS_MASK ( MAX_CLIENT_EVENTS - 1 )

#define G_MAX_TIME_DELTAS   8
#define G_MAX_TIME_DELTAS_MASK ( G_MAX_TIME_DELTAS - 1 )

struct client_snapreset_t {
	int buttons;
	uint8_t plrkeys; // used for displaying key icons
	int damageTaken;
	Vec3 damageTakenDir;
};

struct client_respawnreset_t {
	client_snapreset_t snap;
	chasecam_t chase;

	int64_t timeStamp; // last time it was reset

	SyncEvent events[MAX_CLIENT_EVENTS];
	unsigned int eventsCurrent;
	unsigned int eventsHead;

	int old_waterlevel;
	int old_watertype;
};

struct client_levelreset_t {
	int64_t timeStamp;				// last time it was reset

	int64_t last_vsay;				// time when last vsay was said
	int64_t last_activity;
	int64_t last_spray;

	score_stats_t stats;

	// flood protection
	int64_t flood_locktill;			// locked from talking
	int64_t flood_when[MAX_FLOOD_MESSAGES];        // when messages were said
	int flood_whenhead;             // head pointer for when said
	// team only
	int64_t flood_team_when[MAX_FLOOD_MESSAGES];   // when messages were said
	int flood_team_whenhead;        // head pointer for when said

	int64_t callvote_when;
};

struct client_teamreset_t {
	int64_t timeStamp; // last time it was reset

	// for position command
	bool position_saved;
	Vec3 position_origin;
	Vec3 position_angles;
	int64_t position_lastcmd;
};

struct gclient_t {
	// known to server
	SyncPlayerState ps;          // communicated by server to clients
	client_shared_t r;

	// DO NOT MODIFY ANYTHING ABOVE THIS, THE SERVER
	// EXPECTS THE FIELDS IN THAT ORDER!

	client_respawnreset_t resp;
	client_levelreset_t level;
	client_teamreset_t teamstate;

	//short ucmd_angles[3]; // last ucmd angles

	// persistent info along all the time the client is connected

	char userinfo[MAX_INFO_STRING];
	char netname[MAX_NAME_CHARS + 1];
	char ip[MAX_INFO_VALUE];
	char socket[MAX_INFO_VALUE];

	bool connecting;

	int team;
	bool isoperator;

	UserCommand ucmd;
	int timeDelta;              // time offset to adjust for shots collision (antilag)
	int timeDeltas[G_MAX_TIME_DELTAS];
	int timeDeltasHead;

	pmove_state_t old_pmove;    // for detecting out-of-pmove changes
};

struct snap_edict_t {
	bool kill;
	float damage_given;
};

using EdictTouchCallback = void ( * )( edict_t * self, edict_t * other, Plane * plane, int surfFlags );

struct edict_t {
	SyncEntityState s;
	entity_shared_t r;

	// DO NOT MODIFY ANYTHING ABOVE THIS, THE SERVER
	// EXPECTS THE FIELDS IN THAT ORDER!

	//================================

	int linkcount;

	// physics grid areas this edict is linked into
	link_t areagrid[MAX_ENT_AREAS];

	SyncEntityState olds; // state in the last sent frame snap

	int movetype;

	int64_t freetime;          // time when the object was freed

	int numEvents;
	bool eventPriority[2];

	//
	// only used locally in game, not by server
	//

	StringHash classname;
	int spawnflags;

	int64_t nextThink;

	void ( *think )( edict_t *self );
	EdictTouchCallback touch;
	void ( *use )( edict_t *self, edict_t *other, edict_t *activator );
	void ( *pain )( edict_t *self, edict_t *other, float kick, int damage );
	void ( *die )( edict_t *self, edict_t *inflictor, edict_t *attacker, int assistorNo, DamageType damage_type, int damage );
	void ( *stop )( edict_t *self );

	StringHash name;
	StringHash target;
	StringHash killtarget;
	StringHash pathtarget;
	edict_t *target_ent;

	Vec3 velocity;
	Vec3 avelocity;

	float angle;                // set in qe3, -1 = up, -2 = down
	float speed;
	float accel, decel;         // usef for func_rotating

	int64_t timeStamp;
	int64_t deathTimeStamp;
	int timeDelta;              // SVF_PROJECTILE only. Used for 4D collision detection

	projectileinfo_t projectileInfo;    // specific for projectiles

	int dmg;

	const char *message;

	int mass;

	edict_t *movetarget;

	int64_t pain_debounce_time;

	float health;
	float max_health;
	int deadflag;

	int viewheight;				// height above origin where eyesight is determined
	int takedamage;

	int count;

	int64_t timeout;			// for SW and fat PG

	edict_t *chain;
	edict_t *enemy;
	edict_t *oldenemy;
	edict_t *activator;
	edict_t *groundentity;
	int groundentity_linkcount;
	StringHash sound;

	// timing variables
	float wait;
	float delay;                // before firing targets
	float random;

	int watertype;
	int waterlevel;

	int style;                  // also used as areaportal number

	// common data blocks
	moveinfo_t moveinfo;        // func movers movement

	snap_edict_t snap; // information that is cleared each frame snap

	edict_t *trigger_entity;
	int64_t trigger_timeout;

	bool linked;

	assistinfo_t recent_attackers[MAX_ASSIST_INFO];
	u32 num_bounces;
};

struct game_locals_t {
	edict_t edicts[ MAX_EDICTS ];
	gclient_t clients[ MAX_CLIENTS ];

	// store latched cvars here that we want to get at often
	int maxentities;
	int numentities;

	// cross level triggers
	int serverflags;

	unsigned int frametime;         // in milliseconds
	int snapFrameTime;              // in milliseconds
	int64_t prevServerTime;         // last frame's server time

	int numBots;
};

extern game_locals_t game;

#define world game.edicts

static inline int ENTNUM( const edict_t *x ) { return x - game.edicts; }
static inline int ENTNUM( const gclient_t *x ) { return x - game.clients + 1; }

static inline int PLAYERNUM( const edict_t *x ) { return x - game.edicts - 1; }
static inline int PLAYERNUM( const gclient_t *x ) { return x - game.clients; }

static inline edict_t *PLAYERENT( int x ) { return game.edicts + x + 1; }
