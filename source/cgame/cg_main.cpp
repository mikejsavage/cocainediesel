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

cg_static_t cgs;
cg_state_t cg;

mempool_t *cg_mempool;

centity_t cg_entities[MAX_EDICTS];

cvar_t *cg_showMiss;

cvar_t *cg_hand;
cvar_t *cg_handicap;

cvar_t *cg_addDecals;

cvar_t *cg_gun;

cvar_t *cg_thirdPerson;
cvar_t *cg_thirdPersonAngle;
cvar_t *cg_thirdPersonRange;

cvar_t *cg_weaponFlashes;
cvar_t *cg_gunx;
cvar_t *cg_guny;
cvar_t *cg_gunz;
cvar_t *cg_debugPlayerModels;
cvar_t *cg_debugWeaponModels;
cvar_t *cg_gunbob;

cvar_t *cg_handOffset;
cvar_t *cg_gun_fov;
cvar_t *cg_volume_players;
cvar_t *cg_volume_effects;
cvar_t *cg_volume_announcer;
cvar_t *cg_volume_voicechats;
cvar_t *cg_projectileFireTrail;
cvar_t *cg_bloodTrail;
cvar_t *cg_showBloodTrail;
cvar_t *cg_projectileFireTrailAlpha;
cvar_t *cg_bloodTrailAlpha;
cvar_t *cg_explosionsRing;
cvar_t *cg_explosionsDust;
cvar_t *cg_fov;
cvar_t *cg_zoomfov;
cvar_t *cg_voiceChats;
cvar_t *cg_projectileAntilagOffset;
cvar_t *cg_chatFilter;

cvar_t *cg_cartoonEffects;

cvar_t *cg_volume_hitsound;
cvar_t *cg_autoaction_demo;
cvar_t *cg_autoaction_screenshot;
cvar_t *cg_autoaction_spectator;
cvar_t *cg_showObituaries;
cvar_t *cg_damageNumbers;
cvar_t *cg_particles;
cvar_t *cg_showClamp;

cvar_t *cg_allyColor;
cvar_t *cg_allyModel;
cvar_t *cg_allyForceModel;

cvar_t *cg_enemyColor;
cvar_t *cg_enemyModel;
cvar_t *cg_enemyForceModel;

/*
* CG_LocalPrint
*/
void CG_LocalPrint( const char *format, ... ) {
	va_list argptr;
	char msg[GAMECHAT_STRING_SIZE];

	va_start( argptr, format );
	vsnprintf( msg, sizeof( msg ), format, argptr );
	va_end( argptr );

	Con_Print( msg );

	CG_AddChat( msg );
}

/*
* CG_GS_Trace
*/
static void CG_GS_Trace( trace_t *t, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int ignore, int contentmask, int timeDelta ) {
	assert( !timeDelta );
	CG_Trace( t, start, mins, maxs, end, ignore, contentmask );
}

/*
* CG_GS_PointContents
*/
static int CG_GS_PointContents( const vec3_t point, int timeDelta ) {
	assert( !timeDelta );
	return CG_PointContents( point );
}

/*
* CG_GS_GetEntityState
*/
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

/*
* CG_GS_GetConfigString
*/
static const char *CG_GS_GetConfigString( int index ) {
	if( index < 0 || index >= MAX_CONFIGSTRINGS ) {
		return NULL;
	}

	return cgs.configStrings[ index ];
}

/*
* CG_InitGameShared
*
* Give gameshared access to some utilities
*/
static void CG_InitGameShared( void ) {
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
	client_gs.api.Trace = CG_GS_Trace;
	client_gs.api.GetEntityState = CG_GS_GetEntityState;
	client_gs.api.PointContents = CG_GS_PointContents;
	client_gs.api.PMoveTouchTriggers = CG_Predict_TouchTriggers;
	client_gs.api.GetConfigString = CG_GS_GetConfigString;
}

/*
* CG_CopyString
*/
char *_CG_CopyString( const char *in, const char *filename, int fileline ) {
	char * out = ( char * )_Mem_AllocExt( cg_mempool, strlen( in ) + 1, 16, 1, 0, 0, filename, fileline );
	strcpy( out, in );
	return out;
}

