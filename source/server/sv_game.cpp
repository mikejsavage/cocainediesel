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

#include "server.h"
#include "qcommon/version.h"

game_export_t *ge;

//======================================================================

// PF versions of the CM functions passed to the game module
// they only add svs.cms as the first parameter

//======================================================================

static inline int PF_CM_TransformedPointContents( const vec3_t p, struct cmodel_s *cmodel, const vec3_t origin, const vec3_t angles ) {
	return CM_TransformedPointContents( svs.cms, p, cmodel, origin, angles );
}

static inline void PF_CM_TransformedBoxTrace( trace_t *tr, const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs,
											  struct cmodel_s *cmodel, int brushmask, const vec3_t origin, const vec3_t angles ) {
	CM_TransformedBoxTrace( svs.cms, tr, start, end, mins, maxs, cmodel, brushmask, origin, angles );
}

static inline int PF_CM_NumInlineModels( void ) {
	return CM_NumInlineModels( svs.cms );
}

static inline struct cmodel_s *PF_CM_InlineModel( int num ) {
	return CM_InlineModel( svs.cms, num );
}

static inline void PF_CM_InlineModelBounds( const struct cmodel_s *cmodel, vec3_t mins, vec3_t maxs ) {
	CM_InlineModelBounds( svs.cms, cmodel, mins, maxs );
}

static inline struct cmodel_s *PF_CM_ModelForBBox( vec3_t mins, vec3_t maxs ) {
	return CM_ModelForBBox( svs.cms, mins, maxs );
}

static inline struct cmodel_s *PF_CM_OctagonModelForBBox( vec3_t mins, vec3_t maxs ) {
	return CM_OctagonModelForBBox( svs.cms, mins, maxs );
}

static inline bool PF_CM_AreasConnected( int area1, int area2 ) {
	return CM_AreasConnected( svs.cms, area1, area2 );
}

static inline void PF_CM_SetAreaPortalState( int area, int otherarea, bool open ) {
	CM_SetAreaPortalState( svs.cms, area, otherarea, open );
}

static inline int PF_CM_BoxLeafnums( vec3_t mins, vec3_t maxs, int *list, int listsize, int *topnode ) {
	return CM_BoxLeafnums( svs.cms, mins, maxs, list, listsize, topnode );
}

static inline int PF_CM_LeafCluster( int leafnum ) {
	return CM_LeafCluster( svs.cms, leafnum );
}

static inline int PF_CM_LeafArea( int leafnum ) {
	return CM_LeafArea( svs.cms, leafnum );
}

static inline int PF_CM_LeafsInPVS( int leafnum1, int leafnum2 ) {
	return CM_LeafsInPVS( svs.cms, leafnum1, leafnum2 );
}

//======================================================================

/*
* PF_DropClient
*/
static void PF_DropClient( edict_t *ent, int type, const char *message ) {
	int p;
	client_t *drop;

	if( !ent ) {
		return;
	}

	p = NUM_FOR_EDICT( ent );
	if( p < 1 || p > sv_maxclients->integer ) {
		return;
	}

	drop = svs.clients + ( p - 1 );
	if( message ) {
		SV_DropClient( drop, type, "%s", message );
	} else {
		SV_DropClient( drop, type, NULL );
	}
}

/*
* PF_GetClientState
*
* Game code asks for the state of this client
*/
static int PF_GetClientState( int numClient ) {
	if( numClient < 0 || numClient >= sv_maxclients->integer ) {
		return -1;
	}
	return svs.clients[numClient].state;
}

