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
	void *initFunc;
	void *spawnFunc;
	void *matchStateStartedFunc;
	void *matchStateFinishedFunc;
	void *thinkRulesFunc;
	void *playerRespawnFunc;
	void *scoreEventFunc;
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
	bool removeInactivePlayers;
	bool selfDamage;

	int spawnpointRadius;
} gametype_descriptor_t;

typedef struct {
	bool locked;
	int asRefCount;
	int asFactored;
} g_teaminfo_t;

extern g_teaminfo_t teaminfo[ GS_MAX_TEAMS ];

//
//	matches management
//
void G_Match_RemoveProjectiles( edict_t *owner );
void G_Match_CleanUpPlayerStats( edict_t *ent );
void G_Match_FreeBodyQueue();
void G_Match_LaunchState( int matchState );

//
//	teams
//
void G_Teams_Init();

void G_Teams_ExecuteChallengersQueue();
void G_Teams_AdvanceChallengersQueue();

void G_Match_Autorecord_Start();
void G_Match_Autorecord_AltStart();
void G_Match_Autorecord_Stop();
void G_Match_Autorecord_Cancel();
bool G_Match_ScorelimitHit();
bool G_Match_TimelimitHit();
