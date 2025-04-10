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
#include "gameshared/collision.h"
#include "game/g_public.h"
#include "game/g_gametypes.h"
#include "game/g_ai.h"
#include "game/g_maps.h"
#include "server/server.h"

//==================================================================

// FIXME: Medar: Remove the spectator test and just make sure they always have health
#define G_IsDead( ent )       ( ( !( ent )->r.client || ( ent )->s.team != Team_None ) && HEALTH_TO_INT( ( ent )->health ) <= 0 )

#define FRAMETIME ( (float)game.frametime * 0.001f )

#define MAX_FLOOD_MESSAGES 32

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
	Team caller;
	int used[Team_Count];
};

//
// this structure is cleared as each map is entered
//
struct level_locals_t {
	int64_t time; // time in milliseconds
	Time spawnedTimeStamp; // time when map was restarted
	int64_t finalMatchDuration;

	char callvote_map[128];

	bool canSpawnEntities; // security check to prevent entities being spawned before map entities

	bool exitNow;

	GametypeDef gametype;

	bool ready[MAX_CLIENTS];

	edict_t * current_entity;    // entity running from G_RunFrame

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
	StringHash noise;
	StringHash noise_start;
	StringHash noise_stop;
	float pausetime;
	int gameteam;
	int size;
	float spawn_probability;
	float power;
	StringHash weapon;
};

struct score_stats_t {
	int kills;
	int deaths;
	int suicides;

	int score;
	bool ready;

	int total_damage_given;
};

extern gs_state_t server_gs;
extern level_locals_t level;

extern Vec3 knockbackOfDeath;
extern int damageFlagsOfDeath;

extern Cvar *sv_password;

extern Cvar *g_maxvelocity;

extern Cvar *sv_cheats;

extern Cvar *g_floodprotection_messages;
extern Cvar *g_floodprotection_team;
extern Cvar *g_floodprotection_seconds;
extern Cvar *g_floodprotection_penalty;

extern Cvar *g_inactivity_maxtime;

extern Cvar *g_projectile_prestep;
extern Cvar *g_numbots;
extern Cvar *g_maxtimeouts;

extern Cvar *g_antilag_timenudge;
extern Cvar *g_antilag_maxtimedelta;

extern Cvar *g_teams_maxplayers;
extern Cvar *g_teams_allow_uneven;

extern Cvar *g_autorecord;
extern Cvar *g_autorecord_maxdemos;
extern Cvar *g_allow_spectator_voting;

void G_Match_Ready( edict_t * ent );
void G_Match_NotReady( edict_t * ent );
void G_Match_ToggleReady( edict_t * ent );
void G_Match_CheckReadys();
void G_EndMatch();

//
// g_spawnpoints.c
//
void DropSpawnToFloor( edict_t * ent );
const edict_t * SelectSpawnPoint( const edict_t * ent );
void SP_post_match_camera( edict_t * ent, const spawn_temp_t * st );

// g_teams

void G_Teams_Join_Cmd( edict_t * ent, msg_t args );
bool G_Teams_JoinTeam( edict_t * ent, Team team );
void G_Teams_UpdateMembersList();
bool G_Teams_JoinAnyTeam( edict_t * ent, bool silent );
void G_Teams_SetTeam( edict_t * ent, Team team );

u8 PlayersAliveOnTeam( Team team );

void GhostEveryone();

struct RespawnQueues {
	struct Queue {
		int players[ MAX_CLIENTS ];
		size_t n;
	};

	Queue teams[ Team_Count ];
};

void InitRespawnQueues( RespawnQueues * queues );
void RemovePlayerFromRespawnQueues( RespawnQueues * queues, int player );
void RemoveDisconnectedPlayersFromRespawnQueues( RespawnQueues * queues );
void EnqueueRespawn( RespawnQueues * queues, Team team, int player );
void SpawnTeams( RespawnQueues * queues );

//
// g_func.c
//
void SP_func_rotating( edict_t * ent, const spawn_temp_t * st );
void SP_func_door( edict_t * ent, const spawn_temp_t * st );
void SP_func_train( edict_t * ent, const spawn_temp_t * st );

//
// g_spikes
//
void SP_spike( edict_t * ent, const spawn_temp_t * st );
void SP_spikes( edict_t * ent, const spawn_temp_t * st );