/*
* CG_RegisterWeaponModels
*/
static void CG_RegisterWeaponModels( void ) {
	for( WeaponType i = 0; i < cgs.numWeaponModels; i++ ) {
		cgs.weaponInfos[i] = CG_RegisterWeaponModel( cgs.weaponModels[i], i );
	}

	cgs.weaponInfos[ Weapon_Count ] = CG_CreateWeaponZeroModel();
}

/*
* CG_RegisterModels
*/
static void CG_RegisterModels( void ) {
	int i;
	const char *name;

	if( cgs.precacheModelsStart == MAX_MODELS ) {
		return;
	}

	if( cgs.precacheModelsStart == 0 ) {
		name = cgs.configStrings[CS_WORLDMODEL];
		if( name[0] ) {
			if( !CG_LoadingItemName( name ) ) {
				return;
			}
		}

		cgs.numWeaponModels = 0;
		cgs.precacheModelsStart = 1;
	}

	for( i = cgs.precacheModelsStart; i < MAX_MODELS; i++ ) {
		name = cgs.configStrings[CS_MODELS + i];

		if( !name[0] ) {
			cgs.precacheModelsStart = MAX_MODELS;
			break;
		}

		cgs.precacheModelsStart = i;

		if( name[0] == '#' ) {
			assert( cgs.numWeaponModels < Weapon_Count );

			if( !CG_LoadingItemName( name ) ) {
				return;
			}

			Q_strncpyz( cgs.weaponModels[cgs.numWeaponModels], name + 1, sizeof( cgs.weaponModels[cgs.numWeaponModels] ) );
			cgs.numWeaponModels++;
		} else if( name[0] == '*' ) {
			u64 hash = Hash64( name, strlen( name ), cgs.map->base_hash );
			cgs.modelDraw[i] = FindModel( StringHash( hash ) );
		} else {
			if( !CG_LoadingItemName( name ) ) {
				return;
			}
			cgs.modelDraw[i] = FindModel( name );
		}
	}

	if( cgs.precacheModelsStart != MAX_MODELS ) {
		return;
	}

	CG_RegisterMediaModels();
	CG_RegisterWeaponModels();

	// precache forcemodels if defined
	CG_RegisterForceModels();

	// create a tag to offset the weapon models when seen in the world as items
	VectorSet( cgs.weaponItemTag.origin, 0, 0, 0 );
	Matrix3_Copy( axis_identity, cgs.weaponItemTag.axis );
	VectorMA( cgs.weaponItemTag.origin, -14, &cgs.weaponItemTag.axis[AXIS_FORWARD], cgs.weaponItemTag.origin );
}

/*
* CG_RegisterSounds
*/
static void CG_RegisterSounds( void ) {
	if( cgs.precacheSoundsStart == MAX_SOUNDS ) {
		return;
	}

	if( !cgs.precacheSoundsStart ) {
		cgs.precacheSoundsStart = 1;
	}

	for( int i = cgs.precacheSoundsStart; i < MAX_SOUNDS; i++ ) {
		const char *name = cgs.configStrings[CS_SOUNDS + i];
		if( !name[0] ) {
			cgs.precacheSoundsStart = MAX_SOUNDS;
			break;
		}

		cgs.precacheSoundsStart = i;

		if( !CG_LoadingItemName( name ) ) {
			return;
		}
		cgs.soundPrecache[i] = FindSoundEffect( name );
	}

	if( cgs.precacheSoundsStart != MAX_SOUNDS ) {
		return;
	}

	CG_RegisterMediaSounds();
}

