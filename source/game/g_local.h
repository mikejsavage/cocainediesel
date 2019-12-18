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
#include "gameshared/gs_public.h"
#include "game/g_public.h"
#include "game/g_syscalls.h"
#include "game/g_gametypes.h"
#include "game/g_ai.h"
#include "server/server.h"

#include "angelscript/angelscript.h"

//==================================================================

// FIXME: Medar: Remove the spectator test and just make sure they always have health
#define G_IsDead( ent )       ( ( !( ent )->r.client || ( ent )->s.team != TEAM_SPECTATOR ) && HEALTH_TO_INT( ( ent )->health ) <= 0 )

#define CLIENT_RESPAWN_FREEZE_DELAY 300

// edict->flags
#define FL_FLY              0x00000001
#define FL_SWIM             0x00000002  // implied immunity to drowining
#define FL_IMMUNE_LASER     0x00000004
#define FL_INWATER          0x00000008
#define FL_GODMODE          0x00000010
#define FL_NOTARGET         0x00000020
#define FL_IMMUNE_SLIME     0x00000040
#define FL_IMMUNE_LAVA      0x00000080
#define FL_PARTIALGROUND    0x00000100  // not all corners are valid
#define FL_WATERJUMP        0x00000200  // player jumping out of water
#define FL_TEAMSLAVE        0x00000400  // not the first on the team
#define FL_NO_KNOCKBACK     0x00000800

#define FRAMETIME ( (float)game.frametime * 0.001f )

#define BODY_QUEUE_SIZE     8
#define MAX_FLOOD_MESSAGES 32

typedef enum {
	DAMAGE_NO,
	DAMAGE_YES,     // will take damage if hit
	DAMAGE_AIM      // auto targeting recognizes this
} damage_t;

// deadflag
#define DEAD_NO         0
#define DEAD_DEAD       1

// edict->movetype values
typedef enum {
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
} movetype_t;

//
// this structure is left intact through an entire game
// it should be initialized at dll load time, and read/written to
// the server.ssv file for savegames
//
typedef struct {
	edict_t *edicts;        // [maxentities]
	gclient_t *clients;     // [maxclients]

	// store latched cvars here that we want to get at often
	int maxentities;
	int numentities;

	// cross level triggers
	int serverflags;

	// AngelScript engine object
	struct angelwrap_api_s *asExport;
	asIScriptEngine *asEngine;

	unsigned int frametime;         // in milliseconds
	int snapFrameTime;              // in milliseconds
	int64_t prevServerTime;         // last frame's server time

	int numBots;
} game_locals_t;

#define TIMEOUT_TIME                    180000
#define TIMEIN_TIME                     5000

typedef struct {
	int64_t time;
	int64_t endtime;
	int caller;
	int used[MAX_CLIENTS];
} timeout_t;

//
// this structure is cleared as each map is entered
//
typedef struct {
	int64_t framenum;
	int64_t time; // time in milliseconds
	int64_t spawnedTimeStamp; // time when map was restarted
	int64_t finalMatchDuration;

	char mapname[MAX_CONFIGSTRING_CHARS];
	char callvote_map[MAX_CONFIGSTRING_CHARS];
	char autorecord_name[128];

	// backup entities string
	char *mapString;
	size_t mapStrlen;

	// string used for parsing entities
	char *map_parsed_ents;      // string used for storing parsed key values
	size_t map_parsed_len;

	bool canSpawnEntities; // security check to prevent entities being spawned before map entities

	// intermission state
	bool exitNow;
	bool hardReset;

	// gametype definition and execution
	gametype_descriptor_t gametype;

	bool teamlock;
	bool ready[MAX_CLIENTS];
	bool forceStart;    // force starting the game, when warmup timelimit is up
	bool forceExit;     // just exit, ignore extended time checks

	edict_t *current_entity;    // entity running from G_RunFrame
	edict_t *spawning_entity;   // entity being spawned from G_InitLevel
	int body_que;               // dead bodies

	timeout_t timeout;
	float gravity;
} level_locals_t;


