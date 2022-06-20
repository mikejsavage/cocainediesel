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

Cvar *cg_thirdPerson;
Cvar *cg_thirdPersonAngle;
Cvar *cg_thirdPersonRange;

Cvar *cg_projectileAntilagOffset;

Cvar *cg_showClamp;

Cvar *cg_showServerDebugPrints;

void CG_LocalPrint( const char *format, ... ) {
	va_list argptr;
	char msg[ 1024 ];

	va_start( argptr, format );
	vsnprintf( msg, sizeof( msg ), format, argptr );
	va_end( argptr );

	Con_Print( msg );

	CG_AddChat( msg );
}

static void CG_GS_Trace( trace_t *t, Vec3 start, Vec3 mins, Vec3 maxs, Vec3 end, int ignore, int contentmask, int timeDelta ) {
	assert( !timeDelta );
	CG_Trace( t, start, mins, maxs, end, ignore, contentmask );
}

static int CG_GS_PointContents( Vec3 point, int timeDelta ) {
	assert( !timeDelta );
	return CG_PointContents( point );
}

static SyncEntityState *CG_GS_GetEntityState( int entNum, int deltaTime ) {
	centity_t *cent;

	if( entNum == -1 ) {
		return NULL;
	}

	assert( entNum >= 0 && entNum < MAX_EDICTS );
	cent = &cg_entities[entNum];

	if( cent->serverFrame != cg.frame.serverFrame ) {
		return NULL;
	}
	return &cent->current;
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
	client_gs.api.GetEntityState = CG_GS_GetEntityState;
	client_gs.api.PointContents = CG_GS_PointContents;
	client_gs.api.PMoveTouchTriggers = CG_Predict_TouchTriggers;
}

static void CG_RegisterVariables() {
	cg_showMiss = NewCvar( "cg_showMiss", "0", 0 );
	cg_showMasks = NewCvar( "cg_showMasks", "0", 0 );

	cg_thirdPerson = NewCvar( "cg_thirdPerson", "0", CvarFlag_Cheat );
	cg_thirdPersonAngle = NewCvar( "cg_thirdPersonAngle", "0", 0 );
	cg_thirdPersonRange = NewCvar( "cg_thirdPersonRange", "90", 0 );

	cg_projectileAntilagOffset = NewCvar( "cg_projectileAntilagOffset", "1.0", CvarFlag_Archive );

	cg_showClamp = NewCvar( "cg_showClamp", "0", CvarFlag_Developer );

	cg_showServerDebugPrints = NewCvar( "cg_showServerDebugPrints", "0", CvarFlag_Archive );
}

const char * PlayerName( int i ) {
	if( i < 0 || i >= client_gs.maxclients ) {
		return "";
	}

	Span< const char[ MAX_CONFIGSTRING_CHARS ] > names( cl.configstrings + CS_PLAYERINFOS, client_gs.maxclients );
	return names[ i ];
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

static void PrintMap() {
	Com_Printf( "Current map: %s\n", cl.map == NULL ? "null" : cl.map->name );
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

	CG_DemocamInit();

	AddCommand( "printmap", PrintMap );
}

void CG_Shutdown() {
	CG_DemocamShutdown();
	CG_UnregisterCGameCommands();
	CG_ShutdownChat();
	CG_ShutdownInput();
	CG_ShutdownHUD();
	ShutdownDecals();

	RemoveCommand( "printmap" );
}
