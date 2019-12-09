/*
Copyright (C) 2002-2003 Victor Luchits

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
#include "client/renderer/skybox.h"

ChasecamState chaseCam;

int CG_LostMultiviewPOV( void );

/*
* CG_ChaseStep
*
* Returns whether the POV was actually requested to be changed.
*/
bool CG_ChaseStep( int step ) {
	int index, checkPlayer, i;

	if( cg.frame.multipov ) {
		// find the playerState containing our current POV, then cycle playerStates
		index = -1;
		for( i = 0; i < cg.frame.numplayers; i++ ) {
			if( cg.frame.playerStates[i].playerNum < (unsigned)client_gs.maxclients && cg.frame.playerStates[i].playerNum == cg.multiviewPlayerNum ) {
				index = i;
				break;
			}
		}

		// the POV was lost, find the closer one (may go up or down, but who cares)
		if( index == -1 ) {
			checkPlayer = CG_LostMultiviewPOV();
		} else {
			checkPlayer = index;
			for( i = 0; i < cg.frame.numplayers; i++ ) {
				checkPlayer += step;
				if( checkPlayer < 0 ) {
					checkPlayer = cg.frame.numplayers - 1;
				} else if( checkPlayer >= cg.frame.numplayers ) {
					checkPlayer = 0;
				}

				if( ( checkPlayer != index ) && cg.frame.playerStates[checkPlayer].stats[STAT_REALTEAM] == TEAM_SPECTATOR ) {
					continue;
				}
				break;
			}
		}

		if( index < 0 ) {
			return false;
		}

		cg.multiviewPlayerNum = cg.frame.playerStates[checkPlayer].playerNum;
		return true;
	}
	
	if( !cgs.demoPlaying ) {
		trap_Cmd_ExecuteText( EXEC_NOW, step > 0 ? "chasenext" : "chaseprev" );
		return true;
	}

	return false;
}

/*
* CG_AddLocalSounds
*/
static void CG_AddLocalSounds( void ) {
	static unsigned lastSecond = 0;

	// add local announces
	if( GS_Countdown( &client_gs ) ) {
		if( GS_MatchDuration( &client_gs ) ) {
			int64_t duration, curtime;
			unsigned remainingSeconds;
			float seconds;

			curtime = GS_MatchPaused( &client_gs ) ? cg.frame.serverTime : cl.serverTime;
			duration = GS_MatchDuration( &client_gs );

			if( duration + GS_MatchStartTime( &client_gs ) < curtime ) {
				duration = curtime - GS_MatchStartTime( &client_gs ); // avoid negative results

			}
			seconds = (float)( GS_MatchStartTime( &client_gs ) + duration - curtime ) * 0.001f;
			remainingSeconds = (unsigned int)seconds;

			if( remainingSeconds != lastSecond ) {
				if( 1 + remainingSeconds < 4 ) {
					const SoundEffect * sfx = FindSoundEffect( va( S_ANNOUNCER_COUNTDOWN_COUNT_1_to_3_SET_1_to_2, 1 + remainingSeconds, 1 ) );
					CG_AddAnnouncerEvent( sfx, false );
					CG_CenterPrint( va( "%i", remainingSeconds + 1 ) );
				}

				lastSecond = remainingSeconds;
			}
		}
	} else {
		lastSecond = 0;
	}

	// add sounds from announcer
	CG_ReleaseAnnouncerEvents();
}

/*
* CG_FlashGameWindow
*
* Flashes game window in case of important events (match state changes, etc) for user to notice
*/
static void CG_FlashGameWindow( void ) {
	static int oldState = -1;
	bool flash = false;
	static int oldAlphaScore, oldBetaScore;
	static bool scoresSet = false;

	// notify player of important match states
	int newState = GS_MatchState( &client_gs );
	if( oldState != newState ) {
		switch( newState ) {
			case MATCH_STATE_COUNTDOWN:
			case MATCH_STATE_PLAYTIME:
			case MATCH_STATE_POSTMATCH:
				flash = true;
				break;
			default:
				break;
		}

		oldState = newState;
	}

	// notify player of teams scoring in team-based gametypes
	if( !scoresSet ||
		( oldAlphaScore != cg.predictedPlayerState.stats[STAT_TEAM_ALPHA_SCORE] || oldBetaScore != cg.predictedPlayerState.stats[STAT_TEAM_BETA_SCORE] ) ) {
		oldAlphaScore = cg.predictedPlayerState.stats[STAT_TEAM_ALPHA_SCORE];
		oldBetaScore = cg.predictedPlayerState.stats[STAT_TEAM_BETA_SCORE];

		flash = scoresSet && GS_TeamBasedGametype( &client_gs ) && !GS_IndividualGameType( &client_gs );
		scoresSet = true;
	}

	if( flash ) {
		trap_VID_FlashWindow();
	}
}

