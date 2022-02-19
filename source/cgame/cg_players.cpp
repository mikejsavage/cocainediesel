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
	assert( meta != NULL );
	if( meta == NULL ) {
		Com_Printf( "Player model metadata is null\n" );
		return EMPTY_HASH;
	}
	return meta->sounds[ ps ];
}

float CG_PlayerPitch( int entnum ) {
	static const float basis = Length( Vec3( 1 ) );
	return 1.0f / ( Length( cg_entities[ entnum ].current.scale ) / basis );
}

void CG_PlayerSound( int entnum, int entchannel, PlayerSound ps ) {
	StringHash sfx = GetPlayerSound( entnum, ps );

	float pitch = 1.0f;
	if( ps == PlayerSound_Death || ps == PlayerSound_Void || ps == PlayerSound_Pain25 || ps == PlayerSound_Pain50 || ps == PlayerSound_Pain75 || ps == PlayerSound_Pain100 || ps == PlayerSound_WallJump ) {
		pitch = CG_PlayerPitch( entnum );
	}

	if( ISVIEWERENTITY( entnum ) ) {
		S_StartGlobalSound( sfx, entchannel, 1.0f, pitch );
	}
	else {
		S_StartEntitySound( sfx, entnum, entchannel, 1.0f, pitch );
	}
}
