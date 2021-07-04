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
#include "client/renderer/renderer.h"
#include "client/renderer/skybox.h"

ChasecamState chaseCam;

int CG_LostMultiviewPOV();

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

				if( checkPlayer != index && ISREALSPECTATOR() ) {
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
		Cbuf_ExecuteText( EXEC_NOW, step > 0 ? "chasenext" : "chaseprev" );
		return true;
	}

	return false;
}

/*
* CG_AddLocalSounds
*/
static void CG_AddLocalSounds() {
	static unsigned lastSecond = 0;

	// add local announces
	if( GS_Countdown( &client_gs ) ) {
		if( client_gs.gameState.match_duration ) {
			s64 curtime = GS_MatchPaused( &client_gs ) ? cg.frame.serverTime : cl.serverTime;
			s64 duration = client_gs.gameState.match_duration;

			if( duration + client_gs.gameState.match_state_start_time < curtime ) {
				duration = curtime - client_gs.gameState.match_state_start_time; // avoid negative results
			}

			float seconds = (float)( client_gs.gameState.match_state_start_time + duration - curtime ) * 0.001f;
			unsigned int remainingSeconds = (unsigned int)seconds;

			if( remainingSeconds != lastSecond ) {
				if( 1 + remainingSeconds < 4 ) {
					constexpr StringHash countdown[] = {
						"sounds/announcer/1",
						"sounds/announcer/2",
						"sounds/announcer/3",
					};

					CG_AddAnnouncerEvent( countdown[ remainingSeconds ], false );
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
static void CG_FlashGameWindow() {
	static int oldState = -1;
	bool flash = false;
	static u8 oldAlphaScore, oldBetaScore;
	static bool scoresSet = false;

	// notify player of important match states
	int newState = client_gs.gameState.match_state;
	if( oldState != newState ) {
		switch( newState ) {
			case MatchState_Countdown:
			case MatchState_Playing:
			case MatchState_PostMatch:
				flash = true;
				break;
			default:
				break;
		}

		oldState = newState;
	}

	// notify player of teams scoring in team-based gametypes
	if( !scoresSet ||
		( oldAlphaScore != client_gs.gameState.teams[ TEAM_ALPHA ].score || oldBetaScore != client_gs.gameState.teams[ TEAM_BETA ].score ) ) {
		oldAlphaScore = client_gs.gameState.teams[ TEAM_ALPHA ].score;
		oldBetaScore = client_gs.gameState.teams[ TEAM_BETA ].score;

		flash = scoresSet && GS_TeamBasedGametype( &client_gs );
		scoresSet = true;
	}

	if( flash ) {
		FlashWindow();
	}
}

Vec3 CG_GetKickAngles() {
	Vec3 angles = Vec3( 0.0f );

	for( int i = 0; i < MAX_ANGLES_KICKS; i++ ) {
		if( cl.serverTime > cg.kickangles[i].timestamp + cg.kickangles[i].kicktime ) {
			continue;
		}

		float time = (float)( ( cg.kickangles[i].timestamp + cg.kickangles[i].kicktime ) - cl.serverTime );
		float uptime = ( (float)cg.kickangles[i].kicktime ) * 0.5f;
		float delta = 1.0f - ( Abs( time - uptime ) / uptime );

		//CG_Printf("Kick Delta:%f\n", delta );
		if( delta > 1.0f ) {
			delta = 1.0f;
		}
		if( delta <= 0.0f ) {
			continue;
		}

		angles.x += cg.kickangles[i].v_pitch * delta;
		angles.z += cg.kickangles[i].v_roll * delta;
	}

	return angles;
}

/*
* CG_CalcViewFov
*/
float CG_CalcViewFov() {
	float hardcoded_fov = 107.9f; // TODO: temp hardcoded fov

	WeaponType weapon = cg.predictedPlayerState.weapon;
	if( weapon == Weapon_None )
		return hardcoded_fov;

	float zoom_fov = GS_GetWeaponDef( weapon )->zoom_fov;
	float frac = float( cg.predictedPlayerState.zoom_time ) / float( ZOOMTIME );
	return Lerp( hardcoded_fov, frac, float( zoom_fov ) );
}

/*
* CG_CalcViewBob
*/
static void CG_CalcViewBob() {
	float bobMove, bobTime, bobScale;

	if( !cg.view.drawWeapon ) {
		return;
	}

	// calculate speed and cycle to be used for all cyclic walking effects
	cg.xyspeed = Length( cg.predictedPlayerState.pmove.velocity.xy() );

	bobScale = 0;
	if( cg.xyspeed < 5 ) {
		cg.oldBobTime = 0;  // start at beginning of cycle again
	}
	else {
		if( !ISVIEWERENTITY( cg.view.POVent ) ) {
			bobScale = 0.0f;
		} else if( CG_PointContents( cg.view.origin ) & MASK_WATER ) {
			bobScale =  0.75f;
		} else {
			trace_t trace;

			const centity_t * cent = &cg_entities[cg.view.POVent];
			Vec3 maxs = cent->current.bounds.mins;
			Vec3 mins = maxs - Vec3( 0.0f, 0.0f, 1.6f * STEPSIZE );

			CG_Trace( &trace, cg.predictedPlayerState.pmove.origin, mins, maxs, cg.predictedPlayerState.pmove.origin, cg.view.POVent, MASK_PLAYERSOLID );
			if( trace.startsolid || trace.allsolid ) {
				if( cg.predictedPlayerState.pmove.crouch_time != 0 ) {
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
	cg.bobFracSin = Abs( sinf( bobTime * PI ) );
}

/*
* CG_ResetKickAngles
*/
void CG_ResetKickAngles() {
	memset( cg.kickangles, 0, sizeof( cg.kickangles ) );
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

//============================================================================

/*
* CG_InterpolatePlayerState
*/
static void CG_InterpolatePlayerState( SyncPlayerState *playerState ) {
	const SyncPlayerState * ps = &cg.frame.playerState;
	const SyncPlayerState * ops = &cg.oldFrame.playerState;

	*playerState = *ps;

	bool teleported = ( ps->pmove.pm_flags & PMF_TIME_TELEPORT ) != 0;

	if( Abs( ops->pmove.origin.x - ps->pmove.origin.x ) > 256
		|| Abs( ops->pmove.origin.y - ps->pmove.origin.y ) > 256
		|| Abs( ops->pmove.origin.z - ps->pmove.origin.z ) > 256 ) {
		teleported = true;
	}

	// if the player entity was teleported this frame use the final position
	if( !teleported ) {
		playerState->pmove.origin = Lerp( ops->pmove.origin, cg.lerpfrac, ps->pmove.origin );
		playerState->pmove.velocity = Lerp( ops->pmove.velocity, cg.lerpfrac, ps->pmove.velocity );
		playerState->viewangles = LerpAngles( ops->viewangles, cg.lerpfrac, ps->viewangles );
		playerState->viewheight = Lerp( ops->viewheight, cg.lerpfrac, ps->viewheight );
	}

	playerState->pmove.velocity = Lerp( ops->pmove.velocity, cg.lerpfrac, ps->pmove.velocity );

	playerState->zoom_time = Lerp( ops->zoom_time, cg.lerpfrac, ps->zoom_time );

	if( ps->weapon_state_time >= ops->weapon_state_time ) {
		playerState->weapon_state_time = Lerp( ops->weapon_state_time, cg.lerpfrac, ps->weapon_state_time );
	}
	else {
		s64 dt = cg.frame.serverTime - cg.oldFrame.serverTime;
		playerState->weapon_state_time = Min2( float( U16_MAX ), ops->weapon_state_time + cg.lerpfrac * dt );
	}
}

/*
* CG_ThirdPersonOffsetView
*/
static void CG_ThirdPersonOffsetView( cg_viewdef_t *view ) {
	float dist, f, r;
	trace_t trace;
	Vec3 mins( -4.0f );
	Vec3 maxs( 4.0f );

	if( !cg_thirdPersonAngle || !cg_thirdPersonRange ) {
		cg_thirdPersonAngle = Cvar_Get( "cg_thirdPersonAngle", "0", CVAR_ARCHIVE );
		cg_thirdPersonRange = Cvar_Get( "cg_thirdPersonRange", "70", CVAR_ARCHIVE );
	}

	// calc exact destination
	Vec3 chase_dest = view->origin;
	r = Radians( cg_thirdPersonAngle->value );
	f = -cosf( r );
	r = -sinf( r );
	chase_dest += FromQFAxis( view->axis, AXIS_FORWARD ) * ( cg_thirdPersonRange->value * f );
	chase_dest += FromQFAxis( view->axis, AXIS_RIGHT ) * ( cg_thirdPersonRange->value * r );
	chase_dest.z += 8;

	// find the spot the player is looking at
	Vec3 dest = view->origin + FromQFAxis( view->axis, AXIS_FORWARD ) * 512.0f;
	CG_Trace( &trace, view->origin, mins, maxs, dest, view->POVent, MASK_SOLID );

	// calculate pitch to look at the same spot from camera
	Vec3 stop = trace.endpos - view->origin;
	dist = Length( stop.xy() );
	if( dist < 1 ) {
		dist = 1;
	}
	view->angles.x = Degrees( -atan2f( stop.z, dist ) );
	view->angles.y -= cg_thirdPersonAngle->value;
	Matrix3_FromAngles( view->angles, view->axis );

	// move towards destination
	CG_Trace( &trace, view->origin, mins, maxs, chase_dest, view->POVent, MASK_SOLID );

	if( trace.fraction != 1.0 ) {
		stop = trace.endpos;
		stop.z += ( 1.0 - trace.fraction ) * 32;
		CG_Trace( &trace, view->origin, mins, maxs, stop, view->POVent, MASK_SOLID );
		chase_dest = trace.endpos;
	}

	view->origin = chase_dest;
}

/*
* CG_ViewSmoothPredictedSteps
*/
void CG_ViewSmoothPredictedSteps( Vec3 * vieworg ) {
	int timeDelta;

	// smooth out stair climbing
	timeDelta = cls.realtime - cg.predictedStepTime;
	if( timeDelta < PREDICTED_STEP_TIME ) {
		vieworg->z -= cg.predictedStep * ( PREDICTED_STEP_TIME - timeDelta ) / PREDICTED_STEP_TIME;
	}
}

/*
* CG_ViewSmoothFallKick
*/
float CG_ViewSmoothFallKick() {
	// fallkick offset
	if( cg.fallEffectTime > cl.serverTime ) {
		float fallfrac = (float)( cl.serverTime - cg.fallEffectRebounceTime ) / (float)( cg.fallEffectTime - cg.fallEffectRebounceTime );
		float fallkick = -1.0f * sinf( Radians( fallfrac * 180 ) ) * ( ( cg.fallEffectTime - cg.fallEffectRebounceTime ) * 0.01f );
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
bool CG_SwitchChaseCamMode() {
	bool chasecam = ( cg.frame.playerState.pmove.pm_type == PM_CHASECAM )
					&& ( cg.frame.playerState.POVnum != (unsigned)( cgs.playerNum + 1 ) );
	bool realSpec = cgs.demoPlaying || ISREALSPECTATOR();

	if( ( cg.frame.multipov || chasecam ) && !CG_DemoCam_IsFree() ) {
		if( chasecam ) {
			if( realSpec ) {
				if( ++chaseCam.mode >= CAM_MODES ) {
					// if exceeds the cycle, start free fly
					Cbuf_ExecuteText( EXEC_NOW, "camswitch" );
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
		Cbuf_ExecuteText( EXEC_NOW, "camswitch" );
		return true;
	}

	return false;
}

/*
* CG_UpdateChaseCam
*/
static void CG_UpdateChaseCam() {
	bool chasecam = ( cg.frame.playerState.pmove.pm_type == PM_CHASECAM )
					&& ( cg.frame.playerState.POVnum != (unsigned)( cgs.playerNum + 1 ) );

	if( !( cg.frame.multipov || chasecam ) || CG_DemoCam_IsFree() ) {
		chaseCam.mode = CAM_INEYES;
	}

	usercmd_t cmd;
	CL_GetUserCmd( CL_GetCurrentUserCmdNum() - 1, &cmd );

	if( chaseCam.key_pressed ) {
		chaseCam.key_pressed = ( cmd.buttons & ( BUTTON_ATTACK | BUTTON_SPECIAL ) ) != 0 || cmd.upmove != 0 || cmd.sidemove != 0;
		return;
	}

	if( cmd.buttons & BUTTON_ATTACK ) {
		CG_SwitchChaseCamMode();
		chaseCam.key_pressed = true;
	}

	int chaseStep = 0;

	if( cg.view.type == VIEWDEF_PLAYERVIEW ) {
		if( cmd.upmove > 0 || cmd.sidemove > 0 || ( cmd.buttons & BUTTON_SPECIAL ) ) {
			chaseStep = 1;
		}
		else if( cmd.upmove < 0 || cmd.sidemove < 0 ) {
			chaseStep = -1;
		}
	}

	if( chaseStep != 0 ) {
		CG_ChaseStep( chaseStep );
		chaseCam.key_pressed = true;
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
			if( cg_entities[view->POVent].serverFrame == cg.frame.serverFrame && cg_entities[view->POVent].current.weapon != Weapon_Count ) {
				view->drawWeapon = true;
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
		Com_Error( ERR_DROP, "CG_SetupView: Invalid view type %i\n", view->type );
	}

	if( view->type == VIEWDEF_PLAYERVIEW ) {
		Vec3 viewoffset;

		if( view->playerPrediction ) {
			CG_PredictMovement();

			// fixme: crouching is predicted now, but it looks very ugly
			viewoffset = Vec3( 0.0f, 0.0f, cg.predictedPlayerState.viewheight );

			view->origin = cg.predictedPlayerState.pmove.origin + viewoffset - ( 1.0f - cg.lerpfrac ) * cg.predictionError;
			view->angles = cg.predictedPlayerState.viewangles;

			CG_Recoil( cg.predictedPlayerState.weapon );

			CG_ViewSmoothPredictedSteps( &view->origin ); // smooth out stair climbing
		} else {
			cg.predictingTimeStamp = cl.serverTime;
			cg.predictFrom = 0;

			// we don't run prediction, but we still set cg.predictedPlayerState with the interpolation
			CG_InterpolatePlayerState( &cg.predictedPlayerState );

			viewoffset = Vec3( 0.0f, 0.0f, cg.predictedPlayerState.viewheight );

			view->origin = cg.predictedPlayerState.pmove.origin + viewoffset;
			view->angles = cg.predictedPlayerState.viewangles;
		}

		view->fov_y = WidescreenFov( CG_CalcViewFov() );

		CG_CalcViewBob();

		view->velocity = cg.predictedPlayerState.pmove.velocity;
	} else if( view->type == VIEWDEF_DEMOCAM ) {
		view->fov_y = WidescreenFov( CG_DemoCam_GetOrientation( &view->origin, &view->angles, &view->velocity ) );
	}

	if( cg.predictedPlayerState.health <= 0 && cg.predictedPlayerState.team != TEAM_SPECTATOR ) {
		AddDamageEffect();
	}
	else {
		cg.damage_effect *= 0.97f;
		if( cg.damage_effect <= 0.001f ) {
			cg.damage_effect = 0.0f;
		}
	}

	view->fov_x = CalcHorizontalFov( view->fov_y, frame_static.viewport_width, frame_static.viewport_height );

	Matrix3_FromAngles( view->angles, view->axis );

	view->fracDistFOV = tanf( Radians( view->fov_x ) * 0.5f );

	if( view->thirdperson ) {
		CG_ThirdPersonOffsetView( view );
	}

	if( !view->playerPrediction ) {
		cg.recoiling = false;
	}
}

static void DrawWorld() {
	ZoneScoped;

	const char * suffix = "*0";
	u64 hash = Hash64( suffix, strlen( suffix ), cl.map->base_hash );
	const Model * model = FindModel( StringHash( hash ) );

	for( u32 i = 0; i < model->num_primitives; i++ ) {
		if( model->primitives[ i ].material->blend_func == BlendFunc_Disabled ) {
			PipelineState pipeline;
			pipeline.pass = frame_static.near_shadowmap_pass;
			pipeline.shader = &shaders.depth_only;
			pipeline.clamp_depth = true;
			pipeline.cull_face = CullFace_Disabled;
			pipeline.set_uniform( "u_View", frame_static.near_shadowmap_view_uniforms );
			pipeline.set_uniform( "u_Model", frame_static.identity_model_uniforms );

			DrawModelPrimitive( model, &model->primitives[ i ], pipeline );
		}

		if( model->primitives[ i ].material->blend_func == BlendFunc_Disabled ) {
			PipelineState pipeline;
			pipeline.pass = frame_static.far_shadowmap_pass;
			pipeline.shader = &shaders.depth_only;
			pipeline.clamp_depth = true;
			pipeline.cull_face = CullFace_Disabled;
			pipeline.set_uniform( "u_View", frame_static.far_shadowmap_view_uniforms );
			pipeline.set_uniform( "u_Model", frame_static.identity_model_uniforms );

			DrawModelPrimitive( model, &model->primitives[ i ], pipeline );
		}

		{
			PipelineState pipeline;
			pipeline.pass = frame_static.world_opaque_prepass_pass;
			pipeline.shader = &shaders.depth_only;
			pipeline.set_uniform( "u_View", frame_static.view_uniforms );
			pipeline.set_uniform( "u_Model", frame_static.identity_model_uniforms );

			DrawModelPrimitive( model, &model->primitives[ i ], pipeline );
		}

		{
			PipelineState pipeline = MaterialToPipelineState( model->primitives[ i ].material );
			pipeline.set_uniform( "u_View", frame_static.view_uniforms );
			pipeline.set_uniform( "u_Model", frame_static.identity_model_uniforms );
			pipeline.write_depth = false;
			pipeline.depth_func = DepthFunc_Equal;

			DrawModelPrimitive( model, &model->primitives[ i ], pipeline );
		}
	}

	{
		bool msaa = frame_static.msaa_samples >= 1;

		PipelineState pipeline;
		pipeline.pass = frame_static.add_world_outlines_pass;
		pipeline.shader = msaa ? &shaders.postprocess_world_gbuffer_msaa : &shaders.postprocess_world_gbuffer;
		pipeline.depth_func = DepthFunc_Disabled;
		pipeline.blend_func = BlendFunc_Blend;
		pipeline.write_depth = false;

		constexpr RGBA8 gray = RGBA8( 30, 30, 30, 255 );

		const Framebuffer & fb = msaa ? frame_static.msaa_fb : frame_static.postprocess_fb;
		pipeline.set_texture( "u_DepthTexture", &fb.depth_texture );
		pipeline.set_uniform( "u_Fog", frame_static.fog_uniforms );
		pipeline.set_uniform( "u_View", frame_static.view_uniforms );
		pipeline.set_uniform( "u_Outline", UploadUniformBlock( sRGBToLinear( gray ) ) );
		DrawFullscreenMesh( pipeline );
	}
}

static void DrawSilhouettes() {
	ZoneScoped;

	{
		PipelineState pipeline;
		pipeline.pass = frame_static.add_silhouettes_pass;
		pipeline.shader = &shaders.postprocess_silhouette_gbuffer;
		pipeline.depth_func = DepthFunc_Disabled;
		pipeline.blend_func = BlendFunc_Blend;
		pipeline.write_depth = false;

		const Framebuffer & fb = frame_static.silhouette_gbuffer;
		pipeline.set_texture( "u_SilhouetteTexture", &fb.albedo_texture );
		pipeline.set_uniform( "u_View", frame_static.ortho_view_uniforms );
		DrawFullscreenMesh( pipeline );
	}
}

void CG_RenderView( unsigned extrapolationTime ) {
	ZoneScoped;

	cg.frameCount++;

	if( !cgs.precacheDone || !cg.frame.valid ) {
		CG_Precache();
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
			Com_Printf( "high clamp %f\n", cg.lerpfrac );
		} else if( cg.lerpfrac < 0.0f ) {
			Com_Printf( "low clamp  %f\n", cg.lerpfrac );
		}
	}

	cg.lerpfrac = Clamp01( cg.lerpfrac );

	{
		constexpr float SYSTEM_FONT_TINY_SIZE = 8;
		constexpr float SYSTEM_FONT_SMALL_SIZE = 14;
		constexpr float SYSTEM_FONT_MEDIUM_SIZE = 16;
		constexpr float SYSTEM_FONT_BIG_SIZE = 24;

		float scale = frame_static.viewport_height / 600.0f;

		cgs.fontSystemTinySize = ceilf( SYSTEM_FONT_TINY_SIZE * scale );
		cgs.fontSystemSmallSize = ceilf( SYSTEM_FONT_SMALL_SIZE * scale );
		cgs.fontSystemMediumSize = ceilf( SYSTEM_FONT_MEDIUM_SIZE * scale );
		cgs.fontSystemBigSize = ceilf( SYSTEM_FONT_BIG_SIZE * scale );

		scale *= 1.3f;
		cgs.textSizeTiny = SYSTEM_FONT_TINY_SIZE * scale;
		cgs.textSizeSmall = SYSTEM_FONT_SMALL_SIZE * scale;
		cgs.textSizeMedium = SYSTEM_FONT_MEDIUM_SIZE * scale;
		cgs.textSizeBig = SYSTEM_FONT_BIG_SIZE * scale;
	}

	CG_FlashGameWindow(); // notify player of important game events

	AllocateDecalBuffers();

	CG_UpdateChaseCam();

	if( CG_DemoCam_Update() ) {
		CG_SetupViewDef( &cg.view, CG_DemoCam_GetViewType() );
	} else {
		CG_SetupViewDef( &cg.view, VIEWDEF_PLAYERVIEW );
	}

	RendererSetView( cg.view.origin, EulerDegrees3( cg.view.angles ), cg.view.fov_y );
	frame_static.fog_uniforms = UploadUniformBlock( cl.map->fog_strength );

	CG_LerpEntities();  // interpolate packet entities positions

	CG_CalcViewWeapon( &cg.weapon );

	CG_FireEvents( false );

	CG_ResetBombHUD();

	DoVisualEffect( "vfx/rain", cg.view.origin );

	DrawWorld();
	DrawSilhouettes();
	DrawEntities();
	CG_AddViewWeapon( &cg.weapon );
	DrawGibs();
	DrawParticles();
	DrawPersistentBeams();
	DrawPersistentDecals();
	DrawPersistentDynamicLights();
	DrawSkybox();
	DrawSprays();

	CG_AddLocalSounds();

	S_Update( cg.view.origin, cg.view.velocity, cg.view.axis );

	CG_Draw2D();

	UploadDecalBuffers();
}