/*
* PF_GameCmd
*
* Sends the server command to clients.
* If ent is NULL the command will be sent to all connected clients
*/
static void PF_GameCmd( edict_t *ent, const char *cmd ) {
	int i;
	client_t *client;

	if( !cmd || !cmd[0] ) {
		return;
	}

	if( !ent ) {
		for( i = 0, client = svs.clients; i < sv_maxclients->integer; i++, client++ ) {
			if( client->state < CS_SPAWNED ) {
				continue;
			}
			SV_AddGameCommand( client, cmd );
		}
	} else {
		i = NUM_FOR_EDICT( ent );
		if( i < 1 || i > sv_maxclients->integer ) {
			return;
		}

		client = svs.clients + ( i - 1 );
		if( client->state < CS_SPAWNED ) {
			return;
		}

		SV_AddGameCommand( client, cmd );
	}
}

/*
* PF_dprint
*
* Debug print to server console
*/
static void PF_dprint( const char *msg ) {
	char copy[MAX_PRINTMSG], *end = copy + sizeof( copy );
	char *out = copy;
	const char *in = msg;

	if( !msg ) {
		return;
	}

	// don't allow control chars except for \n
	for( ; *in && out < end - 1; in++ ) {
		if( ( unsigned char )*in >= ' ' || *in == '\n' ) {
			*out++ = *in;
		}
	}
	*out = '\0';

	Com_Printf( "%s", copy );
}

#ifndef _MSC_VER
static void PF_error( const char *msg ) __attribute__( ( noreturn ) );
#else
__declspec( noreturn ) static void PF_error( const char *msg );
#endif

/*
* PF_error
*
* Abort the server with a game error
*/
static void PF_error( const char *msg ) {
	char copy[MAX_PRINTMSG];

	if( !msg ) {
		Com_Error( ERR_DROP, "Game Error: unknown error" );
	}

	// mask off high bits and colored strings
	size_t i;
	for( i = 0; i < sizeof( copy ) - 1 && msg[i]; i++ )
		copy[i] = msg[i] & 127;
	copy[i] = 0;

	Com_Error( ERR_DROP, "Game Error: %s", copy );
}

/*
* PF_Configstring
*/
static void PF_ConfigString( int index, const char *val ) {
	size_t len;

	if( !val ) {
		return;
	}

	if( index < 0 || index >= MAX_CONFIGSTRINGS ) {
		Com_Error( ERR_DROP, "configstring: bad index %i", index );
	}

	if( index < SERVER_PROTECTED_CONFIGSTRINGS ) {
		Com_Printf( "WARNING: 'PF_Configstring', configstring %i is server protected\n", index );
		return;
	}

	len = strlen( val );
	if( len >= sizeof( sv.configstrings[0] ) ) {
		Com_Printf( "WARNING: 'PF_Configstring', configstring %i overflowed (%" PRIuPTR ")\n", index, (uintptr_t)strlen( val ) );
		len = sizeof( sv.configstrings[0] ) - 1;
	}

	if( !COM_ValidateConfigstring( val ) ) {
		Com_Printf( "WARNING: 'PF_Configstring' invalid configstring %i: %s\n", index, val );
		return;
	}

	// ignore if no changes
	if( !strncmp( sv.configstrings[index], val, len ) && sv.configstrings[index][len] == '\0' ) {
		return;
	}

	// change the string in sv
	Q_strncpyz( sv.configstrings[index], val, sizeof( sv.configstrings[index] ) );

	if( sv.state != ss_loading ) {
		SV_SendServerCommand( NULL, "cs %i \"%s\"", index, val );
	}
}

static const char *PF_GetConfigString( int index ) {
	if( index < 0 || index >= MAX_CONFIGSTRINGS ) {
		return NULL;
	}

	return sv.configstrings[ index ];
}

/*
* PF_inPVS
*/
static bool PF_inPVS( const vec3_t p1, const vec3_t p2 ) {
	return CM_InPVS( svs.cms, p1, p2 );
}

//==============================================

/*
* SV_ShutdownGameProgs
*
* Called when either the entire server is being killed, or
* it is changing to a different game directory.
*/
void SV_ShutdownGameProgs( void ) {
	if( !ge ) {
		return;
	}

	ge->Shutdown();
	ge = NULL;
}

