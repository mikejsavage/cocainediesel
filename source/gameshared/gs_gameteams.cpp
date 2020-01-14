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

#include "qcommon/base.h"
#include "gameshared/q_arch.h"

static const char *gs_teamNames[] = {
	"SPECTATOR",
	"PLAYERS",
	"COCAINE",
	"DIESEL",
};

const char *GS_TeamName( int team ) {
	if( team < 0 || team >= int( ARRAY_COUNT( gs_teamNames ) ) ) {
		return NULL;
	}
	return gs_teamNames[team];
}

int GS_TeamFromName( const char *teamname ) {
	if( !teamname || !teamname[0] ) {
		return -1; // invalid
	}

	for( int i = 0; i < int( ARRAY_COUNT( gs_teamNames ) ); i++ ) {
		if( !Q_stricmp( gs_teamNames[i], teamname ) ) {
			return i;
		}
	}

	return -1; // invalid
}