/*
* CG_AddKickAngles
*/
void CG_AddKickAngles( vec3_t viewangles ) {
	float time;
	float uptime;
	float delta;
	int i;

	for( i = 0; i < MAX_ANGLES_KICKS; i++ ) {
		if( cl.serverTime > cg.kickangles[i].timestamp + cg.kickangles[i].kicktime ) {
			continue;
		}

		time = (float)( ( cg.kickangles[i].timestamp + cg.kickangles[i].kicktime ) - cl.serverTime );
		uptime = ( (float)cg.kickangles[i].kicktime ) * 0.5f;
		delta = 1.0f - ( fabs( time - uptime ) / uptime );

		//CG_Printf("Kick Delta:%f\n", delta );
		if( delta > 1.0f ) {
			delta = 1.0f;
		}
		if( delta <= 0.0f ) {
			continue;
		}

		viewangles[PITCH] += cg.kickangles[i].v_pitch * delta;
		viewangles[ROLL] += cg.kickangles[i].v_roll * delta;
	}
}

/*
* CG_CalcViewFov
*/
static float CG_CalcViewFov( void ) {
	float frac;
	float fov, zoomfov;

	fov = cg_fov->value;
	zoomfov = cg_zoomfov->value;

	if( !cg.predictedPlayerState.pmove.stats[PM_STAT_ZOOMTIME] ) {
		return fov;
	}

	frac = (float)cg.predictedPlayerState.pmove.stats[PM_STAT_ZOOMTIME] / (float)ZOOMTIME;
	return fov - ( fov - zoomfov ) * frac;
}

/*
* CG_CalcViewBob
*/
static void CG_CalcViewBob( void ) {
	float bobMove, bobTime, bobScale;

	if( !cg.view.drawWeapon ) {
		return;
	}

	// calculate speed and cycle to be used for all cyclic walking effects
	cg.xyspeed = sqrt( cg.predictedPlayerState.pmove.velocity[0] * cg.predictedPlayerState.pmove.velocity[0] + cg.predictedPlayerState.pmove.velocity[1] * cg.predictedPlayerState.pmove.velocity[1] );

	bobScale = 0;
	if( cg.xyspeed < 5 ) {
		cg.oldBobTime = 0;  // start at beginning of cycle again
	} else if( cg_gunbob->integer ) {
		if( !ISVIEWERENTITY( cg.view.POVent ) ) {
			bobScale = 0.0f;
		} else if( CG_PointContents( cg.view.origin ) & MASK_WATER ) {
			bobScale =  0.75f;
		} else {
			centity_t *cent;
			vec3_t mins, maxs;
			trace_t trace;

			cent = &cg_entities[cg.view.POVent];
			CG_BBoxForEntityState( &cent->current, mins, maxs );
			maxs[2] = mins[2];
			mins[2] -= ( 1.6f * STEPSIZE );

			CG_Trace( &trace, cg.predictedPlayerState.pmove.origin, mins, maxs, cg.predictedPlayerState.pmove.origin, cg.view.POVent, MASK_PLAYERSOLID );
			if( trace.startsolid || trace.allsolid ) {
				if( cg.predictedPlayerState.pmove.stats[PM_STAT_CROUCHTIME] ) {
					bobScale = 1.5f;
				} else {
					bobScale = 2.5f;
				}
			}
		}
	}

	bobMove = cls.frametime * bobScale * 0.001f;
	bobTime = ( cg.oldBobTime += bobMove );

	cg.bobCycle = (int)bobTime;
	cg.bobFracSin = fabs( sin( bobTime * M_PI ) );
}

/*
* CG_ResetKickAngles
*/
void CG_ResetKickAngles( void ) {
	memset( cg.kickangles, 0, sizeof( cg.kickangles ) );
}

