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

#include "cg_local.h"

static constexpr const char * cg_defaultSexedSounds[] = {
	"*death",
	"*fall_0", "*fall_1", "*fall_2",
	"*jump_1", "*jump_2",
	"*pain25", "*pain50", "*pain75", "*pain100",
	"*wj_1", "*wj_2",
	"*dash_1", "*dash_2",
};

/*
* CG_RegisterPmodelSexedSound
*/
static struct sfx_s *CG_RegisterPmodelSexedSound( PlayerModelMetadata *metadata, const char *name ) {
	if( !metadata ) {
		return NULL;
	}

	for( const cg_sexedSfx_t * sexedSfx = metadata->sexedSfx; sexedSfx; sexedSfx = sexedSfx->next ) {
		if( !Q_stricmp( sexedSfx->name, name ) ) {
			return sexedSfx->sfx;
		}
	}

	// find out what's the model name
	const char * model_name = metadata->name;
	const char * p = strrchr( model_name, '/' );
	if( p != NULL ) {
		model_name = p + 1;
	}

	// if we can't figure it out, they're DEFAULT_PLAYERMODEL
	if( !model_name[0] ) {
		model_name = DEFAULT_PLAYERMODEL;
	}

	cg_sexedSfx_t * sexedSfx = ( cg_sexedSfx_t * )CG_Malloc( sizeof( cg_sexedSfx_t ) );
	sexedSfx->name = name;
	sexedSfx->next = metadata->sexedSfx;
	metadata->sexedSfx = sexedSfx;

	char sexedFilename[MAX_QPATH];
	Q_snprintfz( sexedFilename, sizeof( sexedFilename ), "sounds/players/%s/%s", model_name, name + 1 );
	sexedSfx->sfx = trap_S_RegisterSound( sexedFilename );

	return sexedSfx->sfx;
}

/*
* CG_UpdateSexedSoundsRegistration
*/
void CG_UpdateSexedSoundsRegistration( PlayerModelMetadata *metadata ) {
	cg_sexedSfx_t *sexedSfx, *next;

	if( !metadata ) {
		return;
	}

	// free loaded sounds
	for( sexedSfx = metadata->sexedSfx; sexedSfx; sexedSfx = next ) {
		next = sexedSfx->next;
		CG_Free( sexedSfx );
	}
	metadata->sexedSfx = NULL;

	// load default sounds
	for( const char * name : cg_defaultSexedSounds ) {
		CG_RegisterPmodelSexedSound( metadata, name );
	}
}

/*
* CG_RegisterSexedSound
*/
struct sfx_s *CG_RegisterSexedSound( int entnum, const char *name ) {
	if( entnum < 0 || entnum >= MAX_EDICTS ) {
		return NULL;
	}
	return CG_RegisterPmodelSexedSound( cg_entPModels[entnum].metadata, name );
}

/*
* CG_SexedSound
*/
void CG_SexedSound( int entnum, int entchannel, const char *name, float volume, float attn ) {
	bool fixed;

	fixed = entchannel & CHAN_FIXED ? true : false;
	entchannel &= ~CHAN_FIXED;

	if( fixed ) {
		trap_S_StartFixedSound( CG_RegisterSexedSound( entnum, name ), cg_entities[entnum].current.origin, entchannel, volume, attn );
	} else if( ISVIEWERENTITY( entnum ) ) {
		trap_S_StartGlobalSound( CG_RegisterSexedSound( entnum, name ), entchannel, volume );
	} else {
		trap_S_StartEntitySound( CG_RegisterSexedSound( entnum, name ), entnum, entchannel, volume, attn );
	}
}

/*
* CG_ParseClientInfo
*/
static void CG_ParseClientInfo( cg_clientInfo_t *ci, const char *info ) {
	char *s;

	assert( ci );
	assert( info );

	if( !Info_Validate( info ) ) {
		CG_Error( "Invalid client info" );
	}

	s = Info_ValueForKey( info, "name" );
	Q_strncpyz( ci->name, s && s[0] ? s : "badname", sizeof( ci->name ) );

	// name with color tokes stripped
	Q_strncpyz( ci->cleanname, COM_RemoveColorTokens( ci->name ), sizeof( ci->cleanname ) );

	s = Info_ValueForKey( info, "hand" );
	ci->hand = s && s[0] ? atoi( s ) : 2;
}

/*
* CG_LoadClientInfo
* Updates cached client info from the current CS_PLAYERINFOS configstring value
*/
void CG_LoadClientInfo( int client ) {
	assert( client >= 0 && client < gs.maxclients );
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
