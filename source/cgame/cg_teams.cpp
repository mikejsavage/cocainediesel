/*
Copyright (C) 2006 Pekka Lampila ("Medar"), Damien Deville ("Pb")
and German Garcia Fernandez ("Jal") for Chasseur de bots association.


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
#include "client/renderer/srgb.h"

bool CG_IsAlly( int team ) {
	if( team == TEAM_ALLY || team == TEAM_ENEMY )
		return team == TEAM_ALLY;

	int myteam = cg.predictedPlayerState.team;
	if( myteam == TEAM_SPECTATOR )
		return team == TEAM_ALPHA;
	if( myteam == TEAM_PLAYERS )
		return false;
	return team == myteam;
}

RGB8 CG_TeamColor( int team ) {
	if( team == TEAM_PLAYERS )
		return RGB8( 255, 255, 255 );

	return CG_IsAlly( team ) ? TEAM_COLORS[ 0 ] : TEAM_COLORS[ 1 ];
}

Vec4 CG_TeamColorVec4( int team ) {
	return Vec4( sRGBToLinear( CG_TeamColor( team ) ), 1.0f );
}
