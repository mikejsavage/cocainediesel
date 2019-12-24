/*
Copyright (C) 2006-2007 Benjamin Litzelmann ("Kurim")
for Chasseur de bots association.

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

#include "g_local.h"

void G_PlayerAward( edict_t *ent, const char *awardMsg ) {
	edict_t *other;
	char cmd[MAX_STRING_CHARS];

	if( !awardMsg || !awardMsg[0] || !ent->r.client ) {
		return;
	}

	snprintf( cmd, sizeof( cmd ), "aw \"%s\"", awardMsg );
	trap_GameCmd( ent, cmd );

	G_Gametype_ScoreEvent( ent->r.client, "award", awardMsg );

	// add it to every player who's chasing this player
	for( other = game.edicts + 1; PLAYERNUM( other ) < server_gs.maxclients; other++ ) {
		if( !other->r.client || !other->r.inuse || !other->r.client->resp.chase.active ) {
			continue;
		}

		if( other->r.client->resp.chase.target == ENTNUM( ent ) ) {
			trap_GameCmd( other, cmd );
		}
	}
}

void G_AwardRaceRecord( edict_t *self ) {
	G_PlayerAward( self, S_COLOR_CYAN "New Record!" );
}
