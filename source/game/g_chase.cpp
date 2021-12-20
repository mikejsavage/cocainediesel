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

#include "game/g_local.h"

static void G_Chase_SetChaseActive( edict_t *ent, bool active ) {
	ent->r.client->resp.chase.active = active;
}

static bool G_Chase_IsValidTarget( edict_t *ent, edict_t *target ) {
	bool teamonly = ent->s.team >= TEAM_ALPHA;

	if( !ent || !target ) {
		return false;
	}

	if( !target->r.inuse || !target->r.client || PF_GetClientState( PLAYERNUM( target ) ) < CS_SPAWNED ) {
		return false;
	}

	if( target->s.team < TEAM_PLAYERS || target->s.team >= GS_MAX_TEAMS || target == ent ) {
		return false;
	}

	if( teamonly && G_ISGHOSTING( target ) ) {
		return false;
	}

	if( teamonly && target->s.team != ent->s.team ) {
		return false;
	}

	if( G_ISGHOSTING( target ) && !target->deadflag && target->s.team != TEAM_SPECTATOR ) {
		return false; // ghosts that are neither dead, nor speccing (probably originating from gt-specific rules)
	}

	return true;
}

static void G_EndFrame_UpdateChaseCam( edict_t *ent ) {
	// not in chasecam
	if( !ent->r.client->resp.chase.active ) {
		return;
	}

	// is our chase target gone?
	edict_t * targ = &game.edicts[ent->r.client->resp.chase.target];

	if( !G_Chase_IsValidTarget( ent, targ ) ) {
		if( svs.realtime < ent->r.client->resp.chase.timeout ) { // wait for timeout
			return;
		}

		G_ChasePlayer( ent );
		targ = &game.edicts[ent->r.client->resp.chase.target];
		if( !G_Chase_IsValidTarget( ent, targ ) ) {
			return;
		}
	}

	ent->r.client->resp.chase.timeout = svs.realtime + 1500; // update timeout

	if( targ == ent ) {
		return;
	}

	// free our psev buffer when in chasecam
	G_ClearPlayerStateEvents( ent->r.client );

	// copy target playerState to me
	bool ready = ent->r.client->ps.ready;
	bool voted = ent->r.client->ps.voted;

	ent->r.client->ps = targ->r.client->ps;

	// fix some stats we don't want copied from the target
	ent->r.client->ps.ready = ready;
	ent->r.client->ps.voted = voted;
	ent->r.client->ps.real_team = ent->s.team;

	// chasecam uses PM_CHASECAM
	ent->r.client->ps.pmove.pm_type = PM_CHASECAM;
	ent->r.client->ps.pmove.pm_flags |= PMF_NO_PREDICTION;

	ent->s.origin = targ->s.origin;
	ent->s.angles = targ->s.angles;
	GClip_LinkEntity( ent );
}

void G_EndServerFrames_UpdateChaseCam() {
	// do it by teams, so spectators can copy the chasecam information from players
	for( int team = TEAM_PLAYERS; team < GS_MAX_TEAMS; team++ ) {
		SyncTeamState * current_team = &server_gs.gameState.teams[ team ];
		for( u8 i = 0; i < current_team->num_players; i++ ) {
			edict_t * ent = game.edicts + current_team->player_indices[i];
			if( PF_GetClientState( PLAYERNUM( ent ) ) < CS_SPAWNED ) {
				G_Chase_SetChaseActive( ent, false );
				continue;
			}

			G_EndFrame_UpdateChaseCam( ent );
		}
	}

	// Do spectators last
	SyncTeamState * spec_team = &server_gs.gameState.teams[ TEAM_SPECTATOR ];
	for( u8 i = 0; i < spec_team->num_players; i++ ) {
		edict_t * ent = game.edicts + spec_team->player_indices[ i ];
		if( PF_GetClientState( PLAYERNUM( ent ) ) < CS_SPAWNED ) {
			G_Chase_SetChaseActive( ent, false );
			continue;
		}

		G_EndFrame_UpdateChaseCam( ent );
	}
}

