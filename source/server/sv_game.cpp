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

#include "server/server.h"
#include "qcommon/cmodel.h"

void PF_DropClient( edict_t *ent, const char *message ) {
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
		SV_DropClient( drop, "%s", message );
	} else {
		SV_DropClient( drop, NULL );
	}
}

int PF_GetClientState( int numClient ) {
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
void PF_GameCmd( edict_t * ent, const char * cmd ) {
	if( ent != NULL ) {
		client_t * client = &svs.clients[ NUM_FOR_EDICT( ent ) - 1 ];
		SV_AddGameCommand( client, cmd );
		return;
	}

	for( int i = 0; i < sv_maxclients->integer; i++ ) {
		client_t * client = &svs.clients[ i ];
		SV_AddGameCommand( client, cmd );
	}
}

void PF_ConfigString( int index, const char *val ) {
	if( !val ) {
		return;
	}

	if( index < 0 || index >= MAX_CONFIGSTRINGS ) {
		Fatal( "configstring: bad index %i", index );
	}

	size_t len = strlen( val );
	if( len >= sizeof( sv.configstrings[0] ) ) {
		Com_Printf( "WARNING: 'PF_Configstring', configstring %i overflowed (%zu)\n", index, strlen( val ) );
		len = sizeof( sv.configstrings[0] ) - 1;
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

const char *PF_GetConfigString( int index ) {
	if( index < 0 || index >= MAX_CONFIGSTRINGS ) {
		return NULL;
	}

	return sv.configstrings[ index ];
}

//==============================================

/*
* SV_ShutdownGameProgs
*
* Called when either the entire server is being killed, or
* it is changing to a different game directory.
*/
void SV_ShutdownGameProgs() {
	G_Shutdown();
}

void SV_LocateEntities( edict_t *edicts, int num_edicts, int max_edicts ) {
	sv.gi.edicts = edicts;
	sv.gi.clients = svs.clients;
	sv.gi.num_edicts = num_edicts;
	sv.gi.max_edicts = max_edicts;
	sv.gi.max_clients = Min2( num_edicts, sv_maxclients->integer );
}

/*
* SV_InitGameProgs
*
* Init the game subsystem for a new map
*/
void SV_InitGameProgs() {
	SV_SetServerConfigStrings();

	G_Init( svc.snapFrameTime );
}