//
// g_shooter
//
void SP_shooter( edict_t * ent, const spawn_temp_t * st );

//
// g_speakers
//

void SP_speaker_wall( edict_t * ent, const spawn_temp_t * st );

//
// g_jumppad
//

void SP_jumppad( edict_t * ent, const spawn_temp_t * st );

//
// g_cinematic
//

void SP_cinematic_mapname( edict_t * ent, const spawn_temp_t * st );

//
// g_cmds.c
//

typedef void ( *gamecommandfunc_t )( edict_t * ent, msg_t args );

bool CheckFlood( edict_t * ent, bool teamonly );
void G_InitGameCommands();
void G_AddCommand( ClientCommandType command, gamecommandfunc_t cmdfunc );

//
// g_utils.c
//

EntityID NewEntity();
void ResetEntityIDSequence();
edict_t * GetEntity( EntityID id );

void KillBox( edict_t * ent, DamageType damage_type, Vec3 knockback );
float LookAtKillerYAW( edict_t * self, edict_t * inflictor, edict_t * attacker );
edict_t * G_Find( edict_t * cursor, const StringHash edict_t::* field, StringHash value );
edict_t * G_PickRandomEnt( StringHash edict_t::* field, StringHash value );
edict_t * G_PickTarget( StringHash name );
void G_UseTargets( edict_t * ent, edict_t * activator );
void G_SetMovedir( EulerDegrees3 * angles, Vec3 * movedir );
void G_InitMover( edict_t * ent );

void G_InitEdict( edict_t * e );
edict_t * G_Spawn();
void G_FreeEdict( edict_t * e );

void G_AddEvent( edict_t * ent, int event, u64 parm, bool highPriority );
edict_t * G_SpawnEvent( int event, u64 parm, const Vec3 * origin );

void G_CallThink( edict_t * ent );
void G_CallTouch( edict_t * self, edict_t * other, Vec3 normal, SolidBits solid_mask );
void G_CallUse( edict_t * self, edict_t * other, edict_t * activator );
void G_CallStop( edict_t * self );
void G_CallPain( edict_t * ent, edict_t * attacker, float kick, float damage );
void G_CallDie( edict_t * ent, edict_t * inflictor, edict_t * attacker, int assistorNo, DamageType damage_type, int damage );

[[gnu::format( printf, 2, 3 )]] void G_PrintMsg( edict_t * ent, const char * format, ... );
void G_ChatMsg( edict_t * ent, const edict_t * who, bool teamonly, Span< const char > msg );
[[gnu::format( printf, 2, 3 )]] void G_CenterPrintMsg( edict_t * ent, const char * format, ... );
void G_ClearCenterPrint( edict_t * ent );

void G_DebugPrint( const char * format, ... );

edict_t * G_Sound( edict_t * owner, StringHash sound );
edict_t * G_PositionedSound( Vec3 origin, StringHash sound );
void G_GlobalSound( StringHash sound );
void G_LocalSound( edict_t * owner, StringHash sound );

void G_TeleportEffect( edict_t * ent, bool in );
void G_RespawnEffect( edict_t * ent );
void G_CheckGround( edict_t * ent );
void G_ReleaseClientPSEvent( gclient_t *client );
void G_AddPlayerStateEvent( gclient_t *client, int event, u64 parm );
void G_ClearPlayerStateEvents( gclient_t *client );

// announcer events
void G_AnnouncerSound( edict_t * targ, StringHash sound, Team team, bool queued, edict_t * ignore );
edict_t * G_PlayerForText( Span< const char > text );

void G_SunCycle( Time duration );

//
// g_callvotes.c
//
void G_CallVotes_Init();
void G_FreeCallvotes();
void G_CallVotes_ResetClient( int n );
void G_CallVotes_Think();
bool G_Callvotes_HasVoted( edict_t * ent );
void G_CallVote_Cmd( edict_t * ent, msg_t args );
void G_CallVotes_VoteYes( edict_t * ent );
void G_CallVotes_VoteNo( edict_t * ent );

//
// g_trigger.c
//
void SP_trigger_teleport( edict_t * ent, const spawn_temp_t * st );
void SP_trigger_push( edict_t * ent, const spawn_temp_t * st );
void SP_trigger_hurt( edict_t * ent, const spawn_temp_t * st );