/*
* CG_StartKickAnglesEffect
*/
void CG_StartKickAnglesEffect( vec3_t source, float knockback, float radius, int time ) {
	float kick;
	float side;
	float dist;
	float delta;
	float ftime;
	vec3_t forward, right, v;
	int i, kicknum = -1;
	vec3_t playerorigin;

	if( knockback <= 0 || time <= 0 || radius <= 0.0f ) {
		return;
	}

	// if spectator but not in chasecam, don't get any kick
	if( cg.frame.playerState.pmove.pm_type == PM_SPECTATOR ) {
		return;
	}

	// not if dead
	if( cg_entities[cg.view.POVent].current.type == ET_CORPSE || cg_entities[cg.view.POVent].current.type == ET_GIB ) {
		return;
	}

	// predictedPlayerState is predicted only when prediction is enabled, otherwise it is interpolated
	VectorCopy( cg.predictedPlayerState.pmove.origin, playerorigin );

	VectorSubtract( source, playerorigin, v );
	dist = VectorNormalize( v );
	if( dist > radius ) {
		return;
	}

	delta = 1.0f - ( dist / radius );
	if( delta > 1.0f ) {
		delta = 1.0f;
	}
	if( delta <= 0.0f ) {
		return;
	}

	kick = fabs( knockback ) * delta;
	if( kick ) { // kick of 0 means no view adjust at all
		//find first free kick spot, or the one closer to be finished
		for( i = 0; i < MAX_ANGLES_KICKS; i++ ) {
			if( cl.serverTime > cg.kickangles[i].timestamp + cg.kickangles[i].kicktime ) {
				kicknum = i;
				break;
			}
		}

		// all in use. Choose the closer to be finished
		if( kicknum == -1 ) {
			int remaintime;
			int best = ( cg.kickangles[0].timestamp + cg.kickangles[0].kicktime ) - cl.serverTime;
			kicknum = 0;
			for( i = 1; i < MAX_ANGLES_KICKS; i++ ) {
				remaintime = ( cg.kickangles[i].timestamp + cg.kickangles[i].kicktime ) - cl.serverTime;
				if( remaintime < best ) {
					best = remaintime;
					kicknum = i;
				}
			}
		}

		AngleVectors( cg.frame.playerState.viewangles, forward, right, NULL );

		if( kick < 1.0f ) {
			kick = 1.0f;
		}

		side = DotProduct( v, right );
		cg.kickangles[kicknum].v_roll = Clamp( -20.0f, kick * side * 0.3f, 20.0f );

		side = -DotProduct( v, forward );
		cg.kickangles[kicknum].v_pitch = Clamp( -20.0f, kick * side * 0.3f, 20.0f );

		cg.kickangles[kicknum].timestamp = cl.serverTime;
		ftime = (float)time * delta;
		if( ftime < 100 ) {
			ftime = 100;
		}
		cg.kickangles[kicknum].kicktime = ftime;
	}
}

/*
* CG_StartFallKickEffect
*/
void CG_StartFallKickEffect( int bounceTime ) {
	if( cg.fallEffectTime > cl.serverTime ) {
		cg.fallEffectRebounceTime = 0;
	}

	bounceTime = Min2( 400, bounceTime + 200 );

	cg.fallEffectTime = cl.serverTime + bounceTime;
	if( cg.fallEffectRebounceTime ) {
		cg.fallEffectRebounceTime = cl.serverTime - ( ( cl.serverTime - cg.fallEffectRebounceTime ) * 0.5 );
	} else {
		cg.fallEffectRebounceTime = cl.serverTime;
	}
}

/*
* CG_ResetColorBlend
*/
void CG_ResetColorBlend( void ) {
	memset( cg.colorblends, 0, sizeof( cg.colorblends ) );
}

//============================================================================

/*
* CG_AddEntityToScene
*/
void CG_AddEntityToScene( entity_t * ent ) {
	// trap_R_AddEntityToScene( ent );
}

//============================================================================

/*
* CG_RenderFlags
*/
static int CG_RenderFlags( void ) {
	int rdflags, contents;

	rdflags = 0;

	// set the RDF_UNDERWATER and RDF_CROSSINGWATER bitflags
	contents = CG_PointContents( cg.view.origin );
	if( contents & MASK_WATER ) {
		rdflags |= RDF_UNDERWATER;

		// undewater, check above
		contents = CG_PointContents( tv( cg.view.origin[0], cg.view.origin[1], cg.view.origin[2] + 9 ) );
		if( !( contents & MASK_WATER ) ) {
			rdflags |= RDF_CROSSINGWATER;
		}
	} else {
		// look down a bit
		contents = CG_PointContents( tv( cg.view.origin[0], cg.view.origin[1], cg.view.origin[2] - 9 ) );
		if( contents & MASK_WATER ) {
			rdflags |= RDF_CROSSINGWATER;
		}
	}

	if( GS_MatchState( &client_gs ) >= MATCH_STATE_POSTMATCH ) {
		rdflags |= RDF_BLURRED;
	}

	return rdflags;
}

/*
* CG_InterpolatePlayerState
*/
static void CG_InterpolatePlayerState( player_state_t *playerState ) {
	int i;
	player_state_t *ps, *ops;
	bool teleported;

	ps = &cg.frame.playerState;
	ops = &cg.oldFrame.playerState;

	*playerState = *ps;

	teleported = ( ps->pmove.pm_flags & PMF_TIME_TELEPORT ) ? true : false;

	if( abs( (int)( ops->pmove.origin[0] - ps->pmove.origin[0] ) ) > 256
		|| abs( (int)( ops->pmove.origin[1] - ps->pmove.origin[1] ) ) > 256
		|| abs( (int)( ops->pmove.origin[2] - ps->pmove.origin[2] ) ) > 256 ) {
		teleported = true;
	}

	// if the player entity was teleported this frame use the final position
	if( !teleported ) {
		for( i = 0; i < 3; i++ ) {
			playerState->pmove.origin[i] = ops->pmove.origin[i] + cg.lerpfrac * ( ps->pmove.origin[i] - ops->pmove.origin[i] );
			playerState->pmove.velocity[i] = ops->pmove.velocity[i] + cg.lerpfrac * ( ps->pmove.velocity[i] - ops->pmove.velocity[i] );
			playerState->viewangles[i] = LerpAngle( ops->viewangles[i], ps->viewangles[i], cg.lerpfrac );
		}
	}

	// interpolate fov and viewheight
	if( !teleported ) {
		playerState->viewheight = ops->viewheight + cg.lerpfrac * ( ps->viewheight - ops->viewheight );
		playerState->pmove.stats[PM_STAT_ZOOMTIME] = ops->pmove.stats[PM_STAT_ZOOMTIME] + cg.lerpfrac * ( ps->pmove.stats[PM_STAT_ZOOMTIME] - ops->pmove.stats[PM_STAT_ZOOMTIME] );
	}
}

