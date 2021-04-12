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

mempool_t *cg_mempool;

centity_t cg_entities[MAX_EDICTS];

cvar_t *cg_showMiss;

cvar_t *cg_thirdPerson;
cvar_t *cg_thirdPersonAngle;
cvar_t *cg_thirdPersonRange;

cvar_t *cg_gunx;
cvar_t *cg_guny;
cvar_t *cg_gunz;
cvar_t *cg_gunbob;

cvar_t *cg_gun_fov;
cvar_t *cg_projectileAntilagOffset;
cvar_t *cg_chat;

cvar_t *cg_showHotkeys;

cvar_t *cg_autoaction_demo;
cvar_t *cg_autoaction_screenshot;
cvar_t *cg_autoaction_spectator;
cvar_t *cg_showClamp;

cvar_t *cg_particleDebug;

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

static void CG_InitGameShared() {
	char cstring[MAX_CONFIGSTRING_CHARS];
	trap_GetConfigString( CS_MAXCLIENTS, cstring, MAX_CONFIGSTRING_CHARS );
	int maxclients = atoi( cstring );
	if( maxclients < 1 || maxclients > MAX_CLIENTS ) {
		maxclients = MAX_CLIENTS;
	}

	client_gs = { };
	client_gs.module = GS_MODULE_CGAME;
	client_gs.maxclients = maxclients;

	client_gs.api.PredictedEvent = CG_PredictedEvent;
	client_gs.api.PredictedFireWeapon = CG_PredictedFireWeapon;
	client_gs.api.Trace = CG_GS_Trace;
	client_gs.api.GetEntityState = CG_GS_GetEntityState;
	client_gs.api.PointContents = CG_GS_PointContents;
	client_gs.api.PMoveTouchTriggers = CG_Predict_TouchTriggers;
}

char *_CG_CopyString( const char *in, const char *filename, int fileline ) {
	char * out = ( char * )_Mem_AllocExt( cg_mempool, strlen( in ) + 1, 16, 1, 0, 0, filename, fileline );
	strcpy( out, in );
	return out;
}

static void CG_RegisterClients() {
	for( int i = 0; i < MAX_CLIENTS; i++ ) {
		const char * name = cgs.configStrings[CS_PLAYERINFOS + i];
		if( !name[0] )
			continue;
		CG_LoadClientInfo( i );
	}
}

static void CG_RegisterVariables() {
	cg_showMiss =       Cvar_Get( "cg_showMiss", "0", 0 );

	cg_showHotkeys = Cvar_Get( "cg_showHotkeys", "1", CVAR_ARCHIVE );

	cg_thirdPerson =    Cvar_Get( "cg_thirdPerson", "0", CVAR_CHEAT );
	cg_thirdPersonAngle =   Cvar_Get( "cg_thirdPersonAngle", "0", 0 );
	cg_thirdPersonRange =   Cvar_Get( "cg_thirdPersonRange", "90", 0 );

	cg_gunx =       Cvar_Get( "cg_gunx", "10", CVAR_CHEAT );
	cg_guny =       Cvar_Get( "cg_guny", "-12", CVAR_CHEAT );
	cg_gunz =       Cvar_Get( "cg_gunz", "3", CVAR_CHEAT );
	cg_gunbob =     Cvar_Get( "cg_gunbob", "1", CVAR_ARCHIVE );

	cg_gun_fov =        Cvar_Get( "cg_gun_fov", "90", CVAR_ARCHIVE );

	cg_autoaction_demo =    Cvar_Get( "cg_autoaction_demo", "0", CVAR_ARCHIVE );
	cg_autoaction_screenshot =  Cvar_Get( "cg_autoaction_screenshot", "0", CVAR_ARCHIVE );
	cg_autoaction_spectator = Cvar_Get( "cg_autoaction_spectator", "0", CVAR_ARCHIVE );

	cg_projectileAntilagOffset = Cvar_Get( "cg_projectileAntilagOffset", "1.0", CVAR_ARCHIVE );

	cg_chat =           Cvar_Get( "cg_chat", "1", CVAR_ARCHIVE );

	cg_showClamp =      Cvar_Get( "cg_showClamp", "0", CVAR_DEVELOPER );

	cg_particleDebug =  Cvar_Get( "cg_particleDebug", "0", CVAR_DEVELOPER );

	Cvar_Get( "cg_loadout", "", CVAR_ARCHIVE | CVAR_USERINFO );
}