void G_ChasePlayer( edict_t *ent ) {
	if( server_gs.gameState.match_state >= MatchState_PostMatch ) {
		edict_t * camera = G_Find( NULL, &edict_t::classname, "post_match_camera" );
		if( camera == NULL ) {
			camera = world;
		}

		ent->s.origin = camera->s.origin;
		ent->s.angles = camera->s.angles;
		return;
	}

	bool teamonly = ent->s.team >= TEAM_ALPHA;
	gclient_t * client = ent->r.client;

	int targetNum = -1;
	for( int i = 0; i < server_gs.maxclients; i++ ) {
		edict_t * e = PLAYERENT( i );
		if( !G_Chase_IsValidTarget( ent, e ) ) {
			continue;
		}

		targetNum = ENTNUM( e );
		break;
	}

	if( targetNum != -1 ) {
		client->resp.chase.active = true;
		client->resp.chase.target = targetNum;
	}
	else {
		client->resp.chase.active = false;
		if( !teamonly ) {
			ent->movetype = MOVETYPE_NOCLIP;
		}
	}
}

void G_ChaseStep( edict_t *ent, int step ) {
	edict_t *newtarget = NULL;

	assert( Abs( step ) <= 1 );

	if( !ent->r.client->resp.chase.active ) {
		return;
	}

	int start = ent->r.client->resp.chase.target;
	int i = -1;
	bool player_found = false; // needed to prevent an infinite loop if there are no players
	// find the team of the previously chased player and his index in the sorted list
	int team;
	for( team = TEAM_PLAYERS; team < GS_MAX_TEAMS; team++ ) {
		const SyncTeamState * current_team = &server_gs.gameState.teams[ team ];
		u8 j;
		for( j = 0; j < current_team->num_players; j++ ) {
			player_found = true;
			if( current_team->player_indices[ j ] == start ) {
				i = j;
				break;
			}
		}
		if( j != current_team->num_players ) {
			break;
		}
	}

	if( step == 0 ) {
		// keep chasing the current player if possible
		if( i >= 0 && G_Chase_IsValidTarget( ent, game.edicts + start ) ) {
			newtarget = game.edicts + start;
		} else {
			step = 1;
		}
	}

	if( !newtarget && player_found ) {
		// reset the team if the previously chased player was not found
		if( team == GS_MAX_TEAMS ) {
			team = TEAM_PLAYERS;
		}
		for( int j = 0; j < server_gs.maxclients; j++ ) {
			// at this point step is -1 or 1
			i += step;

			// change to the previous team if we skipped before the start of this one
			// the loop assures empty teams before this team are skipped as well
			while( i < 0 ) {
				team--;
				if( team < TEAM_PLAYERS ) {
					team = GS_MAX_TEAMS - 1;
				}
				i = server_gs.gameState.teams[ team ].num_players - 1;
			}

			// similarly, change to the next team if we skipped past the end of this one
			while( i >= server_gs.gameState.teams[ team ].num_players ) {
				team++;
				if( team == GS_MAX_TEAMS ) {
					team = TEAM_PLAYERS;
				}
				i = 0;
			}
			int actual = server_gs.gameState.teams[ team ].player_indices[i];
			if( actual == start ) {
				break; // back at the original player, no need to waste time
			}
			if( G_Chase_IsValidTarget( ent, game.edicts + actual ) ) {
				newtarget = game.edicts + actual;
				break;
			}

			// make another step if this player is not valid
		}
	}

	if( newtarget ) {
		ent->r.client->resp.chase.target = ENTNUM( newtarget );
	}
}

void Cmd_ChaseCam_f( edict_t *ent ) {
	if( ent->s.team != TEAM_SPECTATOR ) {
		G_Teams_JoinTeam( ent, TEAM_SPECTATOR );
		if( !CheckFlood( ent, false ) ) { // prevent 'joined spectators' spam
			G_PrintMsg( NULL, "%s joined the %s team.\n", ent->r.client->netname, GS_TeamName( ent->s.team ) );
		}
	}
}

void Cmd_SwitchChaseCamMode_f( edict_t *ent ) {
	if( ent->s.team == TEAM_SPECTATOR ) {
		if( ent->r.client->resp.chase.active ) {
			G_Chase_SetChaseActive( ent, false );
			ent->movetype = MOVETYPE_NOCLIP;
		}
		else {
			G_Chase_SetChaseActive( ent, true );
			G_ChaseStep( ent, 0 );
		}
	}
}
