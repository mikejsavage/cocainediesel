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

#include "cgame/cg_local.h"

static StringHash GetPlayerSound( int entnum, PlayerSound ps ) {
	const PlayerModelMetadata * meta = GetPlayerModelMetadata( entnum );
	if( meta == NULL ) {
		Com_Printf( "Player model metadata is null\n" );
		return EMPTY_HASH;
	}
	return meta->sounds[ ps ];
}

void CG_PlayerSound( int entnum, int entchannel, PlayerSound ps ) {
	bool fixed = ( entchannel & CHAN_FIXED ) != 0;
	entchannel &= ~CHAN_FIXED;

	StringHash sfx = GetPlayerSound( entnum, ps );
	if( fixed ) {
		S_StartFixedSound( sfx, cg_entities[entnum].current.origin, entchannel, 1.0f );
	}
	else if( ISVIEWERENTITY( entnum ) ) {
		S_StartGlobalSound( sfx, entchannel, 1.0f );
	}
	else {
		S_StartEntitySound( sfx, entnum, entchannel, 1.0f );
	}
}

static void CG_ParseClientInfo( cg_clientInfo_t *ci, const char *info ) {
	assert( ci );
	assert( info );

	if( !Info_Validate( info ) ) {
		Com_Error( ERR_DROP, "Invalid client info" );
	}

	char *s = Info_ValueForKey( info, "name" );
	Q_strncpyz( ci->name, s && s[0] ? s : "badname", sizeof( ci->name ) );

	s = Info_ValueForKey( info, "hand" );
	ci->hand = s && s[0] ? atoi( s ) : 2;
}

/*
* CG_LoadClientInfo
* Updates cached client info from the current CS_PLAYERINFOS configstring value
*/
void CG_LoadClientInfo( int client ) {
	assert( client >= 0 && client < client_gs.maxclients );
	CG_ParseClientInfo( &cgs.clientInfo[client], cgs.configStrings[CS_PLAYERINFOS + client] );
}

void CG_ResetClientInfos() {
	memset( cgs.clientInfo, 0, sizeof( cgs.clientInfo ) );

	for( int i = 0; i < MAX_CLIENTS; i++ ) {
		if( strlen( cgs.configStrings[ CS_PLAYERINFOS + i ] ) > 0 ) {
			CG_LoadClientInfo( i );
		}
	}
}