/*
* CG_ThirdPersonOffsetView
*/
static void CG_ThirdPersonOffsetView( cg_viewdef_t *view ) {
	float dist, f, r;
	vec3_t dest, stop;
	vec3_t chase_dest;
	trace_t trace;
	vec3_t mins = { -4, -4, -4 };
	vec3_t maxs = { 4, 4, 4 };

	if( !cg_thirdPersonAngle || !cg_thirdPersonRange ) {
		cg_thirdPersonAngle = trap_Cvar_Get( "cg_thirdPersonAngle", "0", CVAR_ARCHIVE );
		cg_thirdPersonRange = trap_Cvar_Get( "cg_thirdPersonRange", "70", CVAR_ARCHIVE );
	}

	// calc exact destination
	VectorCopy( view->origin, chase_dest );
	r = DEG2RAD( cg_thirdPersonAngle->value );
	f = -cos( r );
	r = -sin( r );
	VectorMA( chase_dest, cg_thirdPersonRange->value * f, &view->axis[AXIS_FORWARD], chase_dest );
	VectorMA( chase_dest, cg_thirdPersonRange->value * r, &view->axis[AXIS_RIGHT], chase_dest );
	chase_dest[2] += 8;

	// find the spot the player is looking at
	VectorMA( view->origin, 512, &view->axis[AXIS_FORWARD], dest );
	CG_Trace( &trace, view->origin, mins, maxs, dest, view->POVent, MASK_SOLID );

	// calculate pitch to look at the same spot from camera
	VectorSubtract( trace.endpos, view->origin, stop );
	dist = sqrt( stop[0] * stop[0] + stop[1] * stop[1] );
	if( dist < 1 ) {
		dist = 1;
	}
	view->angles[PITCH] = RAD2DEG( -atan2( stop[2], dist ) );
	view->angles[YAW] -= cg_thirdPersonAngle->value;
	Matrix3_FromAngles( view->angles, view->axis );

	// move towards destination
	CG_Trace( &trace, view->origin, mins, maxs, chase_dest, view->POVent, MASK_SOLID );

	if( trace.fraction != 1.0 ) {
		VectorCopy( trace.endpos, stop );
		stop[2] += ( 1.0 - trace.fraction ) * 32;
		CG_Trace( &trace, view->origin, mins, maxs, stop, view->POVent, MASK_SOLID );
		VectorCopy( trace.endpos, chase_dest );
	}

	VectorCopy( chase_dest, view->origin );
}

/*
* CG_ViewSmoothPredictedSteps
*/
void CG_ViewSmoothPredictedSteps( vec3_t vieworg ) {
	int timeDelta;

	// smooth out stair climbing
	timeDelta = cls.realtime - cg.predictedStepTime;
	if( timeDelta < PREDICTED_STEP_TIME ) {
		vieworg[2] -= cg.predictedStep * ( PREDICTED_STEP_TIME - timeDelta ) / PREDICTED_STEP_TIME;
	}
}

/*
* CG_ViewSmoothFallKick
*/
float CG_ViewSmoothFallKick( void ) {
	// fallkick offset
	if( cg.fallEffectTime > cl.serverTime ) {
		float fallfrac = (float)( cl.serverTime - cg.fallEffectRebounceTime ) / (float)( cg.fallEffectTime - cg.fallEffectRebounceTime );
		float fallkick = -1.0f * sin( DEG2RAD( fallfrac * 180 ) ) * ( ( cg.fallEffectTime - cg.fallEffectRebounceTime ) * 0.01f );
		return fallkick;
	} else {
		cg.fallEffectTime = cg.fallEffectRebounceTime = 0;
	}
	return 0.0f;
}