// spawn_temp_t is only used to hold entity field values that
// can be set from the editor, but aren't actualy present
// in edict_t during gameplay
typedef struct {
	int lip;
	int distance;
	int height;
	float radius;
	const char *noise;
	const char *noise_start;
	const char *noise_stop;
	float pausetime;
	const char *gravity;
	const char *debris1, *debris2;

	int gameteam;

	int size;

	int rgba;
} spawn_temp_t;

extern game_locals_t game;
extern gs_state_t server_gs;
extern level_locals_t level;
extern spawn_temp_t st;

extern mempool_t *gamepool;

extern int meansOfDeath;
extern vec3_t knockbackOfDeath;

#define FOFS( x ) offsetof( edict_t,x )
#define STOFS( x ) offsetof( spawn_temp_t,x )

extern cvar_t *password;
extern cvar_t *g_operator_password;
extern cvar_t *developer;

extern cvar_t *filterban;

extern cvar_t *g_gravity;
extern cvar_t *g_maxvelocity;

extern cvar_t *sv_cheats;

extern cvar_t *g_floodprotection_messages;
extern cvar_t *g_floodprotection_team;
extern cvar_t *g_floodprotection_seconds;
extern cvar_t *g_floodprotection_penalty;

extern cvar_t *g_inactivity_maxtime;

extern cvar_t *g_maplist;
extern cvar_t *g_maprotation;

extern cvar_t *g_enforce_map_pool;
extern cvar_t *g_map_pool;

extern cvar_t *g_scorelimit;

extern cvar_t *g_projectile_prestep;
extern cvar_t *g_numbots;
extern cvar_t *g_maxtimeouts;

extern cvar_t *g_self_knockback;
extern cvar_t *g_knockback_scale;
extern cvar_t *g_respawn_delay_min;
extern cvar_t *g_respawn_delay_max;
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

edict_t **G_Teams_ChallengersQueue( void );
void G_Teams_Join_Cmd( edict_t *ent );
bool G_Teams_JoinTeam( edict_t *ent, int team );
bool G_Teams_TeamIsLocked( int team );
bool G_Teams_LockTeam( int team );
bool G_Teams_UnLockTeam( int team );
void G_Teams_UpdateMembersList( void );
bool G_Teams_JoinAnyTeam( edict_t *ent, bool silent );
void G_Teams_SetTeam( edict_t *ent, int team );

void Cmd_Say_f( edict_t *ent, bool arg0, bool checkflood );
void G_Say_Team( edict_t *who, const char *inmsg, bool checkflood );

void G_Match_Ready( edict_t *ent );
void G_Match_NotReady( edict_t *ent );
void G_Match_ToggleReady( edict_t *ent );
void G_Match_CheckReadys( void );
void G_EndMatch( void );

void G_Teams_JoinChallengersQueue( edict_t *ent );
void G_Teams_LeaveChallengersQueue( edict_t *ent );
void G_InitChallengersQueue( void );

void G_Gametype_Init( void );
void G_Gametype_ScoreEvent( gclient_t *client, const char *score_event, const char *args );
void G_RunGametype( void );

//
// g_spawnpoints.c
//
enum {
	SPAWNSYSTEM_INSTANT,
	SPAWNSYSTEM_WAVES,
	SPAWNSYSTEM_HOLD,

	SPAWNSYSTEM_TOTAL
};

void G_SpawnQueue_Init( void );
void G_SpawnQueue_SetTeamSpawnsystem( int team, int spawnsystem, int wave_time, int wave_maxcount, bool spectate_team );
int G_SpawnQueue_NextRespawnTime( int team );
void G_SpawnQueue_ResetTeamQueue( int team );
int G_SpawnQueue_GetSystem( int team );
void G_SpawnQueue_ReleaseTeamQueue( int team );
void G_SpawnQueue_AddClient( edict_t *ent );
void G_SpawnQueue_RemoveClient( edict_t *ent );
void G_SpawnQueue_Think( void );

void SelectSpawnPoint( edict_t *ent, edict_t **spawnpoint, vec3_t origin, vec3_t angles );
void SP_info_player_start( edict_t *ent );
void SP_info_player_deathmatch( edict_t *ent );
void SP_post_match_camera( edict_t *ent );

//
// g_func.c
//
void G_AssignMoverSounds( edict_t *ent, const char *start, const char *move, const char *stop );

