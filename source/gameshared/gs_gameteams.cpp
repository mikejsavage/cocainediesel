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

#include "qcommon/types.h"
#include "gameshared/q_shared.h"
#include "gameshared/gs_synctypes.h"

static const char * gs_teamNames[] = {
	"SPECTATOR",
	"COCAINE",
	"DIESEL",
	"THREE",
	"FOUR",
};

const char * GS_TeamName( Team team ) {
	return gs_teamNames[ team ];
}

Team GS_TeamFromName( const char * name ) {
	for( int i = 0; i < int( ARRAY_COUNT( gs_teamNames ) ); i++ ) {
		if( StrCaseEqual( gs_teamNames[ i ], name ) ) {
			return Team( i );
		}
	}

	return Team_None;
}