/*
* CG_RegisterShaders
*/
static void CG_RegisterShaders( void ) {
	int i;
	const char *name;

	if( cgs.precacheShadersStart == MAX_IMAGES ) {
		return;
	}

	if( !cgs.precacheShadersStart ) {
		cgs.precacheShadersStart = 1;
	}

	for( i = cgs.precacheShadersStart; i < MAX_IMAGES; i++ ) {
		name = cgs.configStrings[CS_IMAGES + i];
		if( !name[0] ) {
			cgs.precacheShadersStart = MAX_IMAGES;
			break;
		}

		cgs.precacheShadersStart = i;

		if( !CG_LoadingItemName( name ) ) {
			return;
		}

		cgs.imagePrecache[i] = FindMaterial( name );
	}

	if( cgs.precacheShadersStart != MAX_IMAGES ) {
		return;
	}

	CG_RegisterMediaShaders();
}

/*
* CG_RegisterClients
*/
static void CG_RegisterClients( void ) {
	int i;
	const char *name;

	if( cgs.precacheClientsStart == MAX_CLIENTS ) {
		return;
	}

	for( i = cgs.precacheClientsStart; i < MAX_CLIENTS; i++ ) {
		name = cgs.configStrings[CS_PLAYERINFOS + i];
		cgs.precacheClientsStart = i;

		if( !name[0] ) {
			continue;
		}
		if( !CG_LoadingItemName( name ) ) {
			return;
		}

		CG_LoadClientInfo( i );
	}

	cgs.precacheClientsStart = MAX_CLIENTS;
}