void SP_func_plat( edict_t *ent );
void SP_func_rotating( edict_t *ent );
void SP_func_button( edict_t *ent );
void SP_func_door( edict_t *ent );
void SP_func_door_rotating( edict_t *ent );
void SP_func_train( edict_t *ent );
void SP_func_timer( edict_t *self );
void SP_func_wall( edict_t *self );
void SP_func_object( edict_t *self );
void SP_func_explosive( edict_t *self );
void SP_func_static( edict_t *ent );

//
// g_gladiator
//
void SP_spikes( edict_t *ent );

//
// g_ascript.c
//
bool GT_asLoadScript( const char *gametypeName );
void GT_asShutdownScript( void );
void GT_asCallSpawn( void );
void GT_asCallMatchStateStarted( void );
bool GT_asCallMatchStateFinished( int incomingMatchState );
void GT_asCallThinkRules( void );
void GT_asCallPlayerKilled( edict_t *targ, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point, int mod );
void GT_asCallPlayerRespawn( edict_t *ent, int old_team, int new_team );
void GT_asCallScoreEvent( gclient_t *client, const char *score_event, const char *args );
void GT_asCallScoreboardMessage( char * buf, size_t buf_size );
edict_t *GT_asCallSelectSpawnPoint( edict_t *ent );
bool GT_asCallGameCommand( gclient_t *client, const char *cmd, const char *args, int argc );
bool GT_asCallBotStatus( edict_t *ent );
void GT_asCallShutdown( void );

void G_asCallMapEntityThink( edict_t *ent );
void G_asCallMapEntityTouch( edict_t *ent, edict_t *other, cplane_t *plane, int surfFlags );
void G_asCallMapEntityUse( edict_t *ent, edict_t *other, edict_t *activator );
void G_asCallMapEntityPain( edict_t *ent, edict_t *other, float kick, float damage );
void G_asCallMapEntityDie( edict_t *ent, edict_t *inflicter, edict_t *attacker, int damage, const vec3_t point );
void G_asCallMapEntityStop( edict_t *ent );
void G_asResetEntityBehaviors( edict_t *ent );
void G_asClearEntityBehaviors( edict_t *ent );
void G_asReleaseEntityBehaviors( edict_t *ent );

bool G_asCallMapEntitySpawnScript( const char *classname, edict_t *ent );

void G_asInitGameModuleEngine( void );
void G_asShutdownGameModuleEngine( void );
void G_asGarbageCollect( bool force );
void G_asDumpAPI_f( void );

#define world game.edicts

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

char *G_StatsMessage( edict_t *ent );
bool CheckFlood( edict_t *ent, bool teamonly );
void G_InitGameCommands( void );
void G_PrecacheGameCommands( void );
void G_AddCommand( const char *name, gamecommandfunc_t cmdfunc );

//
// g_items.c
//
void PrecacheItem( const gsitem_t *it );
void G_PrecacheItems( void );
void G_FireWeapon( edict_t *ent, int parm );
const gsitem_t *GetItemByTag( int tag );
bool Add_Ammo( gclient_t *client, const gsitem_t *item, int count, bool add_it );
bool G_PickupItem( edict_t *other, const gsitem_t *it, int flags, int count );
void G_UseItem( struct edict_s *ent, const gsitem_t *item );

//
// g_utils.c
//
#define G_LEVELPOOL_BASE_SIZE   45 * 1024 * 1024

bool KillBox( edict_t *ent, int mod, const vec3_t knockback );
float LookAtKillerYAW( edict_t *self, edict_t *inflictor, edict_t *attacker );
edict_t *G_Find( edict_t *from, size_t fieldofs, const char *match );
edict_t *G_PickTarget( const char *targetname );
void G_UseTargets( edict_t *ent, edict_t *activator );
void G_SetMovedir( vec3_t angles, vec3_t movedir );
void G_InitMover( edict_t *ent );
void G_DropSpawnpointToFloor( edict_t *ent );

void G_InitEdict( edict_t *e );
edict_t *G_Spawn( void );
void G_FreeEdict( edict_t *e );

