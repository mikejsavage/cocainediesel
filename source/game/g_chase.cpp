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
#include "g_local.h"

/*
* G_Chase_SetChaseActive
*/
static void G_Chase_SetChaseActive( edict_t *ent, bool active ) {
	ent->r.client->resp.chase.active = active;
}

/*
* G_Chase_IsValidTarget
*/
static bool G_Chase_IsValidTarget( edict_t *ent, edict_t *target, bool teamonly ) {
	if( !ent || !target ) {
		return false;
	}

	if( !target->r.inuse || !target->r.client || trap_GetClientState( PLAYERNUM( target ) ) < CS_SPAWNED ) {
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



/*
* G_EndFrame_UpdateChaseCam
*/
static void G_EndFrame_UpdateChaseCam( edict_t *ent ) {
	edict_t *targ;

	// not in chasecam
	if( !ent->r.client->resp.chase.active ) {
		return;
	}

	// is our chase target gone?
	targ = &game.edicts[ent->r.client->resp.chase.target];

	if( !G_Chase_IsValidTarget( ent, targ, ent->r.client->resp.chase.teamonly ) ) {
		if( svs.realtime < ent->r.client->resp.chase.timeout ) { // wait for timeout
			return;
		}

		ent->r.client->resp.chase.timeout = svs.realtime + 1500; // update timeout

		G_ChasePlayer( ent, NULL, ent->r.client->resp.chase.teamonly, ent->r.client->resp.chase.followmode );
		targ = &game.edicts[ent->r.client->resp.chase.target];
		if( !G_Chase_IsValidTarget( ent, targ, ent->r.client->resp.chase.teamonly ) ) {
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
	int64_t layouts = ent->r.client->ps.stats[STAT_LAYOUTS];
	ent->r.client->ps = targ->r.client->ps;

	// fix some stats we don't want copied from the target
	ent->r.client->ps.stats[STAT_REALTEAM] = ent->s.team;
	ent->r.client->ps.stats[STAT_LAYOUTS] = layouts;

	// chasecam uses PM_CHASECAM
	ent->r.client->ps.pmove.pm_type = PM_CHASECAM;
	ent->r.client->ps.pmove.pm_flags |= PMF_NO_PREDICTION;

	VectorCopy( targ->s.origin, ent->s.origin );
	VectorCopy( targ->s.angles, ent->s.angles );
	GClip_LinkEntity( ent );
}

/*
* G_EndServerFrames_UpdateChaseCam
*/
void G_EndServerFrames_UpdateChaseCam( void ) {
	int i, team;
	edict_t *ent;

	// do it by teams, so spectators can copy the chasecam information from players
	for( team = TEAM_PLAYERS; team < GS_MAX_TEAMS; team++ ) {
		for( i = 0; i < teamlist[team].numplayers; i++ ) {
			ent = game.edicts + teamlist[team].playerIndices[i];
			if( trap_GetClientState( PLAYERNUM( ent ) ) < CS_SPAWNED ) {
				G_Chase_SetChaseActive( ent, false );
				continue;
			}

			G_EndFrame_UpdateChaseCam( ent );
		}
	}

	// Do spectators last
	for( i = 0; i < teamlist[TEAM_SPECTATOR].numplayers; i++ ) {
		ent = game.edicts + teamlist[TEAM_SPECTATOR].playerIndices[i];
		if( trap_GetClientState( PLAYERNUM( ent ) ) < CS_SPAWNED ) {
			G_Chase_SetChaseActive( ent, false );
			continue;
		}

		G_EndFrame_UpdateChaseCam( ent );
	}
}

/*
* G_ChasePlayer
*/
void G_ChasePlayer( edict_t *ent, const char *name, bool teamonly, int followmode ) {
	int i;
	edict_t *e;
	gclient_t *client;
	int targetNum = -1;
	int oldTarget;

	client = ent->r.client;

	oldTarget = client->resp.chase.target;

	if( teamonly && followmode ) {
		G_PrintMsg( ent, "Chasecam follow mode unavailable\n" );
		followmode = false;
	}

	if( ent->r.client->resp.chase.followmode && !followmode ) {
		G_PrintMsg( ent, "Disabling chasecam follow mode\n" );
	}

	// always disable chasing as a start
	memset( &client->resp.chase, 0, sizeof( chasecam_t ) );

	// locate the requested target
	if( name && name[0] ) {
		// find it by player names
		for( e = game.edicts + 1; PLAYERNUM( e ) < server_gs.maxclients; e++ ) {
			if( !G_Chase_IsValidTarget( ent, e, teamonly ) ) {
				continue;
			}

			if( !Q_stricmp( name, e->r.client->netname ) ) {
				targetNum = PLAYERNUM( e );
				break;
			}
		}

		// didn't find it by name, try by numbers
		if( targetNum == -1 ) {
			i = atoi( name );
			if( i >= 0 && i < server_gs.maxclients ) {
				e = game.edicts + 1 + i;
				if( G_Chase_IsValidTarget( ent, e, teamonly ) ) {
					targetNum = PLAYERNUM( e );
				}
			}
		}

		if( targetNum == -1 ) {
			G_PrintMsg( ent, "Requested chasecam target is not available\n" );
		}
	}

	// try to reuse old target if we didn't find a valid one
	if( targetNum == -1 && oldTarget > 0 && oldTarget < server_gs.maxclients ) {
		e = game.edicts + 1 + oldTarget;
		if( G_Chase_IsValidTarget( ent, e, teamonly ) ) {
			targetNum = PLAYERNUM( e );
		}
	}

	// if we still don't have a target, just pick the first valid one
	if( targetNum == -1 ) {
		for( e = game.edicts + 1; PLAYERNUM( e ) < server_gs.maxclients; e++ ) {
			if( !G_Chase_IsValidTarget( ent, e, teamonly ) ) {
				continue;
			}

			targetNum = PLAYERNUM( e );
			break;
		}
	}

	// make the client a ghost
	G_GhostClient( ent );
	if( targetNum != -1 ) {
		// we found a target, set up the chasecam
		client->resp.chase.target = targetNum + 1;
		client->resp.chase.teamonly = teamonly;
		client->resp.chase.followmode = followmode;
		G_Chase_SetChaseActive( ent, true );
	} else {
		// stay as observer
		if( !teamonly ) {
			ent->movetype = MOVETYPE_NOCLIP;
		}
		client->level.showscores = false;
		G_Chase_SetChaseActive( ent, false );
	}
}

/*
* ChaseStep
*/
void G_ChaseStep( edict_t *ent, int step ) {
	int i, j, team;
	bool player_found;
	int actual;
	int start;
	edict_t *newtarget = NULL;

	assert( step == -1 || step == 0 || step == 1 );

	if( !ent->r.client->resp.chase.active ) {
		return;
	}

	start = ent->r.client->resp.chase.target;
	i = -1;
	player_found = false; // needed to prevent an infinite loop if there are no players
	// find the team of the previously chased player and his index in the sorted teamlist
	for( team = TEAM_PLAYERS; team < GS_MAX_TEAMS; team++ ) {
		for( j = 0; j < teamlist[team].numplayers; j++ ) {
			player_found = true;
			if( teamlist[team].playerIndices[j] == start ) {
				i = j;
				break;
			}
		}
		if( j != teamlist[team].numplayers ) {
			break;
		}
	}

	if( step == 0 ) {
		// keep chasing the current player if possible
		if( i >= 0 && G_Chase_IsValidTarget( ent, game.edicts + start, ent->r.client->resp.chase.teamonly ) ) {
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
		for( j = 0; j < server_gs.maxclients; j++ ) {
			// at this point step is -1 or 1
			i += step;

			// change to the previous team if we skipped before the start of this one
			// the loop assures empty teams before this team are skipped as well
			while( i < 0 ) {
				team--;
				if( team < TEAM_PLAYERS ) {
					team = GS_MAX_TEAMS - 1;
				}
				i = teamlist[team].numplayers - 1;
			}

			// similarly, change to the next team if we skipped past the end of this one
			while( i >= teamlist[team].numplayers ) {
				team++;
				if( team == GS_MAX_TEAMS ) {
					team = TEAM_PLAYERS;
				}
				i = 0;
			}
			actual = teamlist[team].playerIndices[i];
			if( actual == start ) {
				break; // back at the original player, no need to waste time
			}
			if( G_Chase_IsValidTarget( ent, game.edicts + actual, ent->r.client->resp.chase.teamonly ) ) {
				newtarget = game.edicts + actual;
				break;
			}

			// make another step if this player is not valid
		}
	}

	if( newtarget ) {
		G_ChasePlayer( ent, va( "%i", PLAYERNUM( newtarget ) ), ent->r.client->resp.chase.teamonly, ent->r.client->resp.chase.followmode );
	}
}

/*
* Cmd_ChaseCam_f
*/
void Cmd_ChaseCam_f( edict_t *ent ) {
	if( ent->s.team != TEAM_SPECTATOR ) {
		G_Teams_JoinTeam( ent, TEAM_SPECTATOR );
		if( !CheckFlood( ent, false ) ) { // prevent 'joined spectators' spam
			G_PrintMsg( NULL, "%s joined the %s team.\n", ent->r.client->netname, GS_TeamName( ent->s.team ) );
		}
	}

	// & 1 = scorelead
	// & 4 = objectives
	// & 8 = fragger

	const char * arg1 = trap_Cmd_Argv( 1 );

	if( trap_Cmd_Argc() < 2 ) {
		G_ChasePlayer( ent, NULL, false, 0 );
	} else if( !Q_stricmp( arg1, "auto" ) ) {
		G_PrintMsg( ent, "Chasecam mode is 'auto'. It will follow the score leader when no powerup nor flag is carried.\n" );
		G_ChasePlayer( ent, NULL, false, 7 );
	} else if( !Q_stricmp( arg1, "carriers" ) ) {
		G_PrintMsg( ent, "Chasecam mode is 'carriers'. It will switch to flag or powerup carriers when any of these items is picked up.\n" );
		G_ChasePlayer( ent, NULL, false, 6 );
	} else if( !Q_stricmp( arg1, "objectives" ) ) {
		G_PrintMsg( ent, "Chasecam mode is 'objectives'. It will switch to objectives carriers when any of these items is picked up.\n" );
		G_ChasePlayer( ent, NULL, false, 4 );
	} else if( !Q_stricmp( arg1, "score" ) ) {
		G_PrintMsg( ent, "Chasecam mode is 'score'. It will always follow the player with the best score.\n" );
		G_ChasePlayer( ent, NULL, false, 1 );
	} else if( !Q_stricmp( arg1, "fragger" ) ) {
		G_PrintMsg( ent, "Chasecam mode is 'fragger'. The last fragging player will be followed.\n" );
		G_ChasePlayer( ent, NULL, false, 8 );
	} else if( !Q_stricmp( arg1, "help" ) ) {
		G_PrintMsg( ent, "Chasecam modes:\n" );
		G_PrintMsg( ent, "- 'auto': Chase the score leader unless there's an objective carrier or a powerup carrier.\n" );
		G_PrintMsg( ent, "- 'carriers': User has pov control unless there's an objective carrier or a powerup carrier.\n" );
		G_PrintMsg( ent, "- 'objectives': User has pov control unless there's an objective carrier.\n" );
		G_PrintMsg( ent, "- 'score': Always follow the score leader. User has no pov control.\n" );
		G_PrintMsg( ent, "- 'none': Disable chasecam.\n" );
		return;
	} else {
		G_ChasePlayer( ent, arg1, false, 0 );
	}

	G_Teams_LeaveChallengersQueue( ent );
}

/*
* G_SpectatorMode
*/
void G_SpectatorMode( edict_t *ent ) {
	// join spectator team
	if( ent->s.team != TEAM_SPECTATOR ) {
		G_Teams_JoinTeam( ent, TEAM_SPECTATOR );
		G_PrintMsg( NULL, "%s%s joined the %s%s team.\n", ent->r.client->netname,
					S_COLOR_WHITE, GS_TeamName( ent->s.team ), S_COLOR_WHITE );
	}

	// was in chasecam
	if( ent->r.client->resp.chase.active ) {
		ent->r.client->level.showscores = false;
		G_Chase_SetChaseActive( ent, false );

		// reset movement speeds
		ent->r.client->ps.pmove.stats[PM_STAT_MAXSPEED] = DEFAULT_PLAYERSPEED;
		ent->r.client->ps.pmove.stats[PM_STAT_JUMPSPEED] = DEFAULT_JUMPSPEED;
		ent->r.client->ps.pmove.stats[PM_STAT_DASHSPEED] = DEFAULT_DASHSPEED;
	}

	ent->movetype = MOVETYPE_NOCLIP;
}

/*
* Cmd_Spec_f
*/
void Cmd_Spec_f( edict_t *ent ) {
	if( ent->s.team == TEAM_SPECTATOR && !ent->r.client->queueTimeStamp ) {
		G_PrintMsg( ent, "You are already a spectator.\n" );
		return;
	}

	G_SpectatorMode( ent );
	G_Teams_LeaveChallengersQueue( ent );
}

/*
* Cmd_SwitchChaseCamMode_f
*/
void Cmd_SwitchChaseCamMode_f( edict_t *ent ) {
	if( ent->s.team == TEAM_SPECTATOR ) {
		if( ent->r.client->resp.chase.active ) {
			G_SpectatorMode( ent );
		} else {
			G_Chase_SetChaseActive( ent, true );
			G_ChaseStep( ent, 0 );
		}
	}
}