/*
* CG_SwitchChaseCamMode
*
* Returns whether the mode was actually switched.
*/
bool CG_SwitchChaseCamMode( void ) {
	bool chasecam = ( cg.frame.playerState.pmove.pm_type == PM_CHASECAM )
					&& ( cg.frame.playerState.POVnum != (unsigned)( cgs.playerNum + 1 ) );
	bool realSpec = cgs.demoPlaying || ISREALSPECTATOR();

	if( ( cg.frame.multipov || chasecam ) && !CG_DemoCam_IsFree() ) {
		if( chasecam ) {
			if( realSpec ) {
				if( ++chaseCam.mode >= CAM_MODES ) {
					// if exceeds the cycle, start free fly
					trap_Cmd_ExecuteText( EXEC_NOW, "camswitch" );
					chaseCam.mode = 0;
				}
				return true;
			}
			return false;
		}

		chaseCam.mode = ( ( chaseCam.mode != CAM_THIRDPERSON ) ? CAM_THIRDPERSON : CAM_INEYES );
		return true;
	}

	if( realSpec && ( CG_DemoCam_IsFree() || cg.frame.playerState.pmove.pm_type == PM_SPECTATOR ) ) {
		trap_Cmd_ExecuteText( EXEC_NOW, "camswitch" );
		return true;
	}

	return false;
}

/*
* CG_UpdateChaseCam
*/
static void CG_UpdateChaseCam( void ) {
	bool chasecam = ( cg.frame.playerState.pmove.pm_type == PM_CHASECAM )
					&& ( cg.frame.playerState.POVnum != (unsigned)( cgs.playerNum + 1 ) );

	if( !( cg.frame.multipov || chasecam ) || CG_DemoCam_IsFree() ) {
		chaseCam.mode = CAM_INEYES;
	}

	usercmd_t cmd;
	trap_NET_GetUserCmd( trap_NET_GetCurrentUserCmdNum() - 1, &cmd );

	if( chaseCam.key_pressed ) {
		chaseCam.key_pressed = ( cmd.buttons & ( BUTTON_ATTACK | BUTTON_SPECIAL ) ) != 0 || cmd.upmove != 0 || cmd.sidemove != 0;
		return;
	}

	if( cmd.buttons & BUTTON_ATTACK ) {
		CG_SwitchChaseCamMode();
		chaseCam.key_pressed = true;
	}

	int chaseStep = 0;
	if( cmd.upmove > 0 || cmd.sidemove > 0 || ( cmd.buttons & BUTTON_SPECIAL ) ) {
		chaseStep = 1;
	} else if( cmd.upmove < 0 || cmd.sidemove < 0 ) {
		chaseStep = -1;
	}

	if( chaseStep != 0 ) {
		CG_ChaseStep( chaseStep );
		chaseCam.key_pressed = true;
	}
}

/*
* CG_SetupRefDef
*/
#define WAVE_AMPLITUDE  0.015   // [0..1]
#define WAVE_FREQUENCY  0.6     // [0..1]
static void CG_SetupRefDef( cg_viewdef_t *view, refdef_t *rd ) {
	memset( rd, 0, sizeof( *rd ) );

	// view rectangle size
	rd->x = 0;
	rd->y = 0;
	rd->width = frame_static.viewport_width;
	rd->height = frame_static.viewport_height;

	rd->scissor_x = 0;
	rd->scissor_y = 0;
	rd->scissor_width = frame_static.viewport_width;
	rd->scissor_height = frame_static.viewport_height;

	rd->fov_x = view->fov_x;
	rd->fov_y = view->fov_y;

	rd->time = cl.serverTime;
	rd->areabits = cg.frame.areabits;

	rd->minLight = 0.3f;

	VectorCopy( cg.view.origin, rd->vieworg );
	Matrix3_Copy( cg.view.axis, rd->viewaxis );
	VectorInverse( &rd->viewaxis[AXIS_RIGHT] );

	VectorCopy( view->origin, rd->vieworg );

	AnglesToAxis( view->angles, rd->viewaxis );

	rd->rdflags = CG_RenderFlags();

	// warp if underwater
	if( rd->rdflags & RDF_UNDERWATER ) {
		float phase = rd->time * 0.001 * WAVE_FREQUENCY * M_TWOPI;
		float v = WAVE_AMPLITUDE * ( sinf( phase ) - 1.0 ) + 1;
		rd->fov_x *= v;
		rd->fov_y *= v;
	}
}

