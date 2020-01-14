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

/*
* G_ClientUpdateScoreBoardMessage
*
* Show the scoreboard messages if the scoreboards are active
*/
void G_UpdateScoreBoardMessages( void ) {
	char as_scoreboard[ 1024 ];
	GT_asCallScoreboardMessage( as_scoreboard, sizeof( as_scoreboard ) );

	String< 1024 > scoreboard( "scb \"{}", as_scoreboard );

	// add spectators
	scoreboard.append( " {}", teamlist[ TEAM_SPECTATOR ].numplayers );
	for( int i = 0; i < teamlist[TEAM_SPECTATOR].numplayers; i++ ) {
		const edict_t * e = game.edicts + teamlist[TEAM_SPECTATOR].playerIndices[i];
		scoreboard.append( " {}", PLAYERNUM( e ) );
	}

	scoreboard += '"';

	// send to players who have scoreboard visible
	for( int i = 0; i < server_gs.maxclients; i++ ) {
		edict_t * ent = game.edicts + 1 + i;
		if( !ent->r.inuse || !ent->r.client ) {
			continue;
		}

		gclient_t * client = ent->r.client;

		if( svs.realtime <= client->level.scoreboard_time + scoreboardInterval ) {
			continue;
		}

		if( client->ps.show_scoreboard ) {
			client->level.scoreboard_time = svs.realtime + scoreboardInterval - ( svs.realtime % scoreboardInterval );
			trap_GameCmd( ent, scoreboard.c_str() );
		}
	}
}

//=======================================================================

static unsigned int G_FindPointedPlayer( edict_t *self ) {
	trace_t trace;
	int i, j, bestNum = 0;
	vec3_t boxpoints[8];
	float value, dist, value_best = 0.90f;   // if nothing better is found, print nothing
	edict_t *other;
	vec3_t vieworg, dir, viewforward;

	if( G_IsDead( self ) ) {
		return 0;
	}

	// we can't handle the thirdperson modifications in server side :/
	VectorSet( vieworg, self->r.client->ps.pmove.origin[0], self->r.client->ps.pmove.origin[1], self->r.client->ps.pmove.origin[2] + self->r.client->ps.viewheight );
	AngleVectors( self->r.client->ps.viewangles, viewforward, NULL, NULL );

	for( i = 0; i < server_gs.maxclients; i++ ) {
		other = PLAYERENT( i );
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

		VectorSubtract( other->s.origin, self->s.origin, dir );
		dist = VectorNormalize2( dir, dir );
		if( dist > 1000 ) {
			continue;
		}

		value = DotProduct( dir, viewforward );

		if( value > value_best ) {
			BuildBoxPoints( boxpoints, other->s.origin, tv( 4, 4, 4 ), tv( 4, 4, 4 ) );
			for( j = 0; j < 8; j++ ) {
				G_Trace( &trace, vieworg, vec3_origin, vec3_origin, boxpoints[j], self, MASK_SHOT | MASK_OPAQUE );
				if( trace.ent && trace.ent == ENTNUM( other ) ) {
					value_best = value;
					bestNum = ENTNUM( other );
				}
			}
		}
	}

	return bestNum;
}

/*
* G_SetClientStats
*/
void G_SetClientStats( edict_t * ent ) {
	gclient_t * client = ent->r.client;
	SyncPlayerState * ps = &client->ps;

	ps->show_scoreboard = ent->r.client->level.showscores;
	ps->ready = GS_MatchState( &server_gs ) <= MATCH_STATE_WARMUP && level.ready[ PLAYERNUM( ent ) ];
	ps->voted = G_Callvotes_HasVoted( ent );
	ps->team = ent->s.team;
	ps->real_team = ent->s.team;
	ps->health = ent->s.team == TEAM_SPECTATOR ? 0 : HEALTH_TO_INT( ent->health );

	ps->pointed_player = 0;
	ps->pointed_health = 0;
	if( GS_TeamBasedGametype( &server_gs ) ) {
		unsigned int pointed = G_FindPointedPlayer( ent );
		edict_t * e = &game.edicts[ pointed ];
		if( e->s.team == ent->s.team ) {
			ps->pointed_player = pointed;
			ps->pointed_health = HEALTH_TO_INT( e->health );
		}
	}

	ps->last_killer = 0;
	if( client->teamstate.last_killer ) {
		edict_t * attacker = client->teamstate.last_killer;
		ps->last_killer = attacker->r.client ? ENTNUM( attacker ) : 0;
	}
}