void InitTrigger( edict_t * self );
bool G_TriggerWait( edict_t * ent );

//
// g_clip.c
//

trace_t G_Trace( Vec3 start, MinMax3 bounds, Vec3 end, const edict_t * passedict, SolidBits solid_mask );
trace_t G_Trace4D( Vec3 start, MinMax3 bounds, Vec3 end, const edict_t * passedict, SolidBits solid_mask, int timeDelta );
void GClip_BackUpCollisionFrame();
int GClip_FindInRadius4D( Vec3 org, float rad, int * list, size_t maxcount, int timeDelta );
void G_SplashFrac4D( const edict_t * ent, Vec3 hitpoint, float maxradius, Vec3 * pushdir, float *frac, int timeDelta );
void GClip_ClearWorld();
void GClip_LinkEntity( const edict_t * ent );
void GClip_UnlinkEntity( const edict_t * ent );
void GClip_TouchTriggers( edict_t * ent );
void G_PMoveTouchTriggers( const pmove_t * pm, Vec3 previous_origin );
int GClip_FindInRadius( Vec3 org, float rad, int * list, size_t maxcount );

bool IsHeadshot( int entNum, Vec3 hit, int timeDelta );

//
// g_combat.c
//
bool G_IsTeamDamage( const SyncEntityState * targ, const SyncEntityState * attacker );
void G_Killed( edict_t * targ, edict_t * inflictor, edict_t * attacker, int topAssistorNo, DamageType damage_type, int damage );
void G_SplashFrac( const SyncEntityState * s, const entity_shared_t * r, Vec3 point, float maxradius, Vec3 * pushdir, float * frac );
void G_Damage( edict_t * targ, edict_t * inflictor, edict_t * attacker, Vec3 pushdir, Vec3 dmgdir, Vec3 point, float damage, float knockback, int dflags, DamageType damage_type );
void SpawnDamageEvents( const edict_t * attacker, edict_t * victim, float damage, bool headshot, Vec3 pos, Vec3 dir, bool showNumbers );
void G_RadiusKnockback( float maxknockback, float minknockback, float radius, edict_t * attacker, Vec3 pos, Optional< Vec3 > normal, int timeDelta );
void G_RadiusDamage( edict_t * inflictor, edict_t * attacker, Optional< Vec3 > normal, edict_t * ignore, DamageType damage_type );

// damage flags
#define DAMAGE_RADIUS         ( 1 << 0 )  // damage was indirect
#define DAMAGE_HEADSHOT       ( 1 << 1 )
#define DAMAGE_WALLBANG       ( 1 << 2 )

//
// g_misc.c
//
void ThrowSmallPileOfGibs( edict_t * self, Vec3 knockback, int damage );

void SP_path_corner( edict_t * ent, const spawn_temp_t * st );
void SP_model( edict_t * ent, const spawn_temp_t * st );
void SP_decal( edict_t * ent, const spawn_temp_t * st );

//
// g_weapon.c
//
void G_FireWeapon( edict_t * ent, u64 parm );
void G_AltFireWeapon( edict_t * ent, u64 parm );
void G_UseGadget( edict_t * ent, GadgetType gadget, u64 parm, bool dead );

//
// g_chasecam
//
void G_ChasePlayer( edict_t * ent );
void G_ChaseStep( edict_t * ent, int step );
void Cmd_ToggleFreeFly( edict_t * ent, msg_t args );
void Cmd_Spectate( edict_t * ent );
void G_EndServerFrames_UpdateChaseCam();

