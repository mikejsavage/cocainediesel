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

/*
* CG_SexedSound
*/
void CG_SexedSound( int entnum, int entchannel, const char *name, float volume, float attn ) {
	/* TODO TODO
	// Q_snprintfz( sexedFilename, sizeof( sexedFilename ), "sounds/players/%s/%s", model, oname + 1 );
	bool fixed;

	bool fixed = entchannel & CHAN_FIXED ? true : false;
	entchannel &= ~CHAN_FIXED;

	if( fixed ) {
		trap_S_StartFixedSound( CG_RegisterSexedSound( entnum, name ), cg_entities[entnum].current.origin, entchannel, volume, attn );
	} else if( ISVIEWERENTITY( entnum ) ) {
		trap_S_StartGlobalSound( CG_RegisterSexedSound( entnum, name ), entchannel, volume );
	} else {
		trap_S_StartEntitySound( CG_RegisterSexedSound( entnum, name ), entnum, entchannel, volume, attn );
	}
	*/
}

/*
* CG_ParseClientInfo
*/
static void CG_ParseClientInfo( cg_clientInfo_t *ci, const char *info ) {
	char *s;
	int rgbcolor;

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

	// color
	s = Info_ValueForKey( info, "color" );
	rgbcolor = s && s[0] ? COM_ReadColorRGBString( s ) : -1;
	if( rgbcolor != -1 ) {
		Vector4Set( ci->color, COLOR_R( rgbcolor ), COLOR_G( rgbcolor ), COLOR_B( rgbcolor ), 255 );
	} else {
		Vector4Set( ci->color, 255, 255, 255, 255 );
	}
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
