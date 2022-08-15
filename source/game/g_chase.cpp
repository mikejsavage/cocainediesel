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

static bool G_Chase_IsValidTarget( const edict_t * ent, const edict_t * target ) {
	if( !ent || !target ) {
		return false;
	}

	if( !target->r.inuse || !target->r.client || PF_GetClientState( PLAYERNUM( target ) ) < CS_SPAWNED ) {
		return false;
	}

	if( target->s.team == Team_None || target == ent ) {
		return false;
	}

	if( G_ISGHOSTING( target ) ) {
		return false;
	}

	if( G_ISGHOSTING( target ) && !target->deadflag && target->s.team != Team_None ) {
		return false; // ghosts that are neither dead, nor speccing (probably originating from gt-specific rules)
	}

	if( target->s.team != ent->s.team ) {
		SyncTeamState * current_team = &server_gs.gameState.teams[ ent->s.team ];

		for( u8 i = 0; i < current_team->num_players; i++ ) {
			edict_t * e = game.edicts + current_team->player_indices[i];
			if( !G_ISGHOSTING( e ) ) {
				return false;
			}
		}
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
	for( int team = Team_One; team < Team_One + level.gametype.numTeams; team++ ) {
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
	SyncTeamState * spec_team = &server_gs.gameState.teams[ Team_None ];
	for( u8 i = 0; i < spec_team->num_players; i++ ) {
		edict_t * ent = game.edicts + spec_team->player_indices[ i ];
		if( PF_GetClientState( PLAYERNUM( ent ) ) < CS_SPAWNED ) {
			G_Chase_SetChaseActive( ent, false );
			continue;
		}

		G_EndFrame_UpdateChaseCam( ent );
	}
}

void G_ChasePlayer( edict_t * ent ) {
	// post_match_camera
	if( server_gs.gameState.match_state >= MatchState_PostMatch ) {
		const edict_t * camera = G_Find( NULL, &edict_t::classname, "post_match_camera" );
		if( camera == NULL ) {
			camera = G_PickRandomEnt( &edict_t::classname, "deadcam" );
		}
		if( camera == NULL ) {
			camera = world;
		}

		ent->movetype = MOVETYPE_NONE;
		ent->s.origin = camera->s.origin;
		ent->s.angles = camera->s.angles;
		return;
	}

	// try to find a teammate
	gclient_t * client = ent->r.client;

	for( int i = 0; i < server_gs.maxclients; i++ ) {
		const edict_t * e = PLAYERENT( i );
		if( !G_Chase_IsValidTarget( ent, e ) )
			continue;

		client->resp.chase.active = true;
		client->resp.chase.target = ENTNUM( e );
		return;
	}

	// pick a deadcam
	const edict_t * deadcam = GT_CallSelectDeadcam();

	if( deadcam == NULL ) {
		deadcam = G_PickRandomEnt( &edict_t::classname, "deadcam" );
	}

	if( deadcam != NULL ) {
		ent->movetype = MOVETYPE_NONE;
		ent->s.origin = deadcam->s.origin;
		ent->s.angles = deadcam->s.angles;
		return;
	}

	client->resp.chase.active = false;
}

void G_ChaseStep( edict_t * ent, int step ) {
	assert( Abs( step ) <= 1 );

	if( !ent->r.client->resp.chase.active ) {
		return;
	}

	int old_target = ent->r.client->resp.chase.target;
	Optional< Team > old_target_team = NONE;
	Optional< u8 > old_target_idx = NONE;

	for( int i = 0; i < level.gametype.numTeams; i++ ) {
		const SyncTeamState * team = &server_gs.gameState.teams[ Team_One + i ];
		for( u8 j = 0; j < team->num_players; j++ ) {
			if( team->player_indices[ j ] == old_target ) {
				old_target_team = Team( Team_One + i );
				old_target_idx = j;
				break;
			}
		}
	}

	edict_t * new_target = NULL;
	if( step == 0 ) {
		// keep chasing the current player if possible
		if( old_target_team.exists && G_Chase_IsValidTarget( ent, &game.edicts[ old_target ] ) ) {
			new_target = &game.edicts[ old_target ];
		}
		else {
			step = 1;
		}
	}

	if( new_target == 0 && old_target_team.exists ) {
		for( int j = 0; j < server_gs.maxclients; j++ ) {
			int candidate = PositiveMod( old_target + j * step, server_gs.maxclients );
			if( G_Chase_IsValidTarget( ent, PLAYERENT( candidate ) ) ) {
				new_target = PLAYERENT( candidate );
				break;
			}
		}
	}

	if( new_target != NULL ) {
		ent->r.client->resp.chase.target = ENTNUM( new_target );
	}
}

void Cmd_Spectate( edict_t * ent ) {
	if( ent->s.team != Team_None ) {
		G_Teams_JoinTeam( ent, Team_None );
		if( !CheckFlood( ent, false ) ) { // prevent 'joined spectators' spam
			G_PrintMsg( NULL, "%s joined the %s team.\n", ent->r.client->netname, GS_TeamName( ent->s.team ) );
		}
	}
}

void Cmd_ToggleFreeFly( edict_t * ent ) {
	if( ent->s.team == Team_None ) {
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