//
// g_client.c
//
void ClientUserinfoChanged( edict_t * ent, const char * userinfo );
void G_Client_UpdateActivity( gclient_t *client );
void G_Client_InactivityRemove( gclient_t *client );
void G_ClientRespawn( edict_t * self, bool ghost );
score_stats_t * G_ClientGetStats( edict_t * ent );
void G_ClientClearStats( edict_t * ent );
void ClientThink( edict_t * ent, UserCommand *cmd, int timeDelta );
void G_ClientThink( edict_t * ent );
void G_CheckClientRespawnClick( edict_t * ent );
bool ClientConnect( edict_t * ent, char *userinfo, const NetAddress & address, bool fakeClient );
void ClientDisconnect( edict_t * ent, const char *reason );
void ClientBegin( edict_t * ent );
void ClientCommand( edict_t * ent, ClientCommandType command, msg_t args );
void G_PredictedEvent( int entNum, int ev, u64 parm );
void G_PredictedFireWeapon( int entNum, u64 parm );
void G_PredictedAltFireWeapon( int entNum, u64 parm );
void G_PredictedUseGadget( int entNum, GadgetType gadget, u64 parm, bool dead );
void G_SelectWeapon( edict_t * ent, int index );
void G_GiveWeapon( edict_t * ent, WeaponType weapon );
void G_GivePerk( edict_t * ent, PerkType perk );
void G_TeleportPlayer( edict_t * player, edict_t * dest );
bool G_PlayerCanTeleport( edict_t * player );

//
// g_player.c
//
void player_pain( edict_t * self, edict_t * other, float kick, int damage );
void player_die( edict_t * self, edict_t * inflictor, edict_t * attacker, int topAssistorEntNo, DamageType damage_type, int damage );
void player_think( edict_t * self );

//
// g_target.c
//
void SP_target_laser( edict_t * ent, const spawn_temp_t * st );
void SP_target_position( edict_t * ent, const spawn_temp_t * st );

//
// g_svcmds.c
//
void G_AddServerCommands();
void G_RemoveCommands();

//
// p_view.c
//
void G_ClientSetStats( edict_t * ent );
void G_ClientEndSnapFrame( edict_t * ent );
void G_ClientAddDamageIndicatorImpact( gclient_t * client, int damage, Vec3 dir );
void G_ClientDamageFeedback( edict_t * ent );

//
// g_phys.c
//
void SV_Impact( edict_t * e1, const trace_t & trace );
void G_RunEntity( edict_t * ent );

//
// g_main.c
//

void G_Init( unsigned int framemsec );
void G_Shutdown();
void G_ExitLevel();
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
void G_InitLevel( Span< const char > mapname, int64_t levelTime );

//
// window
//
void SP_window( edict_t * ent, const spawn_temp_t * st );

//============================================================================

struct projectileinfo_t {
	int radius;
	float minDamage;
	float maxDamage;
	float minKnockback;
	float maxKnockback;
	DamageType damage_type;
	StringHash explosion_vfx;
	StringHash explosion_sfx;
};

struct chasecam_t {
	bool active;
	int target;
	int mode;                   //3rd or 1st person
	Time timeout;           //delay after loosing target
};

struct assistinfo_t {
	int entno;
	int cumDamage;
	int64_t lastTime;
};

struct moveinfo_t {
	// fixed data
	Vec3 start_origin;
	EulerDegrees3 start_angles;
	Vec3 end_origin;
	EulerDegrees3 end_angles;

	StringHash sound_start;
	StringHash sound_middle;
	StringHash sound_end;

	Vec3 movedir;  // direction defined in the map

	float speed;
	float distance;    // used by binary movers

	s64 wait;

	// state data
	int state;

	void ( *endfunc )( edict_t * );
	void ( *blocked )( edict_t * self, edict_t * other );

	Vec3 dest;
};

#define MAX_CLIENT_EVENTS   16

#define G_MAX_TIME_DELTAS   8

struct client_snapreset_t {
	int buttons;
	uint8_t plrkeys; // used for displaying key icons
	int damageTaken;
	Vec3 damageTakenDir;
	bool kill;
	float damage_given;
};

struct client_respawnreset_t {
	chasecam_t chase;

	int64_t timeStamp; // last time it was reset

	SyncEvent events[MAX_CLIENT_EVENTS];
	unsigned int eventsCurrent;
	unsigned int eventsHead;
};

struct client_levelreset_t {
	int64_t timeStamp;				// last time it was reset

	Time last_vsay;
	int64_t last_activity;
	Time last_spray;

	score_stats_t stats;

	// flood protection
	Time flood_locktill;			// locked from talking
	Time flood_when[MAX_FLOOD_MESSAGES];        // when messages were said
	int flood_whenhead;             // head pointer for when said
	// team only
	Time flood_team_when[MAX_FLOOD_MESSAGES];   // when messages were said
	int flood_team_whenhead;        // head pointer for when said