/*
* CG_SetupViewDef
*/
static void CG_SetupViewDef( cg_viewdef_t *view, int type ) {
	memset( view, 0, sizeof( cg_viewdef_t ) );

	//
	// VIEW SETTINGS
	//

	view->type = type;

	if( view->type == VIEWDEF_PLAYERVIEW ) {
		view->POVent = cg.frame.playerState.POVnum;

		view->draw2D = true;

		// set up third-person
		if( cgs.demoPlaying ) {
			view->thirdperson = CG_DemoCam_GetThirdPerson();
		} else if( chaseCam.mode == CAM_THIRDPERSON ) {
			view->thirdperson = true;
		} else {
			view->thirdperson = ( cg_thirdPerson->integer != 0 );
		}

		if( cg_entities[view->POVent].serverFrame != cg.frame.serverFrame ) {
			view->thirdperson = false;
		}

		// check for drawing gun
		if( !view->thirdperson && view->POVent > 0 && view->POVent <= client_gs.maxclients ) {
			if( ( cg_entities[view->POVent].serverFrame == cg.frame.serverFrame ) &&
				( cg_entities[view->POVent].current.weapon != 0 ) ) {
				view->drawWeapon = cg_gun->integer != 0;
			}
		}

		// check for chase cams
		if( !( cg.frame.playerState.pmove.pm_flags & PMF_NO_PREDICTION ) ) {
			if( (unsigned)view->POVent == cgs.playerNum + 1 ) {
				if( !cgs.demoPlaying ) {
					view->playerPrediction = true;
				}
			}
		}
	} else if( view->type == VIEWDEF_DEMOCAM ) {
		CG_DemoCam_GetViewDef( view );
	} else {
		CG_Error( "CG_SetupView: Invalid view type %i\n", view->type );
	}

	//
	// SETUP REFDEF FOR THE VIEW SETTINGS
	//

	if( view->type == VIEWDEF_PLAYERVIEW ) {
		vec3_t viewoffset;

		if( view->playerPrediction ) {
			CG_PredictMovement();

			// fixme: crouching is predicted now, but it looks very ugly
			VectorSet( viewoffset, 0.0f, 0.0f, cg.predictedPlayerState.viewheight );

			for( int i = 0; i < 3; i++ ) {
				view->origin[i] = cg.predictedPlayerState.pmove.origin[i] + viewoffset[i] - ( 1.0f - cg.lerpfrac ) * cg.predictionError[i];
				view->angles[i] = cg.predictedPlayerState.viewangles[i];
			}

			CG_ViewSmoothPredictedSteps( view->origin ); // smooth out stair climbing
		} else {
			cg.predictingTimeStamp = cl.serverTime;
			cg.predictFrom = 0;

			// we don't run prediction, but we still set cg.predictedPlayerState with the interpolation
			CG_InterpolatePlayerState( &cg.predictedPlayerState );

			VectorSet( viewoffset, 0.0f, 0.0f, cg.predictedPlayerState.viewheight );

			VectorAdd( cg.predictedPlayerState.pmove.origin, viewoffset, view->origin );
			VectorCopy( cg.predictedPlayerState.viewangles, view->angles );
		}

		view->fov_y = WidescreenFov( CG_CalcViewFov() );

		CG_CalcViewBob();

		VectorCopy( cg.predictedPlayerState.pmove.velocity, view->velocity );
	} else if( view->type == VIEWDEF_DEMOCAM ) {
		view->fov_y = WidescreenFov( CG_DemoCam_GetOrientation( view->origin, view->angles, view->velocity ) );
	}

	view->fov_x = CalcHorizontalFov( view->fov_y, frame_static.viewport_width, frame_static.viewport_height );

	Matrix3_FromAngles( view->angles, view->axis );

	view->fracDistFOV = tan( DEG2RAD( view->fov_x ) * 0.5f );

	if( view->thirdperson ) {
		CG_ThirdPersonOffsetView( view );
	}

	if( !view->playerPrediction ) {
		cg.predictedWeaponSwitch = 0;
	}
}