/*
* SV_LocateEntities
*/
static void SV_LocateEntities( struct edict_s *edicts, size_t edict_size, int num_edicts, int max_edicts ) {
	if( !edicts || edict_size < sizeof( entity_shared_t ) ) {
		Com_Error( ERR_DROP, "SV_LocateEntities: bad edicts" );
	}

	sv.gi.edicts = edicts;
	sv.gi.clients = svs.clients;
	sv.gi.edict_size = edict_size;
	sv.gi.num_edicts = num_edicts;
	sv.gi.max_edicts = max_edicts;
	sv.gi.max_clients = min( num_edicts, sv_maxclients->integer );
}

/*
* SV_InitGameProgs
*
* Init the game subsystem for a new map
*/
void SV_InitGameProgs( void ) {
	game_import_t import;

	// unload anything we have now
	if( ge ) {
		SV_ShutdownGameProgs();
	}

	// load a new game dll
	import.Print = PF_dprint;
	import.Error = PF_error;
	import.GameCmd = PF_GameCmd;

	import.inPVS = PF_inPVS;

	import.CM_TransformedPointContents = PF_CM_TransformedPointContents;
	import.CM_TransformedBoxTrace = PF_CM_TransformedBoxTrace;
	import.CM_NumInlineModels = PF_CM_NumInlineModels;
	import.CM_InlineModel = PF_CM_InlineModel;
	import.CM_InlineModelBounds = PF_CM_InlineModelBounds;
	import.CM_ModelForBBox = PF_CM_ModelForBBox;
	import.CM_OctagonModelForBBox = PF_CM_OctagonModelForBBox;
	import.CM_AreasConnected = PF_CM_AreasConnected;
	import.CM_SetAreaPortalState = PF_CM_SetAreaPortalState;
	import.CM_BoxLeafnums = PF_CM_BoxLeafnums;
	import.CM_LeafCluster = PF_CM_LeafCluster;
	import.CM_LeafArea = PF_CM_LeafArea;
	import.CM_LeafsInPVS = PF_CM_LeafsInPVS;

	import.Milliseconds = Sys_Milliseconds;

	import.ModelIndex = SV_ModelIndex;
	import.SoundIndex = SV_SoundIndex;
	import.ImageIndex = SV_ImageIndex;

	import.ConfigString = PF_ConfigString;
	import.GetConfigString = PF_GetConfigString;

	import.FS_FOpenFile = FS_FOpenFile;
	import.FS_Read = FS_Read;
	import.FS_Write = FS_Write;
	import.FS_FCloseFile = FS_FCloseFile;

	import.Cvar_Get = Cvar_Get;
	import.Cvar_Set = Cvar_Set;
	import.Cvar_SetValue = Cvar_SetValue;
	import.Cvar_ForceSet = Cvar_ForceSet;
	import.Cvar_Value = Cvar_Value;
	import.Cvar_String = Cvar_String;

	import.Cmd_Argc = Cmd_Argc;
	import.Cmd_Argv = Cmd_Argv;
	import.Cmd_Args = Cmd_Args;
	import.Cmd_AddCommand = Cmd_AddCommand;
	import.Cmd_RemoveCommand = Cmd_RemoveCommand;

	import.ML_Update = ML_Update;
	import.ML_GetMapByNum = ML_GetMapByNum;
	import.ML_FilenameExists = ML_FilenameExists;

	import.Cmd_ExecuteText = Cbuf_ExecuteText;
	import.Cbuf_Execute = Cbuf_Execute;

	import.FakeClientConnect = SVC_FakeConnect;
	import.DropClient = PF_DropClient;
	import.GetClientState = PF_GetClientState;
	import.ExecuteClientThinks = SV_ExecuteClientThinks;

	import.LocateEntities = SV_LocateEntities;

	import.is_dedicated_server = is_dedicated_server;

	ge = GetGameAPI( &import );

	SV_SetServerConfigStrings();

	ge->Init( time( NULL ), svc.snapFrameTime );
}