void G_LevelInitPool( size_t size );
void G_LevelFreePool( void );
void *_G_LevelMalloc( size_t size, const char *filename, int fileline );
void _G_LevelFree( void *data, const char *filename, int fileline );
char *_G_LevelCopyString( const char *in, const char *filename, int fileline );

void G_StringPoolInit( void );
const char *_G_RegisterLevelString( const char *string, const char *filename, int fileline );
#define G_RegisterLevelString( in ) _G_RegisterLevelString( in, __FILE__, __LINE__ )

char *G_AllocCreateNamesList( const char *path, const char *extension, const char separator );

char *_G_CopyString( const char *in, const char *filename, int fileline );
#define G_CopyString( in ) _G_CopyString( in, __FILE__, __LINE__ )

void G_AddEvent( edict_t *ent, int event, int parm, bool highPriority );
edict_t *G_SpawnEvent( int event, int parm, vec3_t origin );
void G_MorphEntityIntoEvent( edict_t *ent, int event, int parm );

void G_CallThink( edict_t *ent );
void G_CallTouch( edict_t *self, edict_t *other, cplane_t *plane, int surfFlags );
void G_CallUse( edict_t *self, edict_t *other, edict_t *activator );
void G_CallStop( edict_t *self );
void G_CallPain( edict_t *ent, edict_t *attacker, float kick, float damage );
void G_CallDie( edict_t *ent, edict_t *inflictor, edict_t *attacker, int damage, const vec3_t point );

#ifndef _MSC_VER
void G_PrintMsg( edict_t *ent, const char *format, ... ) __attribute__( ( format( printf, 2, 3 ) ) );
void G_ChatMsg( edict_t *ent, edict_t *who, bool teamonly, const char *format, ... ) __attribute__( ( format( printf, 4, 5 ) ) );
void G_CenterPrintMsg( edict_t *ent, const char *format, ... ) __attribute__( ( format( printf, 2, 3 ) ) );
#else
void G_PrintMsg( edict_t *ent, _Printf_format_string_ const char *format, ... );
void G_ChatMsg( edict_t *ent, edict_t *who, bool teamonly, _Printf_format_string_ const char *format, ... );
void G_CenterPrintMsg( edict_t *ent, _Printf_format_string_ const char *format, ... );
#endif

void G_Obituary( edict_t *victim, edict_t *attacker, int mod );

edict_t *G_Sound( edict_t *owner, int channel, int soundindex, float attenuation );
edict_t *G_PositionedSound( vec3_t origin, int channel, int soundindex, float attenuation );
void G_GlobalSound( int channel, int soundindex );
void G_LocalSound( edict_t *owner, int channel, int soundindex );

#define G_ISGHOSTING( x ) ( ( ( x )->s.modelindex == 0 ) && ( ( x )->r.solid == SOLID_NOT ) )
#define ISBRUSHMODEL( x ) ( ( ( x > 0 ) && ( (int)x < trap_CM_NumInlineModels() ) ) ? true : false )

void G_TeleportEffect( edict_t *ent, bool in );
void G_RespawnEffect( edict_t *ent );
int G_SolidMaskForEnt( edict_t *ent );
void G_CheckGround( edict_t *ent );
void G_CategorizePosition( edict_t *ent );
void G_ReleaseClientPSEvent( gclient_t *client );
void G_AddPlayerStateEvent( gclient_t *client, int event, int parm );
void G_ClearPlayerStateEvents( gclient_t *client );

// announcer events
void G_AnnouncerSound( edict_t *targ, int soundindex, int team, bool queued, edict_t *ignore );
edict_t *G_PlayerForText( const char *text );

void G_PrecacheWeapondef( int weapon, firedef_t *firedef );

void G_SetBoundsForSpanEntity( edict_t *ent, vec_t size );

//
// g_callvotes.c
//
void G_CallVotes_Init( void );
void G_FreeCallvotes( void );
void G_CallVotes_ResetClient( int n );
void G_CallVotes_CmdVote( edict_t *ent );
void G_CallVotes_Think( void );
bool G_Callvotes_HasVoted( edict_t *ent );
void G_CallVote_Cmd( edict_t *ent );
void G_OperatorVote_Cmd( edict_t *ent );
void G_RegisterGametypeScriptCallvote( const char *name, const char *usage, const char *type, const char *help );

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
void SP_trigger_elevator( edict_t *ent );
void SP_trigger_gravity( edict_t *ent );