/*
* CG_RegisterVariables
*/
static void CG_RegisterVariables( void ) {
	cg_showMiss =       Cvar_Get( "cg_showMiss", "0", 0 );

	cg_debugPlayerModels =  Cvar_Get( "cg_debugPlayerModels", "0", CVAR_CHEAT );
	cg_debugWeaponModels =  Cvar_Get( "cg_debugWeaponModels", "0", CVAR_CHEAT );

	cg_hand =           Cvar_Get( "hand", "0", CVAR_USERINFO | CVAR_ARCHIVE );
	cg_handicap =       Cvar_Get( "handicap", "0", CVAR_USERINFO | CVAR_ARCHIVE );
	cg_fov =        Cvar_Get( "fov", "100", CVAR_ARCHIVE );
	cg_zoomfov =    Cvar_Get( "zoomfov", "75", CVAR_ARCHIVE );

	cg_addDecals =      Cvar_Get( "cg_decals", "1", CVAR_ARCHIVE );

	cg_thirdPerson =    Cvar_Get( "cg_thirdPerson", "0", CVAR_CHEAT );
	cg_thirdPersonAngle =   Cvar_Get( "cg_thirdPersonAngle", "0", 0 );
	cg_thirdPersonRange =   Cvar_Get( "cg_thirdPersonRange", "90", 0 );

	cg_gun =        Cvar_Get( "cg_gun", "1", CVAR_ARCHIVE );
	cg_gunx =       Cvar_Get( "cg_gunx", "0", CVAR_ARCHIVE );
	cg_guny =       Cvar_Get( "cg_guny", "0", CVAR_ARCHIVE );
	cg_gunz =       Cvar_Get( "cg_gunz", "0", CVAR_ARCHIVE );
	cg_gunbob =     Cvar_Get( "cg_gunbob", "1", CVAR_ARCHIVE );

	cg_gun_fov =        Cvar_Get( "cg_gun_fov", "75", CVAR_ARCHIVE );
	cg_weaponFlashes =  Cvar_Get( "cg_weaponFlashes", "2", CVAR_ARCHIVE );

	// wsw
	cg_volume_players = Cvar_Get( "cg_volume_players", "1.0", CVAR_ARCHIVE );
	cg_volume_effects = Cvar_Get( "cg_volume_effects", "1.0", CVAR_ARCHIVE );
	cg_volume_announcer =   Cvar_Get( "cg_volume_announcer", "1.0", CVAR_ARCHIVE );
	cg_volume_hitsound =    Cvar_Get( "cg_volume_hitsound", "1.0", CVAR_ARCHIVE );
	cg_volume_voicechats =  Cvar_Get( "cg_volume_voicechats", "1.0", CVAR_ARCHIVE );
	cg_handOffset =     Cvar_Get( "cg_handOffset", "5", CVAR_ARCHIVE );
	cg_projectileFireTrail =    Cvar_Get( "cg_projectileFireTrail", "140", CVAR_ARCHIVE );
	cg_bloodTrail =     Cvar_Get( "cg_bloodTrail", "10", CVAR_ARCHIVE );
	cg_showBloodTrail = Cvar_Get( "cg_showBloodTrail", "1", CVAR_ARCHIVE );
	cg_projectileFireTrailAlpha =   Cvar_Get( "cg_projectileFireTrailAlpha", "0.45", CVAR_ARCHIVE );
	cg_bloodTrailAlpha =    Cvar_Get( "cg_bloodTrailAlpha", "1.0", CVAR_ARCHIVE );
	cg_explosionsRing = Cvar_Get( "cg_explosionsRing", "0", CVAR_ARCHIVE );
	cg_explosionsDust =    Cvar_Get( "cg_explosionsDust", "0", CVAR_ARCHIVE );
	cg_showObituaries = Cvar_Get( "cg_showObituaries", va( "%i", CG_OBITUARY_HUD | CG_OBITUARY_CENTER ), CVAR_ARCHIVE );
	cg_damageNumbers = Cvar_Get( "cg_damageNumbers", "1", CVAR_ARCHIVE );
	cg_autoaction_demo =    Cvar_Get( "cg_autoaction_demo", "0", CVAR_ARCHIVE );
	cg_autoaction_screenshot =  Cvar_Get( "cg_autoaction_screenshot", "0", CVAR_ARCHIVE );
	cg_autoaction_spectator = Cvar_Get( "cg_autoaction_spectator", "0", CVAR_ARCHIVE );
	cg_particles =      Cvar_Get( "cg_particles", "1", CVAR_ARCHIVE );

	cg_cartoonEffects =     Cvar_Get( "cg_cartoonEffects", "7", CVAR_ARCHIVE );

	cg_voiceChats =     Cvar_Get( "cg_voiceChats", "1", CVAR_ARCHIVE );

	cg_projectileAntilagOffset = Cvar_Get( "cg_projectileAntilagOffset", "1.0", CVAR_ARCHIVE );

	cg_chatFilter =     Cvar_Get( "cg_chatFilter", "0", CVAR_ARCHIVE );

	// developer cvars
	cg_showClamp = Cvar_Get( "cg_showClamp", "0", CVAR_DEVELOPER );

	cg_allyColor = Cvar_Get( "cg_allyColor", "0", CVAR_ARCHIVE );
	cg_allyModel = Cvar_Get( "cg_allyModel", "bigvic", CVAR_ARCHIVE );
	cg_allyForceModel = Cvar_Get( "cg_allyForceModel", "1", CVAR_ARCHIVE | CVAR_READONLY );
	cg_allyModel->modified = true;
	cg_allyForceModel->modified = true;

	cg_enemyColor = Cvar_Get( "cg_enemyColor", "1", CVAR_ARCHIVE );
	cg_enemyModel = Cvar_Get( "cg_enemyModel", "padpork", CVAR_ARCHIVE );
	cg_enemyForceModel = Cvar_Get( "cg_enemyForceModel", "1", CVAR_ARCHIVE | CVAR_READONLY );
	cg_enemyModel->modified = true;
	cg_enemyForceModel->modified = true;

	Cvar_Get( "cg_loadout", "", CVAR_ARCHIVE | CVAR_USERINFO );
}

/*
* CG_Precache
*/
void CG_Precache( void ) {
	if( cgs.precacheDone ) {
		return;
	}

	cgs.precacheStart = cgs.precacheCount;
	cgs.precacheStartMsec = Sys_Milliseconds();

	{
		const char * name = cgs.configStrings[ CS_WORLDMODEL ];
		Span< const char > ext = FileExtension( name );

		u64 hash = Hash64( name, strlen( name ) - ext.n );
		cgs.map = FindMapMetadata( StringHash( hash ) );
	}

	CG_RegisterModels();
	if( cgs.precacheModelsStart < MAX_MODELS ) {
		return;
	}

	CG_RegisterSounds();
	if( cgs.precacheSoundsStart < MAX_SOUNDS ) {
		return;
	}

	CG_RegisterShaders();
	if( cgs.precacheShadersStart < MAX_IMAGES ) {
		return;
	}

	CG_RegisterClients();
	if( cgs.precacheClientsStart < MAX_CLIENTS ) {
		return;
	}

	cgs.precacheDone = true;
}