void CG_Precache() {
	if( cgs.precacheDone ) {
		return;
	}

	CG_RegisterMediaModels();
	CG_RegisterMediaSounds();
	CG_RegisterMediaSounds();
	CG_RegisterMediaShaders();
	CG_RegisterClients();

	cgs.precacheDone = true;
}

static void CG_RegisterConfigStrings() {
	for( int i = 0; i < MAX_CONFIGSTRINGS; i++ ) {
		trap_GetConfigString( i, cgs.configStrings[i], MAX_CONFIGSTRING_CHARS );
	}

	// backup initial configstrings for CG_Reset
	memcpy( &cgs.baseConfigStrings[0][0], &cgs.configStrings[0][0], MAX_CONFIGSTRINGS*MAX_CONFIGSTRING_CHARS );

	CG_SC_AutoRecordAction( cgs.configStrings[CS_AUTORECORDSTATE] );
}

void CG_Reset() {
	memcpy( &cgs.configStrings[0][0], &cgs.baseConfigStrings[0][0], MAX_CONFIGSTRINGS*MAX_CONFIGSTRING_CHARS );

	CG_ResetClientInfos();

	CG_ResetPModels();

	CG_ResetKickAngles();

	CG_SC_ResetObituaries();

	// start up announcer events queue from clean
	CG_ClearAnnouncerEvents();

	CG_ClearInputState();

	CG_ClearPointedNum();

	CG_ClearAwards();

	CG_InitDamageNumbers();
	InitDecals();
	InitPersistentBeams();
	InitSprays();

	chaseCam.key_pressed = false;

	// reset prediction optimization
	cg.predictFrom = 0;

	memset( cg_entities, 0, sizeof( cg_entities ) );
}

static void PrintMap() {
	Com_Printf( "Current map: %s\n", cl.map == NULL ? "null" : cl.map->name );
}

void CG_Init( const char *serverName, unsigned int playerNum,
			  bool demoplaying, const char *demoName,
			  unsigned snapFrameTime ) {
	cg_mempool = _Mem_AllocPool( NULL, "CGame", MEMPOOL_CLIENTGAME, __FILE__, __LINE__ );

	CG_InitGameShared();

	memset( &cg, 0, sizeof( cg_state_t ) );
	memset( &cgs, 0, sizeof( cg_static_t ) );

	memset( cg_entities, 0, sizeof( cg_entities ) );

	// save server name
	cgs.serverName = CG_CopyString( serverName );

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

	SCR_UpdateScoreboardMessage( "" );

	CG_InitDamageNumbers();

	// get configstrings
	CG_RegisterConfigStrings();

	// register fonts here so loading screen works
	CG_RegisterFonts();

	CG_RegisterCGameCommands();

	CG_InitHUD();

	InitDecals();
	InitSprays();
	InitPersistentBeams();
	InitGibs();

	CG_InitChat();

	// start up announcer events queue from clean
	CG_ClearAnnouncerEvents();

	cg.firstFrame = true; // think of the next frame in CG_NewFrameSnap as of the first one

	// now that we're done with precaching, let the autorecord actions do something
	CG_ConfigString( CS_AUTORECORDSTATE, cgs.configStrings[CS_AUTORECORDSTATE] );

	CG_DemocamInit();

	Cmd_AddCommand( "printmap", PrintMap );
}

void CG_Shutdown() {
	CG_DemocamShutdown();
	CG_UnregisterCGameCommands();
	CG_ShutdownChat();
	CG_ShutdownInput();
	CG_ShutdownHUD();
	ShutdownDecals();

	CG_Free( const_cast< char * >( cgs.serverName ) );

	Mem_FreePool( &cg_mempool );

	Cmd_RemoveCommand( "printmap" );
}