//
// g_clip.c
//
#define MAX_ENT_AREAS 16

typedef struct link_s {
	struct link_s *prev, *next;
	int entNum;
} link_t;

int G_PointContents( const vec3_t p );
void G_Trace( trace_t *tr, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, edict_t *passedict, int contentmask );
int G_PointContents4D( const vec3_t p, int timeDelta );
void G_Trace4D( trace_t *tr, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, edict_t *passedict, int contentmask, int timeDelta );
void GClip_BackUpCollisionFrame( void );
int GClip_FindInRadius4D( vec3_t org, float rad, int *list, int maxcount, int timeDelta );
void G_SplashFrac4D( const edict_t *ent, vec3_t hitpoint, float maxradius, vec3_t pushdir, float *frac, int timeDelta, bool selfdamage );
void GClip_ClearWorld( void );
void GClip_SetBrushModel( edict_t *ent, const char *name );
void GClip_SetAreaPortalState( edict_t *ent, bool open );
void GClip_LinkEntity( edict_t *ent );
void GClip_UnlinkEntity( edict_t *ent );
void GClip_TouchTriggers( edict_t *ent );
void G_PMoveTouchTriggers( pmove_t *pm, vec3_t previous_origin );
entity_state_t *G_GetEntityStateForDeltaTime( int entNum, int deltaTime );
int GClip_FindInRadius( vec3_t org, float rad, int *list, int maxcount );

// BoxEdicts() can return a list of either solid or trigger entities
// FIXME: eliminate AREA_ distinction?
#define AREA_ALL       -1
#define AREA_SOLID      1
#define AREA_TRIGGERS   2
int GClip_AreaEdicts( const vec3_t mins, const vec3_t maxs, int *list, int maxcount, int areatype, int timeDelta );
bool GClip_EntityContact( const vec3_t mins, const vec3_t maxs, edict_t *ent );

//
// g_combat.c
//
bool G_IsTeamDamage( entity_state_t *targ, entity_state_t *attacker );
void G_Killed( edict_t *targ, edict_t *inflictor, edict_t *attacker, int damage, const vec3_t point, int mod );
int G_ModToAmmo( int mod );
void G_SplashFrac( const entity_state_t *s, const entity_shared_t *r, const vec3_t point, float maxradius, vec3_t pushdir, float *frac, bool selfdamage );
void G_Damage( edict_t *targ, edict_t *inflictor, edict_t *attacker, const vec3_t pushdir, const vec3_t dmgdir, const vec3_t point, float damage, float knockback, int dflags, int mod );
void G_RadiusDamage( edict_t *inflictor, edict_t *attacker, cplane_t *plane, edict_t *ignore, int mod );

// damage flags
#define DAMAGE_RADIUS 0x00000001  // damage was indirect
#define DAMAGE_NO_PROTECTION 0x00000002
#define DAMAGE_KNOCKBACK_SOFT 0x00000040

//
// g_misc.c
//
void ThrowSmallPileOfGibs( edict_t *self, const vec3_t knockback, int damage );

void BecomeExplosion1( edict_t *self );

void SP_path_corner( edict_t *self );

void SP_misc_model( edict_t *ent );

void SP_model( edict_t *ent );

//
// g_weapon.c
//
void W_Fire_Blade( edict_t *self, int range, vec3_t start, vec3_t angles, float damage, int knockback, int timeDelta );
void W_Fire_MG( edict_t *self, vec3_t start, vec3_t angles, int seed, int range, int hspread, int vspread, float damage, int knockback, int timeDelta );
void W_Fire_Riotgun( edict_t *self, vec3_t start, vec3_t angles, int range, int hspread, int vspread, int count, float damage, int knockback, int timeDelta );
edict_t *W_Fire_Grenade( edict_t *self, vec3_t start, vec3_t angles, int speed, float damage, int minKnockback, int maxKnockback, int minDamage, float radius, int timeout, int timeDelta, bool aim_up );
edict_t *W_Fire_Rocket( edict_t *self, vec3_t start, vec3_t angles, int speed, float damage, int minKnockback, int maxKnockback, int minDamage, int radius, int timeout, int timeDelta );
edict_t *W_Fire_Plasma( edict_t *self, vec3_t start, vec3_t angles, float damage, int minKnockback, int maxKnockback, int minDamage, int radius, int speed, int timeout, int timeDelta );
void W_Fire_Electrobolt_FullInstant( edict_t *self, vec3_t start, vec3_t angles, float maxdamage, float mindamage, int maxknockback, int minknockback, int range, int minDamageRange, int timeDelta );
edict_t *W_Fire_Lasergun( edict_t *self, vec3_t start, vec3_t angles, float damage, int knockback, int timeout, int timeDelta );