static void DrawWorld() {
	ZoneScoped;

	const char * suffix = "*0";
	u64 hash = Hash64( suffix, strlen( suffix ), cgs.map->base_hash );
	const Model * model = FindModel( StringHash( hash ) );

	for( u32 i = 0; i < model->num_primitives; i++ ) {
		if( model->primitives[ i ].material->blend_func == BlendFunc_Disabled ) {
			PipelineState pipeline;
			pipeline.pass = frame_static.write_world_gbuffer_pass;
			pipeline.shader = &shaders.write_world_gbuffer;
			pipeline.set_uniform( "u_View", frame_static.view_uniforms );
			pipeline.set_uniform( "u_Model", frame_static.identity_model_uniforms );

			DrawModelPrimitive( model, &model->primitives[ i ], pipeline );
		}

		{
			PipelineState pipeline = MaterialToPipelineState( model->primitives[ i ].material );
			pipeline.set_uniform( "u_View", frame_static.view_uniforms );
			pipeline.set_uniform( "u_Model", frame_static.identity_model_uniforms );

			DrawModelPrimitive( model, &model->primitives[ i ], pipeline );
		}
	}

	{
		bool msaa = frame_static.msaa_samples >= 1;

		PipelineState pipeline;
		pipeline.pass = frame_static.postprocess_world_gbuffer_pass;
		pipeline.shader = msaa ? &shaders.postprocess_world_gbuffer_msaa : &shaders.postprocess_world_gbuffer;

		const Framebuffer & fb = frame_static.world_gbuffer;
		pipeline.set_texture( "u_DepthTexture", &fb.depth_texture );
		pipeline.set_texture( "u_NormalTexture", &fb.normal_texture );
		pipeline.set_uniform( "u_View", frame_static.view_uniforms );

		DrawFullscreenMesh( pipeline );
	}

	{
		PipelineState pipeline;
		pipeline.pass = frame_static.add_world_outlines_pass;
		pipeline.shader = &shaders.standard_vertexcolors;
		pipeline.blend_func = BlendFunc_Add;
		pipeline.write_depth = false;

		const Framebuffer & fb = frame_static.world_outlines_fb;
		pipeline.set_texture( "u_BaseTexture", &fb.albedo_texture );
		pipeline.set_uniform( "u_View", frame_static.ortho_view_uniforms );
		pipeline.set_uniform( "u_Model", frame_static.identity_model_uniforms );
		pipeline.set_uniform( "u_Material", frame_static.identity_material_uniforms );

		Vec3 positions[] = {
			Vec3( 0, 0, 0 ),
			Vec3( frame_static.viewport_width, 0, 0 ),
			Vec3( 0, frame_static.viewport_height, 0 ),
			Vec3( frame_static.viewport_width, frame_static.viewport_height, 0 ),
		};

		Vec2 half_pixel = 0.5f / frame_static.viewport;
		Vec2 uvs[] = {
			Vec2( half_pixel.x, 1.0f - half_pixel.y ),
			Vec2( 1.0f - half_pixel.x, 1.0f - half_pixel.y ),
			Vec2( half_pixel.x, half_pixel.y ),
			Vec2( 1.0f - half_pixel.x, half_pixel.y ),
		};

		constexpr RGBA8 gray = RGBA8( 100, 100, 100, 255 );
		constexpr RGBA8 colors[] = { gray, gray, gray, gray };

		u16 base_index = DynamicMeshBaseIndex();
		u16 indices[] = { 0, 2, 1, 3, 1, 2 };
		for( u16 & idx : indices ) {
			idx += base_index;
		}

		DynamicMesh mesh = { };
		mesh.positions = positions;
		mesh.uvs = uvs;
		mesh.colors = colors;
		mesh.indices = indices;
		mesh.num_vertices = 4;
		mesh.num_indices = 6;

		DrawDynamicMesh( pipeline, mesh );
	}
}

static void DrawSilhouettes() {
	ZoneScoped;

	{
		PipelineState pipeline;
		pipeline.pass = frame_static.postprocess_silhouette_gbuffer_pass;
		pipeline.shader = &shaders.postprocess_silhouette_gbuffer;

		const Framebuffer & fb = frame_static.silhouette_gbuffer;
		pipeline.set_texture( "u_SilhouetteTexture", &fb.albedo_texture );
		pipeline.set_uniform( "u_View", frame_static.view_uniforms );

		DrawFullscreenMesh( pipeline );
	}

	{
		PipelineState pipeline;
		pipeline.pass = frame_static.add_silhouettes_pass;
		pipeline.shader = &shaders.standard;
		pipeline.depth_func = DepthFunc_Disabled;
		pipeline.blend_func = BlendFunc_Blend;
		pipeline.write_depth = false;

		const Framebuffer & fb = frame_static.silhouette_silhouettes_fb;
		pipeline.set_texture( "u_BaseTexture", &fb.albedo_texture );
		pipeline.set_uniform( "u_View", frame_static.ortho_view_uniforms );
		pipeline.set_uniform( "u_Model", frame_static.identity_model_uniforms );
		pipeline.set_uniform( "u_Material", frame_static.identity_material_uniforms );

		Vec3 positions[] = {
			Vec3( 0, 0, 0 ),
			Vec3( frame_static.viewport_width, 0, 0 ),
			Vec3( 0, frame_static.viewport_height, 0 ),
			Vec3( frame_static.viewport_width, frame_static.viewport_height, 0 ),
		};

		Vec2 half_pixel = 0.5f / frame_static.viewport;
		Vec2 uvs[] = {
			Vec2( half_pixel.x, 1.0f - half_pixel.y ),
			Vec2( 1.0f - half_pixel.x, 1.0f - half_pixel.y ),
			Vec2( half_pixel.x, half_pixel.y ),
			Vec2( 1.0f - half_pixel.x, half_pixel.y ),
		};

		constexpr RGBA8 colors[] = { rgba8_white, rgba8_white, rgba8_white, rgba8_white };

		u16 base_index = DynamicMeshBaseIndex();
		u16 indices[] = { 0, 2, 1, 3, 1, 2 };
		for( u16 & idx : indices ) {
			idx += base_index;
		}

		DynamicMesh mesh = { };
		mesh.positions = positions;
		mesh.uvs = uvs;
		mesh.colors = colors;
		mesh.indices = indices;
		mesh.num_vertices = 4;
		mesh.num_indices = 6;

		DrawDynamicMesh( pipeline, mesh );
	}
}

