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

#include "cgame/cg_local.h"
#include "client/renderer/renderer.h"

cg_static_t cgs;
cg_state_t cg;

centity_t cg_entities[MAX_EDICTS];

Cvar *cg_showMiss;
Cvar *cg_mask;

Cvar *cg_thirdPerson;
Cvar *cg_thirdPersonRange;

Cvar *cg_projectileAntilagOffset;

Cvar *cg_showClamp;

Cvar *cg_showServerDebugPrints;

void CG_LocalPrint( Span< const char > str ) {
	Con_Print( str );
	CG_AddChat( str );
}

static trace_t CG_GS_Trace( Vec3 start, MinMax3 bounds, Vec3 end, int ignore, SolidBits solid_mask, int timeDelta ) {
	Assert( timeDelta == 0 );
	return CG_Trace( start, bounds, end, ignore, solid_mask );
}

static void CG_InitGameShared( int max_clients ) {
	client_gs = { };
	client_gs.module = GS_MODULE_CGAME;
	client_gs.maxclients = max_clients;

	client_gs.api.PredictedEvent = CG_PredictedEvent;
	client_gs.api.PredictedFireWeapon = CG_PredictedFireWeapon;
	client_gs.api.PredictedAltFireWeapon = CG_PredictedAltFireWeapon;
	client_gs.api.PredictedUseGadget = CG_PredictedUseGadget;
	client_gs.api.Trace = CG_GS_Trace;
	client_gs.api.PMoveTouchTriggers = CG_Predict_TouchTriggers;
}

static void CG_RegisterVariables() {
	cg_showMiss = NewCvar( "cg_showMiss", "0" );
	cg_mask = NewCvar( "cg_mask", "", CvarFlag_Archive | CvarFlag_UserInfo );

	cg_thirdPerson = NewCvar( "cg_thirdPerson", "0", CvarFlag_Developer );
	cg_thirdPersonRange = NewCvar( "cg_thirdPersonRange", "90", CvarFlag_Developer );

	cg_projectileAntilagOffset = NewCvar( "cg_projectileAntilagOffset", "1.0", CvarFlag_Archive );

	cg_showClamp = NewCvar( "cg_showClamp", "0", CvarFlag_Developer );

	cg_showServerDebugPrints = NewCvar( "cg_showServerDebugPrints", "0", CvarFlag_Archive );
}

const char * PlayerName( int i ) {
	if( i < 0 || i >= client_gs.maxclients ) {
		return "";
	}

	return client_gs.gameState.players[ i ].name;
}

void CG_Reset() {
	CG_ResetPModels();

	CG_SC_ResetObituaries();

	// start up announcer events queue from clean
	CG_ClearAnnouncerEvents();

	CG_ClearInputState();

	CG_InitDamageNumbers();
	ResetDecals();
	InitPersistentBeams();
	InitSprays();
	ClearParticles();

	chaseCam.key_pressed = false;

	// reset prediction optimization
	cg.predictFrom = 0;

	memset( cg_entities, 0, sizeof( cg_entities ) );
}

static void PrintMap( const Tokenized & args ) {
	Com_GGPrint( "Current map: {}", cl.map == NULL ? "none" : cl.map->name );
}

void CG_Init( unsigned int playerNum, int max_clients,
			  bool demoplaying, const char *demoName,
			  unsigned snapFrameTime ) {
	memset( &cg, 0, sizeof( cg_state_t ) );
	memset( &cgs, 0, sizeof( cg_static_t ) );

	memset( cg_entities, 0, sizeof( cg_entities ) );

	MaybeResetShadertoyTime( true );

	CG_InitGameShared( max_clients );

	// save local player number
	cgs.playerNum = playerNum;

	// demo
	cgs.demoPlaying = demoplaying;
	cgs.demoName = demoName;

	cgs.snapFrameTime = snapFrameTime;

	CG_InitInput();

	CG_RegisterVariables();
	InitPlayerModels();
	InitWeaponModels();

	CG_ScreenInit();

	CG_InitDamageNumbers();

	CG_RegisterCGameCommands();

	CG_RegisterMedia();

	CG_SC_ResetObituaries();
	CG_InitHUD();

	InitDecals();
	InitSprays();
	InitPersistentBeams();
	InitGibs();
	ClearParticles();

	CG_InitChat();

	CG_ClearAnnouncerEvents();

	CG_DemoCamInit();

	AddCommand( "printmap", PrintMap );
}

void CG_Shutdown() {
	CG_DemoCamShutdown();
	CG_UnregisterCGameCommands();
	CG_ShutdownChat();
	CG_ShutdownInput();
	CG_ShutdownHUD();
	ShutdownDecals();

	RemoveCommand( "printmap" );
}
