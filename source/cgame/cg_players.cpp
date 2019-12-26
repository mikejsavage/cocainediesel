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
#include "qcommon/string.h"

static constexpr const char * PLAYER_SOUND_NAMES[] = {
	"death",
	"jump",
	"pain25", "pain50", "pain75", "pain100",
	"walljump",
	"dash",
};

STATIC_ASSERT( ARRAY_COUNT( PLAYER_SOUND_NAMES ) == PlayerSound_Count );

void CG_RegisterPlayerSounds( PlayerModelMetadata * metadata, const char * name ) {
	for( size_t i = 0; i < ARRAY_COUNT( metadata->sounds ); i++ ) {
		TempAllocator temp = cls.frame_arena.temp();

		const char * p = strrchr( name, '/' );
		if( p != NULL ) {
			name = p + 1;
		}

		DynamicString path( &temp, "sounds/players/{}/{}", name, PLAYER_SOUND_NAMES[ i ] );
		metadata->sounds[ i ] = FindSoundEffect( path.c_str() );
	}
}

static const SoundEffect * GetPlayerSound( int entnum, PlayerSound ps ) {
	if( entnum < 0 || entnum >= MAX_EDICTS ) {
		return NULL;
	}
	return cg_entPModels[ entnum ].metadata->sounds[ ps ];
}

void CG_PlayerSound( int entnum, int entchannel, PlayerSound ps, float volume, float attn ) {
	bool fixed = entchannel & CHAN_FIXED ? true : false;
	entchannel &= ~CHAN_FIXED;

	const SoundEffect * sfx = GetPlayerSound( entnum, ps );
	if( fixed ) {
		S_StartFixedSound( sfx, FromQF3( cg_entities[entnum].current.origin ), entchannel, volume, attn );
	}
	else if( ISVIEWERENTITY( entnum ) ) {
		S_StartGlobalSound( sfx, entchannel, volume );
	}
	else {
		S_StartEntitySound( sfx, entnum, entchannel, volume, attn );
	}
}

/*
* CG_ParseClientInfo
*/
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

/*
* CG_ResetClientInfos
*/
void CG_ResetClientInfos( void ) {
	int i, cs;

	memset( cgs.clientInfo, 0, sizeof( cgs.clientInfo ) );

	for( i = 0, cs = CS_PLAYERINFOS + i; i < MAX_CLIENTS; i++, cs++ ) {
		if( cgs.configStrings[cs][0] ) {
			CG_LoadClientInfo( i );
		}
	}
}