void Use_Weapon( edict_t *ent, const gsitem_t *item );

//
// g_chasecam	//newgametypes
//
void G_SpectatorMode( edict_t *ent );
void G_ChasePlayer( edict_t *ent, const char *name, bool teamonly, int followmode );
void G_ChaseStep( edict_t *ent, int step );
void Cmd_SwitchChaseCamMode_f( edict_t *ent );
void Cmd_ChaseCam_f( edict_t *ent );
void Cmd_Spec_f( edict_t *ent );
void G_EndServerFrames_UpdateChaseCam( void );

//
// g_client.c
//
void G_InitBodyQueue( void );
void ClientUserinfoChanged( edict_t *ent, char *userinfo );
void G_Client_UpdateActivity( gclient_t *client );
void G_Client_InactivityRemove( gclient_t *client );
void G_ClientRespawn( edict_t *self, bool ghost );
void G_ClientClearStats( edict_t *ent );
void G_GhostClient( edict_t *self );
bool ClientMultiviewChanged( edict_t *ent, bool multiview );
void ClientThink( edict_t *ent, usercmd_t *cmd, int timeDelta );
void G_ClientThink( edict_t *ent );
void G_CheckClientRespawnClick( edict_t *ent );
bool ClientConnect( edict_t *ent, char *userinfo, bool fakeClient );
void ClientDisconnect( edict_t *ent, const char *reason );
void ClientBegin( edict_t *ent );
void ClientCommand( edict_t *ent );
void G_PredictedEvent( int entNum, int ev, int parm );
void G_TeleportPlayer( edict_t *player, edict_t *dest );
bool G_PlayerCanTeleport( edict_t *player );

//
// g_player.c
//
void player_pain( edict_t *self, edict_t *other, float kick, int damage );
void player_die( edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, const vec3_t point );
void player_think( edict_t *self );

//
// g_target.c
//
void target_laser_start( edict_t *self );

void SP_target_temp_entity( edict_t *ent );
void SP_target_explosion( edict_t *ent );
void SP_target_laser( edict_t *self );
void SP_target_position( edict_t *self );
void SP_target_print( edict_t *self );
void SP_target_delay( edict_t *ent );
void SP_target_teleporter( edict_t *self );

//
// g_svcmds.c
//
void SV_ResetPacketFiltersTimeouts( void );
bool SV_FilterPacket( char *from );
void G_AddServerCommands( void );
void G_RemoveCommands( void );
void SV_ReadIPList( void );
void SV_WriteIPList( void );

//
// p_view.c
//
void G_ClientEndSnapFrame( edict_t *ent );
void G_ClientAddDamageIndicatorImpact( gclient_t *client, int damage, const vec3_t dir );
void G_ClientDamageFeedback( edict_t *ent );

//
// p_hud.c
//

constexpr unsigned int scoreboardInterval = 1000;

void G_SetClientStats( edict_t *ent );
void G_Snap_UpdateWeaponListMessages( void );
void G_ScoreboardMessage_AddSpectators( void );
void G_UpdateScoreBoardMessages( void );

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

#define G_LevelMalloc( size ) _G_LevelMalloc( ( size ), __FILE__, __LINE__ )
#define G_LevelFree( data ) _G_LevelFree( ( data ), __FILE__, __LINE__ )
#define G_LevelCopyString( in ) _G_LevelCopyString( ( in ), __FILE__, __LINE__ )

