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

#include "qcommon/string.h"
#include "game/g_local.h"


//=======================================================================

static unsigned int G_FindPointedPlayer( edict_t *self ) {
	int best = 0;
	float value_best = 0.90f;

	if( G_IsDead( self ) ) {
		return 0;
	}

	// we can't handle the thirdperson modifications in server side :/
	Vec3 vieworg = self->r.client->ps.pmove.origin;
	vieworg.z += self->r.client->ps.viewheight;

	Vec3 viewforward;
	AngleVectors( self->r.client->ps.viewangles, &viewforward, NULL, NULL );

	for( int i = 0; i < server_gs.maxclients; i++ ) {
		edict_t * other = PLAYERENT( i );
		if( !other->r.inuse ) {
			continue;
		}
		if( !other->r.client ) {
			continue;
		}
		if( other == self ) {
			continue;
		}
		if( !other->r.solid || ( other->r.svflags & SVF_NOCLIENT ) ) {
			continue;
		}

		Vec3 dir = other->s.origin - self->s.origin;
		float dist = Length( dir );
		dir = SafeNormalize( dir );
		if( dist > 1000 ) {
			continue;
		}

		float value = Dot( dir, viewforward );

		if( value > value_best ) {
			Vec3 boxpoints[8];
			BuildBoxPoints( boxpoints, other->s.origin, Vec3( 4.0f ), Vec3( 4.0f ) );
			for( int j = 0; j < 8; j++ ) {
				trace_t trace;
				G_Trace( &trace, vieworg, Vec3( 0.0f ), Vec3( 0.0f ), boxpoints[j], self, MASK_SHOT | MASK_OPAQUE );
				if( trace.ent && trace.ent == ENTNUM( other ) ) {
					value_best = value;
					best = ENTNUM( other );
				}
			}
		}
	}

	return best;
}

/*
* G_SetClientStats
*/
void G_SetClientStats( edict_t * ent ) {
	gclient_t * client = ent->r.client;
	SyncPlayerState * ps = &client->ps;

	ps->ready = server_gs.gameState.match_state <= MATCH_STATE_WARMUP && level.ready[ PLAYERNUM( ent ) ];
	ps->voted = G_Callvotes_HasVoted( ent );
	ps->team = ent->s.team;
	ps->real_team = ent->s.team;
	ps->health = ent->s.team == TEAM_SPECTATOR ? 0 : HEALTH_TO_INT( ent->health );

	ps->pointed_player = 0;
	ps->pointed_health = 0;
	if( level.gametype.isTeamBased ) {
		unsigned int pointed = G_FindPointedPlayer( ent );
		edict_t * e = &game.edicts[ pointed ];
		if( e->s.team == ent->s.team ) {
			ps->pointed_player = pointed;
			ps->pointed_health = HEALTH_TO_INT( e->health );
		}
	}
}