	Time callvote_when;
};

struct client_teamreset_t {
	int64_t timeStamp; // last time it was reset

	// for position command
	bool position_saved;
	Vec3 position_origin;
	EulerDegrees3 position_angles;
	Time position_lastcmd;
};

struct gclient_t {
	SyncPlayerState ps;
	int frags;

	client_snapreset_t snap;
	client_respawnreset_t resp;
	client_levelreset_t level;
	client_teamreset_t teamstate;

	char userinfo[ MAX_INFO_STRING ];
	char name[ MAX_NAME_CHARS + 1 ];

	bool connecting;

	Team team;

	UserCommand ucmd;
	int timeDelta;              // time offset to adjust for shots collision (antilag)
	int timeDeltas[G_MAX_TIME_DELTAS];
	int timeDeltasHead;

	pmove_state_t old_pmove;    // for detecting out-of-pmove changes
};

using EdictTouchCallback = void ( * )( edict_t * self, edict_t * other, Vec3 normal, SolidBits solid_mask );

struct edict_t {
	SyncEntityState s;
	entity_shared_t r;

	// DO NOT MODIFY ANYTHING ABOVE THIS, THE SERVER
	// EXPECTS THE FIELDS IN THAT ORDER!

	//================================

	int movetype;

	Time freetime;

	int numEvents;
	bool eventPriority[2];

	//
	// only used locally in game, not by server
	//

	StringHash classname;
	int spawnflags;

	int64_t nextThink;

	void ( *think )( edict_t * self );
	EdictTouchCallback touch;
	void ( *use )( edict_t * self, edict_t * other, edict_t * activator );
	void ( *pain )( edict_t * self, edict_t * other, float kick, int damage );
	void ( *die )( edict_t * self, edict_t * inflictor, edict_t * attacker, int assistorNo, DamageType damage_type, int damage );
	void ( *stop )( edict_t * self );

	StringHash name;
	StringHash target;
	StringHash killtarget;
	StringHash pathtarget;
	StringHash deadcam;
	edict_t * target_ent;

	Vec3 velocity;
	EulerDegrees3 avelocity;

	float speed;

	int64_t timeStamp;
	int64_t deathTimeStamp;
	int timeDelta;              // SVF_PROJECTILE only. Used for 4D collision detection

	projectileinfo_t projectileInfo;    // specific for projectiles

	int dmg;

	const char * message;

	int mass;
	float gravity_scale;
	float restitution;

	edict_t * movetarget;

	int64_t pain_debounce_time;

	float health;
	float max_health;
	int deadflag;

	int viewheight;				// height above origin where eyesight is determined
	bool takedamage;

	int count;

	int64_t timeout;			// for SW and fat PG

	edict_t * enemy;
	edict_t * activator;
	edict_t * groundentity;
	StringHash sound;

	// timing variables
	s64 wait;
	s64 delay;                // before firing targets
	s64 wait_randomness;

	// common data blocks
	moveinfo_t moveinfo;        // func movers movement

	assistinfo_t recent_attackers[ 4 ];
	u32 num_bounces;
};

struct game_locals_t {
	edict_t edicts[ MAX_EDICTS ];
	gclient_t clients[ MAX_CLIENTS ];

	// store latched cvars here that we want to get at often
	int numentities;

	// cross level triggers
	int serverflags;

	unsigned int frametime;         // in milliseconds
	int snapFrameTime;              // in milliseconds
	int64_t prevServerTime;         // last frame's server time

	int numBots;
};

extern game_locals_t game;

constexpr edict_t * world = &game.edicts[ 0 ];

constexpr int ENTNUM( const edict_t * x ) { return x - game.edicts; }
constexpr int ENTNUM( const gclient_t * x ) { return x - game.clients + 1; }

constexpr int PLAYERNUM( const edict_t * x ) { return x - game.edicts - 1; }
constexpr int PLAYERNUM( const gclient_t * x ) { return x - game.clients; }

constexpr edict_t * PLAYERENT( int x ) { return game.edicts + x + 1; }

static inline bool G_ISGHOSTING( const edict_t * ent ) { return EntitySolidity( ServerCollisionModelStorage(), &ent->s ) == Solid_NotSolid; }
