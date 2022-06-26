#pragma once

extern Cvar *g_warmup_timelimit;

struct GametypeDef {
	void ( *Init )();
	void ( *MatchStateStarted )();
	void ( *Think )();
	void ( *PlayerRespawning )( edict_t * ent );
	void ( *PlayerRespawned )( edict_t * ent, Team old_team, Team new_team );
	void ( *PlayerConnected )( edict_t * ent );
	void ( *PlayerDisconnected )( edict_t * ent );
	void ( *PlayerKilled )( edict_t * victim, edict_t * attacker, edict_t * inflictor );
	const edict_t * ( *SelectSpawnPoint )( const edict_t * ent );
	const edict_t * ( *SelectDeadcam )();
	void ( *Shutdown )();
	bool ( *SpawnEntity )( StringHash classname, edict_t * ent );
	void ( *MapHotloaded )();

	int numTeams;
	bool removeInactivePlayers;
	bool selfDamage;
	bool autoRespawn;
};

void InitGametype();
void ShutdownGametype();

GametypeDef GetBombGametype();
GametypeDef GetGladiatorGametype();

void G_Match_LaunchState( MatchState matchState );

void G_Teams_Init();

void G_Match_Autorecord_Start();
void G_Match_Autorecord_Stop();
bool G_Match_ScorelimitHit();
bool G_Match_TimelimitHit();

void G_RunGametype();
void GT_CallMatchStateStarted();
void GT_CallPlayerConnected( edict_t * ent );
void GT_CallPlayerRespawning( edict_t * ent );
void GT_CallPlayerRespawned( edict_t * ent, Team old_team, Team new_team );
void GT_CallPlayerKilled( edict_t * victim, edict_t * attacker, edict_t * inflictor );
const edict_t * GT_CallSelectSpawnPoint( const edict_t * ent );
const edict_t * GT_CallSelectDeadcam();