#ifndef _MSC_VER
void G_Error( const char *format, ... ) __attribute__( ( format( printf, 1, 2 ) ) ) __attribute__( ( noreturn ) );
void G_Printf( const char *format, ... ) __attribute__( ( format( printf, 1, 2 ) ) );
#else
__declspec( noreturn ) void G_Error( _Printf_format_string_ const char *format, ... );
void G_Printf( _Printf_format_string_ const char *format, ... );
#endif

void G_Init( unsigned int seed, unsigned int framemsec );
void G_Shutdown( void );
void G_ExitLevel( void );
game_state_t *G_GetGameState( void );
void G_GamestatSetFlag( int flag, bool b );
void G_Timeout_Reset( void );

//
// g_frame.c
//
void G_CheckCvars( void );
void G_RunFrame( unsigned int msec );
void G_SnapClients( void );
void G_ClearSnap( void );
void G_SnapFrame( void );


//
// g_spawn.c
//
bool G_CallSpawn( edict_t *ent );
void G_RespawnLevel( void );
void G_ResetLevel( void );
void G_InitLevel( char *mapname, char *entities, int entstrlen, int64_t levelTime );
const char *G_GetEntitySpawnKey( const char *key, edict_t *self );

//
// g_awards.c
//
void G_PlayerAward( edict_t *ent, const char *awardMsg );
void G_AwardRaceRecord( edict_t *self );

//============================================================================

typedef struct {
	int radius;
	float minDamage;
	float maxDamage;
	float minKnockback;
	float maxKnockback;
} projectileinfo_t;

typedef struct {
	bool active;
	int target;
	int mode;                   //3rd or 1st person
	int range;
	bool teamonly;
	int64_t timeout;           //delay after loosing target
	int followmode;
} chasecam_t;

typedef struct {
	// fixed data
	vec3_t start_origin;
	vec3_t start_angles;
	vec3_t end_origin;
	vec3_t end_angles;

	int sound_start;
	int sound_middle;
	int sound_end;

	vec3_t movedir;  // direction defined in the bsp

	float speed;
	float distance;    // used by binary movers

	float wait;

	// state data
	int state;
	vec3_t dir;             // used by func_bobbing and func_pendulum
	float current_speed;    // used by func_rotating

	void ( *endfunc )( edict_t * );
	void ( *blocked )( edict_t *self, edict_t *other );

	vec3_t dest;
	vec3_t destangles;
} moveinfo_t;

#define MAX_CLIENT_EVENTS   16
#define MAX_CLIENT_EVENTS_MASK ( MAX_CLIENT_EVENTS - 1 )

#define G_MAX_TIME_DELTAS   8
#define G_MAX_TIME_DELTAS_MASK ( G_MAX_TIME_DELTAS - 1 )

typedef struct {
	int buttons;
	uint8_t plrkeys; // used for displaying key icons
	int damageTaken;
	vec3_t damageTakenDir;
} client_snapreset_t;

typedef struct {
	client_snapreset_t snap;
	chasecam_t chase;

	int64_t timeStamp; // last time it was reset

	// player_state_t event
	int events[MAX_CLIENT_EVENTS];
	unsigned int eventsCurrent;
	unsigned int eventsHead;

	int old_waterlevel;
	int old_watertype;
} client_respawnreset_t;

typedef struct {
	int64_t timeStamp;				// last time it was reset

	int64_t last_vsay;				// time when last vsay was said
	int64_t last_activity;

	score_stats_t stats;
	bool showscores;
	int64_t scoreboard_time;		// when scoreboard was last sent

	// flood protection
	int64_t flood_locktill;			// locked from talking
	int64_t flood_when[MAX_FLOOD_MESSAGES];        // when messages were said
	int flood_whenhead;             // head pointer for when said
	// team only
	int64_t flood_team_when[MAX_FLOOD_MESSAGES];   // when messages were said
	int flood_team_whenhead;        // head pointer for when said

	int64_t callvote_when;
} client_levelreset_t;

typedef struct {
	int64_t timeStamp; // last time it was reset

	// for position command
	bool position_saved;
	vec3_t position_origin;
	vec3_t position_angles;
	int64_t position_lastcmd;

	edict_t *last_killer;
} client_teamreset_t;

struct gclient_s {
	// known to server
	player_state_t ps;          // communicated by server to clients
	client_shared_t r;

