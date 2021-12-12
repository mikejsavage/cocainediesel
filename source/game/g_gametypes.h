#pragma once

extern Cvar *g_warmup_timelimit;

struct Gametype {
	void ( *Init )();
	void ( *MatchStateStarted )();
	bool ( *MatchStateFinished )( MatchState incomingMatchState );
	void ( *Think )();
	void ( *PlayerRespawning )( edict_t * ent );
	void ( *PlayerRespawned )( edict_t * ent, int old_team, int new_team );
	void ( *PlayerConnected )( edict_t * ent );
	void ( *PlayerDisconnected )( edict_t * ent );
	void ( *PlayerKilled )( edict_t * victim, edict_t * attacker, edict_t * inflictor );
	edict_t * ( *SelectSpawnPoint )( edict_t * ent );
	bool ( *Command )( gclient_t * client, const char * cmd, const char * args, int argc );
	void ( *Shutdown )();
	bool ( *SpawnEntity )( StringHash classname, edict_t * ent );
	void ( *MapHotloaded )();

	bool isTeamBased;
	bool countdownEnabled;
	bool removeInactivePlayers;
	bool selfDamage;
	bool autoRespawn;
};

void InitGametype();
void ShutdownGametype();

Gametype GetBombGametype();
Gametype GetGladiatorGametype();

void G_Match_CleanUpPlayerStats( edict_t *ent );
void G_Match_LaunchState( MatchState matchState );

void G_Teams_Init();

void G_Match_Autorecord_Start();
void G_Match_Autorecord_Stop();
void G_Match_Autorecord_Cancel();
bool G_Match_ScorelimitHit();
bool G_Match_TimelimitHit();

void G_RunGametype();
void GT_CallMatchStateStarted();
bool GT_CallMatchStateFinished( MatchState incomingMatchState );
void GT_CallPlayerConnected( edict_t * ent );
void GT_CallPlayerRespawning( edict_t * ent );
void GT_CallPlayerRespawned( edict_t * ent, int old_team, int new_team );
void GT_CallPlayerKilled( edict_t * victim, edict_t * attacker, edict_t * inflictor );
edict_t *GT_CallSelectSpawnPoint( edict_t *ent );
bool GT_CallGameCommand( gclient_t *client, const char *cmd, const char *args, int argc );

Span< const char > G_GetWorldspawnKey( const char * key );