/*
* CG_RegisterConfigStrings
*/
static void CG_RegisterConfigStrings( void ) {
	int i;
	const char *cs;

	cgs.precacheCount = cgs.precacheTotal = 0;

	for( i = 0; i < MAX_CONFIGSTRINGS; i++ ) {
		trap_GetConfigString( i, cgs.configStrings[i], MAX_CONFIGSTRING_CHARS );

		cs = cgs.configStrings[i];
		if( !cs[0] ) {
			continue;
		}

		if( i == CS_WORLDMODEL ) {
			cgs.precacheTotal++;
		} else if( i >= CS_MODELS && i < CS_MODELS + MAX_MODELS ) {
			cgs.precacheTotal++;
		} else if( i >= CS_SOUNDS && i < CS_SOUNDS + MAX_SOUNDS ) {
			cgs.precacheTotal++;
		} else if( i >= CS_IMAGES && i < CS_IMAGES + MAX_IMAGES ) {
			cgs.precacheTotal++;
		} else if( i >= CS_PLAYERINFOS && i < CS_PLAYERINFOS + MAX_CLIENTS ) {
			cgs.precacheTotal++;
		}
	}

	// backup initial configstrings for CG_Reset
	memcpy( &cgs.baseConfigStrings[0][0], &cgs.configStrings[0][0], MAX_CONFIGSTRINGS*MAX_CONFIGSTRING_CHARS );

	CG_SC_AutoRecordAction( cgs.configStrings[CS_AUTORECORDSTATE] );
}

/*
* CG_Reset
*/
void CG_Reset( void ) {
	memcpy( &cgs.configStrings[0][0], &cgs.baseConfigStrings[0][0], MAX_CONFIGSTRINGS*MAX_CONFIGSTRING_CHARS );

	CG_ResetClientInfos();

	CG_ResetPModels();

	CG_ResetKickAngles();

	CG_SC_ResetObituaries();

	CG_ClearLocalEntities();

	// start up announcer events queue from clean
	CG_ClearAnnouncerEvents();

	CG_ClearInputState();

	CG_ClearPointedNum();

	CG_ClearAwards();

	chaseCam.key_pressed = false;

	// reset prediction optimization
	cg.predictFrom = 0;

	memset( cg_entities, 0, sizeof( cg_entities ) );
}

/*
* CG_Init
*/
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
	CG_PModelsInit();
	CG_WModelsInit();

	CG_ScreenInit();

	CG_ClearLocalEntities();

	CG_InitDamageNumbers();

	// get configstrings
	CG_RegisterConfigStrings();

	// register fonts here so loading screen works
	CG_RegisterFonts();
	cgs.white_material = FindMaterial( "$whiteimage" );

	CG_RegisterCGameCommands();

	CG_InitHUD();

	InitParticles();
	InitPersistentBeams();
	InitGibs();

	CG_InitChat();

	// start up announcer events queue from clean
	CG_ClearAnnouncerEvents();

	cg.firstFrame = true; // think of the next frame in CG_NewFrameSnap as of the first one

	// now that we're done with precaching, let the autorecord actions do something
	CG_ConfigString( CS_AUTORECORDSTATE, cgs.configStrings[CS_AUTORECORDSTATE] );

	CG_DemocamInit();
}

void CG_Shutdown() {
	CG_FreeLocalEntities();
	CG_DemocamShutdown();
	CG_UnregisterCGameCommands();
	CG_PModelsShutdown();
	CG_ShutdownChat();
	CG_ShutdownInput();
	CG_ShutdownHUD();
	ShutdownParticles();

	CG_Free( const_cast< char * >( cgs.serverName ) );

	Mem_FreePool( &cg_mempool );
}