	// DO NOT MODIFY ANYTHING ABOVE THIS, THE SERVER
	// EXPECTS THE FIELDS IN THAT ORDER!

	//================================

	/*
	// Review notes
	- Revise the calls to G_ClearPlayerStateEvents, they may be useless now
	- self->ai.pers.netname for what? there is self->r.client->netname already
	- CTF prints personal bonuses in global console. I don't think this is worth it
	*/

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
	bool multiview;

	int team;
	int hand;
	int handicap;
	bool isoperator;
	int64_t queueTimeStamp;
	int muted;     // & 1 = chat disabled, & 2 = vsay disabled

	usercmd_t ucmd;
	int timeDelta;              // time offset to adjust for shots collision (antilag)
	int timeDeltas[G_MAX_TIME_DELTAS];
	int timeDeltasHead;

	pmove_state_t old_pmove;    // for detecting out-of-pmove changes

	int asRefCount, asFactored;
};

typedef struct snap_edict_s {
	// whether we have killed anyone this snap
	bool kill;
	bool teamkill;

	// ents can accumulate damage along the frame, so they spawn less events
	float damage_taken;
	float damage_saved;
	vec3_t damage_dir;
	vec3_t damage_at;
	float damage_given;             // for hitsounds
	float damageteam_given;
} snap_edict_t;

struct edict_s {
	entity_state_t s;
	entity_shared_t r;

	// DO NOT MODIFY ANYTHING ABOVE THIS, THE SERVER
	// EXPECTS THE FIELDS IN THAT ORDER!

	//================================

	int linkcount;

	// physics grid areas this edict is linked into
	link_t areagrid[MAX_ENT_AREAS];

	entity_state_t olds; // state in the last sent frame snap

	int movetype;
	int flags;

	const char *model;
	const char *model2;
	int64_t freetime;          // time when the object was freed

	int numEvents;
	bool eventPriority[2];

	//
	// only used locally in game, not by server
	//

	const char *classname;
	const char *spawnString;            // keep track of string definition of this entity
	int spawnflags;

	int64_t nextThink;

	void ( *think )( edict_t *self );
	void ( *touch )( edict_t *self, edict_t *other, cplane_t *plane, int surfFlags );
	void ( *use )( edict_t *self, edict_t *other, edict_t *activator );
	void ( *pain )( edict_t *self, edict_t *other, float kick, int damage );
	void ( *die )( edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, const vec3_t point );
	void ( *stop )( edict_t *self );

	const char *target;
	const char *targetname;
	const char *killtarget;
	const char *team;
	const char *pathtarget;
	edict_t *target_ent;

	vec3_t velocity;
	vec3_t avelocity;

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
	float gravity;              // per entity gravity multiplier (1.0 is normal) // use for lowgrav artifact, flares

	edict_t *movetarget;

	int64_t pain_debounce_time;

	float health;
	int max_health;
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
	edict_t *teamchain;
	edict_t *teammaster;
	int noise_index;
	float attenuation;

	// timing variables
	float wait;
	float delay;                // before firing targets
	float random;

	int watertype;
	int waterlevel;

	int style;                  // also used as areaportal number

	float light;
	vec3_t color;

	const gsitem_t *item;       // for bonus items

	// common data blocks
	moveinfo_t moveinfo;        // func movers movement

	snap_edict_t snap; // information that is cleared each frame snap

	edict_t *trigger_entity;
	int64_t trigger_timeout;

	bool linked;

	bool scriptSpawned;
	asIScriptModule *asScriptModule;
	asIScriptFunction *asSpawnFunc, *asThinkFunc, *asUseFunc, *asTouchFunc, *asPainFunc, *asDieFunc, *asStopFunc;
};

static inline int ENTNUM( const edict_t *x ) { return x - game.edicts; }
static inline int ENTNUM( const gclient_t *x ) { return x - game.clients + 1; }

static inline int PLAYERNUM( const edict_t *x ) { return x - game.edicts - 1; }
static inline int PLAYERNUM( const gclient_t *x ) { return x - game.clients; }

static inline edict_t *PLAYERENT( int x ) { return game.edicts + x + 1; }
