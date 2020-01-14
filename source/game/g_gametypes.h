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

//g_gametypes.c
extern cvar_t *g_warmup_timelimit;

#define G_CHALLENGERS_MIN_JOINTEAM_MAPTIME  9000 // must wait 10 seconds before joining
#define GAMETYPE_PROJECT_EXTENSION          ".gt"

typedef struct {
	int score;
	int deaths;
	int frags;
	int suicides;

	int accuracy_shots[ Weapon_Count ];
	int accuracy_hits[ Weapon_Count ];
	int accuracy_damage[ Weapon_Count ];
	int accuracy_frags[ Weapon_Count ];
	int total_damage_given;
	int total_damage_received;

	int asFactored;
	int asRefCount;
} score_stats_t;

typedef struct {
	void *initFunc;
	void *spawnFunc;
	void *matchStateStartedFunc;
	void *matchStateFinishedFunc;
	void *thinkRulesFunc;
	void *playerRespawnFunc;
	void *scoreEventFunc;
	void *scoreboardMessageFunc;
	void *selectSpawnPointFunc;
	void *clientCommandFunc;
	void *shutdownFunc;

	bool isTeamBased;
	bool isRace;
	bool hasChallengersQueue;
	bool hasChallengersRoulette;
	int maxPlayersPerTeam;

	// few default settings
	bool readyAnnouncementEnabled;
	bool scoreAnnouncementEnabled;
	bool countdownEnabled;
	bool matchAbortDisabled;
	bool shootingDisabled;
	bool customDeadBodyCam;
	bool removeInactivePlayers;
	bool selfDamage;

	int spawnpointRadius;
} gametype_descriptor_t;

typedef struct {
	int playerIndices[MAX_CLIENTS];
	int numplayers;
	int ping;
	bool locked;

	int asRefCount;
	int asFactored;
} g_teamlist_t;

extern g_teamlist_t teamlist[GS_MAX_TEAMS];

//
//	matches management
//
void G_Match_RemoveProjectiles( edict_t *owner );
void G_Match_CleanUpPlayerStats( edict_t *ent );
void G_Match_FreeBodyQueue( void );
void G_Match_LaunchState( int matchState );

//
//	teams
//
void G_Teams_Init( void );

void G_Teams_ExecuteChallengersQueue( void );
void G_Teams_AdvanceChallengersQueue( void );

void G_Match_Autorecord_Start( void );
void G_Match_Autorecord_AltStart( void );
void G_Match_Autorecord_Stop( void );
void G_Match_Autorecord_Cancel( void );
bool G_Match_ScorelimitHit( void );
bool G_Match_TimelimitHit( void );