/*
* CG_RenderView
*/
void CG_RenderView( unsigned extrapolationTime ) {
	ZoneScoped;

	refdef_t *rd = &cg.view.refdef;

	cg.frameCount++;

	if( !cgs.precacheDone || !cg.frame.valid ) {
		CG_Precache();
		CG_DrawLoading();
		return;
	}

	{
		// moved this from CG_Init here
		cgs.extrapolationTime = extrapolationTime;

		if( cg.oldFrame.serverTime == cg.frame.serverTime ) {
			cg.lerpfrac = 1.0f;
		} else {
			int snapTime = cg.frame.serverTime - cg.oldFrame.serverTime;
			cg.lerpfrac = ( (double)( cl.serverTime - cgs.extrapolationTime ) - (double)cg.oldFrame.serverTime ) / (double)snapTime;
		}

		if( cgs.extrapolationTime ) {
			cg.xerpTime = 0.001f * ( (double)cl.serverTime - (double)cg.frame.serverTime );
			cg.oldXerpTime = 0.001f * ( (double)cl.serverTime - (double)cg.oldFrame.serverTime );

			if( cl.serverTime >= cg.frame.serverTime ) {
				cg.xerpSmoothFrac = Clamp01( double( cl.serverTime - cg.frame.serverTime ) / double( cgs.extrapolationTime ) );
			} else {
				cg.xerpSmoothFrac = Clamp( -1.0, double( cg.frame.serverTime - cl.serverTime ) / double( cgs.extrapolationTime ), 0.0 );
				cg.xerpSmoothFrac = 1.0f - cg.xerpSmoothFrac;
			}

			cg.xerpTime = Max2( cg.xerpTime, -( cgs.extrapolationTime * 0.001f ) );
		} else {
			cg.xerpTime = 0.0f;
			cg.xerpSmoothFrac = 0.0f;
		}
	}

	if( cg_showClamp->integer ) {
		if( cg.lerpfrac > 1.0f ) {
			CG_Printf( "high clamp %f\n", cg.lerpfrac );
		} else if( cg.lerpfrac < 0.0f ) {
			CG_Printf( "low clamp  %f\n", cg.lerpfrac );
		}
	}

	cg.lerpfrac = Clamp01( cg.lerpfrac );

	if( !cgs.configStrings[CS_WORLDMODEL][0] ) {
		CG_AddLocalSounds();

		// trap_R_DrawStretchPic( 0, 0, frame_static.viewport_width, frame_static.viewport_height, 0, 0, 1, 1, colorBlack, cgs.shaderWhite );

		S_Update( Vec3( 0 ), Vec3( 0 ), axis_identity );

		return;
	}

	if( cg_fov->modified ) {
		if( cg_fov->value < MIN_FOV ) {
			trap_Cvar_ForceSet( cg_fov->name, STR_TOSTR( MIN_FOV ) );
		} else if( cg_fov->value > MAX_FOV ) {
			trap_Cvar_ForceSet( cg_fov->name, STR_TOSTR( MAX_FOV ) );
		}
		cg_fov->modified = false;
	}

	if( cg_zoomfov->modified ) {
		if( cg_zoomfov->value < MIN_FOV ) {
			trap_Cvar_ForceSet( cg_zoomfov->name, STR_TOSTR( MIN_FOV ) );
		} else if( cg_zoomfov->value > MAX_FOV ) {
			trap_Cvar_ForceSet( cg_zoomfov->name, STR_TOSTR( MAX_FOV ) );
		}
		cg_zoomfov->modified = false;
	}

	CG_FlashGameWindow(); // notify player of important game events

	CG_UpdateChaseCam();

	CG_ClearFragmentedDecals();

	if( CG_DemoCam_Update() ) {
		CG_SetupViewDef( &cg.view, CG_DemoCam_GetViewType() );
	} else {
		CG_SetupViewDef( &cg.view, VIEWDEF_PLAYERVIEW );
	}

	RendererSetView( FromQF3( cg.view.origin ), FromQFAngles( cg.view.angles ), cg.view.fov_y );
	frame_static.fog_uniforms = UploadUniformBlock( cgs.map->fog_strength );

	CG_LerpEntities();  // interpolate packet entities positions

	CG_CalcViewWeapon( &cg.weapon );

	CG_FireEvents( false );

	CG_ResetBombHUD();

	DrawWorld();
	DrawSilhouettes();
	CG_AddEntities();
	CG_AddViewWeapon( &cg.weapon );
	CG_AddLocalEntities();
	CG_AddParticles();
	DrawGibs();
	DrawParticles();
	DrawPersistentBeams();

	CG_AddDlights();
	CG_AddPlayerShadows();
	CG_AddDecals();
	DrawSkybox();

	CG_AddLocalSounds();

	CG_SetupRefDef( &cg.view, rd );

	cg.oldAreabits = true;

	S_Update( FromQF3( cg.view.origin ), FromQF3( cg.view.velocity ), cg.view.axis );

	CG_Draw2D();
}
