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
#include "client/audio/api.h"
#include "client/renderer/renderer.h"
#include "client/renderer/skybox.h"
#include "qcommon/time.h"

#include "gameshared/collision.h"

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
		TempAllocator temp = cls.frame_arena.temp();
		Cmd_Execute( &temp, step > 0 ? "chasenext" : "chaseprev" );
		return true;
	}

	return false;
}

float CG_CalcViewFov() {
	WeaponType weapon = cg.predictedPlayerState.weapon;
	if( weapon == Weapon_None )
		return FOV;

	float zoom_fov = GS_GetWeaponDef( weapon )->zoom_fov;
	float frac = float( cg.predictedPlayerState.zoom_time ) / float( ZOOMTIME );
	return Lerp( FOV, frac, float( zoom_fov ) );
}

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
		}
		else {
			const centity_t * cent = &cg_entities[cg.view.POVent];
			MinMax3 bounds = EntityBounds( ClientCollisionModelStorage(), &cent->current );
			Vec3 maxs = bounds.mins;
			Vec3 mins = maxs - Vec3( 0.0f, 0.0f, 1.6f * STEPSIZE );

			trace_t trace = CG_Trace( cg.predictedPlayerState.pmove.origin, MinMax3( mins, maxs ), cg.predictedPlayerState.pmove.origin, cg.view.POVent, SolidMask_Opaque );
			if( trace.GotNowhere() ) {
				bobScale = 2.5f;
			}
		}
	}

	bobMove = cls.frametime * bobScale * 0.001f;
	bobTime = ( cg.oldBobTime += bobMove );

	cg.bobCycle = (int)bobTime;
	cg.bobFracSin = Abs( sinf( bobTime * PI ) );
}

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

static void CG_InterpolatePlayerState( SyncPlayerState * playerState ) {
	const SyncPlayerState * ps = &cg.frame.playerState;
	const SyncPlayerState * ops = &cg.oldFrame.playerState;

	*playerState = *ops;

	bool teleported = cg_entities[ playerState->POVnum ].current.teleported || Length( ps->pmove.origin - ops->pmove.origin ) > 256.0f;
	if( !teleported ) {
		playerState->pmove.origin = Lerp( ops->pmove.origin, cg.lerpfrac, ps->pmove.origin );
		playerState->pmove.velocity = Lerp( ops->pmove.velocity, cg.lerpfrac, ps->pmove.velocity );
		playerState->viewangles = LerpAngles( ops->viewangles, cg.lerpfrac, ps->viewangles );
		playerState->viewheight = Lerp( ops->viewheight, cg.lerpfrac, ps->viewheight );
	}

	playerState->pmove.velocity = Lerp( ops->pmove.velocity, cg.lerpfrac, ps->pmove.velocity );
	playerState->pmove.stamina = Lerp( ops->pmove.stamina, cg.lerpfrac, ps->pmove.stamina );

	playerState->zoom_time = Lerp( ops->zoom_time, cg.lerpfrac, ps->zoom_time );
	playerState->flashed = Lerp( ops->flashed, cg.lerpfrac, ps->flashed );

	if( ops->progress_type == ps->progress_type ) {
		playerState->progress = Lerp( ops->progress, cg.lerpfrac, ps->progress );
	}

	// TODO: this should probably go through UpdateWeapons
	if( ps->weapon_state_time >= ops->weapon_state_time ) {
		playerState->weapon_state_time = Lerp( ops->weapon_state_time, cg.lerpfrac, ps->weapon_state_time );
	}
	else {
		s64 dt = cg.frame.serverTime - cg.oldFrame.serverTime;
		playerState->weapon_state = ops->weapon_state;
		playerState->weapon_state_time = Min2( float( U16_MAX ), ops->weapon_state_time + cg.lerpfrac * dt );
	}
}

static void CG_ThirdPersonOffsetView( cg_viewdef_t *view, bool hold_angle ) {
	float dist, f, r;

	// calc exact destination
	Vec3 chase_dest = view->origin;
	EulerDegrees3 angles = hold_angle ? -view->angles : EulerDegrees3( 0.0f, 0.0f, 0.0f );
	r = Radians( angles.yaw );
	f = -cosf( r );
	r = -sinf( r );
	chase_dest += FromQFAxis( view->axis, AXIS_FORWARD ) * ( cg_thirdPersonRange->number * f );
	chase_dest += FromQFAxis( view->axis, AXIS_RIGHT ) * ( cg_thirdPersonRange->number * r );
	chase_dest.z += 8;

	// find the spot the player is looking at
	Vec3 dest = view->origin + FromQFAxis( view->axis, AXIS_FORWARD ) * 512.0f;
	trace_t trace = CG_Trace( view->origin, MinMax3( 4.0f ), dest, view->POVent, SolidMask_AnySolid );

	// calculate pitch to look at the same spot from camera
	Vec3 stop = trace.endpos - view->origin;
	dist = Length( stop.xy() );
	if( dist < 1 ) {
		dist = 1;
	}
	view->angles.pitch = Degrees( -atan2f( stop.z, dist ) );
	view->angles.yaw -= angles.yaw;
	Matrix3_FromAngles( view->angles, view->axis );

	// move towards destination
	trace = CG_Trace( view->origin, MinMax3( 4.0f ), chase_dest, view->POVent, SolidMask_AnySolid );

	if( trace.HitSomething() ) {
		stop = trace.endpos;
		stop.z += ( 1.0f - trace.fraction ) * 32;
		trace = CG_Trace( view->origin, MinMax3( 4.0f ), stop, view->POVent, SolidMask_AnySolid );
		chase_dest = trace.endpos;
	}

	view->origin = chase_dest;
}

void CG_ViewSmoothPredictedSteps( Vec3 * vieworg ) {
	int timeDelta;

	// smooth out stair climbing
	timeDelta = cls.realtime - cg.predictedStepTime;
	if( timeDelta < PREDICTED_STEP_TIME ) {
		vieworg->z -= cg.predictedStep * ( PREDICTED_STEP_TIME - timeDelta ) / PREDICTED_STEP_TIME;
	}
}

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

static void CG_UpdateChaseCam( UserCommand * cmd ) {
	if( chaseCam.key_pressed ) {
		chaseCam.key_pressed = cmd->buttons != 0 || cmd->sidemove != 0 || cmd->forwardmove != 0;
		return;
	}

	if( cmd->buttons & Button_Attack1 ) {
		if( ISREALSPECTATOR() ) {
			CL_AddReliableCommand( ClientCommand_ToggleFreeFly );
		}
		chaseCam.key_pressed = true;
	}

	int chaseStep = 0;

	if( cg.view.type == ViewType_Player ) {
		if( cmd->sidemove > 0 || ( cmd->buttons & Button_Attack2 ) ) {
			chaseStep = 1;
		}
		else if( cmd->sidemove < 0 || ( cmd->buttons & Button_Attack1 ) ) {
			chaseStep = -1;
		}
	}

	if( chaseStep != 0 ) {
		CG_ChaseStep( chaseStep );
		chaseCam.key_pressed = true;
	}
}

float WidescreenFov( float fov ) {
	return atanf( tanf( fov / 360.0f * PI ) * 0.75f ) * ( 360.0f / PI );
}

float CalcHorizontalFov( const char * caller, float fov_y, float width, float height ) {
	if( fov_y < 1 || fov_y > 179 ) {
		Com_Printf( S_COLOR_YELLOW "Bad vertical fov: caller = %s, fov_y = %f, width = %f, height = %f\n", caller, fov_y, width, height );
		return 100.0f;
	}

	float x = width * tanf( fov_y / 360.0f * PI );
	return atanf( x / height ) * 360.0f / PI;
}

static void ScreenShake( cg_viewdef_t * view ) {
	if( !client_gs.gameState.exploding )
		return;

	s64 dt = cl.serverTime - client_gs.gameState.exploded_at;

	// TODO: we need this because the game drops you into busted noclip when you have noone to spec
	if( dt >= 3000 )
		return;

	float shake_amount = Unlerp01( s64( 3000 ), dt, s64( 0 ) ) * 4.0f;

	view->origin.x += shake_amount * RandomFloat11( &cls.rng );
	view->origin.y += shake_amount * RandomFloat11( &cls.rng );
	view->origin.z += shake_amount * RandomFloat11( &cls.rng );
}

static void CG_SetupViewDef( cg_viewdef_t *view, ViewType type, UserCommand * cmd ) {
	*view = { .type = type };

	if( view->type == ViewType_Player ) {
		view->POVent = cg.frame.playerState.POVnum;

		if( cg.oldFrame.gameState.camera_override.exists ) {
			const SyncGameState::CameraOverride & a = cg.oldFrame.gameState.camera_override.value;
			const SyncGameState::CameraOverride & b = cg.frame.gameState.camera_override.exists ? cg.frame.gameState.camera_override.value : a;

			view->origin = Lerp( a.origin, cg.lerpfrac, b.origin );
			view->angles = LerpAngles( a.angles, cg.lerpfrac, b.angles );

			float dt = float( cg.frame.serverTime - cg.oldFrame.serverTime ) / 1000.0f;
			view->velocity = ( b.origin - a.origin ) / dt;
		}
		else {
			view->draw2D = true;
			view->thirdperson = cg_thirdPerson->integer != 0;

			if( cg_entities[view->POVent].serverFrame != cg.frame.serverFrame ) {
				view->thirdperson = false;
			}

			// check for drawing gun
			if( !view->thirdperson && view->POVent > 0 && view->POVent <= client_gs.maxclients ) {
				if( cg_entities[view->POVent].serverFrame == cg.frame.serverFrame ) {
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

			if( view->playerPrediction ) {
				CG_PredictMovement();

				Vec3 viewoffset = Vec3( 0.0f, 0.0f, cg.predictedPlayerState.viewheight );
				view->origin = cg.predictedPlayerState.pmove.origin + viewoffset - ( 1.0f - cg.lerpfrac ) * cg.predictionError;

				view->angles = cg.predictedPlayerState.viewangles;

				CG_Recoil( cg.predictedPlayerState.weapon );

				CG_ViewSmoothPredictedSteps( &view->origin ); // smooth out stair climbing
			}
			else {
				cg.predictingTimeStamp = cl.serverTime;
				cg.predictFrom = 0;

				// we don't run prediction, but we still set cg.predictedPlayerState with the interpolation
				CG_InterpolatePlayerState( &cg.predictedPlayerState );

				Vec3 viewoffset = Vec3( 0.0f, 0.0f, cg.predictedPlayerState.viewheight );

				view->origin = cg.predictedPlayerState.pmove.origin + viewoffset;
				view->angles = cg.predictedPlayerState.viewangles;
			}

			CG_CalcViewBob();

			view->velocity = cg.predictedPlayerState.pmove.velocity;

			ScreenShake( view );

			if( cg.predictedPlayerState.health <= 0 && cg.predictedPlayerState.team != Team_None ) {
				AddDamageEffect();
			}
			else {
				cg.damage_effect *= 0.97f;
				if( cg.damage_effect <= 0.001f ) {
					cg.damage_effect = 0.0f;
				}
			}
		}
	}
	else if( view->type == ViewType_Demo ) {
		CG_DemoCamGetViewDef( view );

		EulerDegrees2 angles2;
		CG_DemoCamGetOrientation( &view->origin, &angles2, &view->velocity );
		view->angles = EulerDegrees3( angles2 );
	}

	view->fov_y = WidescreenFov( CG_CalcViewFov() );
	view->fov_x = CalcHorizontalFov( "SetupViewDef", view->fov_y, frame_static.viewport_width, frame_static.viewport_height );

	Matrix3_FromAngles( view->angles, view->axis );

	view->fracDistFOV = tanf( Radians( view->fov_x ) * 0.5f );

	if( view->thirdperson ) {
		CG_ThirdPersonOffsetView( view, view->type == ViewType_Player && cg_thirdPerson->integer == 2 );
	}

	if( !view->playerPrediction ) {
		cg.recoiling = false;
	}
}

static void DrawSilhouettes() {
	TracyZoneScoped;

	frame_static.render_passes[ RenderPass_AddSilhouettes ] = NewRenderPass( RenderPassConfig {
		.name = "Add silhouettes",
		.representative_shader = shaders.postprocess_silhouette_gbuffer,
		.bindings = {
			.textures = { { "u_SilhouetteTexture", rt.color_attachments[ FragmentShaderOutput_Albedo ] } },
		},
	} );

	PipelineState pipeline = {
		.shader = shaders.postprocess_silhouette_gbuffer,
		.dynamic_state = { .depth_func = DepthFunc_AlwaysNoWrite },
	};
	// pipeline.blend_func = BlendFunc_Blend;

	EncodeDrawCall( RenderPass_AddSilhouettes, pipeline, FullscreenMesh() );
}

static void DrawOutlines() {
	bool msaa = frame_static.msaa_samples > 1;

	constexpr RGBA8 gray = RGBA8( 30, 30, 30 );

	frame_static.render_passes[ RenderPass_AddOutlines ] = NewRenderPass( RenderPassConfig {
		.name = "Add outlines",
		.representative_shader = shaders.postprocess_world_gbuffer,
		.bindings = {
			.buffers = {
				{ "u_View", frame_static.ortho_view_uniforms },
				{ "u_OutlineColor", NewTempBuffer( sRGBToLinear( gray ) ) },
			},
			.textures = {
				{ "u_DepthTexture", frame_static.resolved_depth },
				{ "u_CurvedSurfaceMask", frame_static.curved_surface_mask },
			},
		},
	} );

	PipelineState pipeline = {
		.shader = msaa ? shaders.postprocess_world_gbuffer_msaa : shaders.postprocess_world_gbuffer,
		.dynamic_state = { .depth_func = DepthFunc_AlwaysNoWrite },
	};
	// pipeline.blend_func = BlendFunc_Blend;

	EncodeDrawCall( RenderPass_AddOutlines, pipeline, FullscreenMesh() );

	// const RenderTarget & rt = msaa ? frame_static.render_targets.msaa_masked : frame_static.render_targets.postprocess_masked;
	// pipeline.bind_texture_and_sampler( "u_DepthTexture", &rt.depth_attachment, Sampler_Standard );
	// pipeline.bind_texture_and_sampler( "u_CurvedSurfaceMask", &rt.color_attachments[ FragmentShaderOutput_CurvedSurfaceMask ], Sampler_Unfiltered );
	// pipeline.bind_uniform( "u_View", frame_static.view_uniforms );
	// pipeline.bind_uniform( "u_Outline", UploadUniformBlock( sRGBToLinear( gray ) ) );
	// DrawFullscreenMesh( pipeline );
}

void CG_RenderView( unsigned extrapolationTime ) {
	TracyZoneScoped;

	cg.frameCount++;

	Assert( cg.frame.valid );

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

	UserCommand cmd;
	CL_GetUserCmd( CL_GetCurrentUserCmdNum() - 1, &cmd );

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
		constexpr float SYSTEM_FONT_EXTRASMALL_SIZE = 12;
		constexpr float SYSTEM_FONT_SMALL_SIZE = 14;
		constexpr float SYSTEM_FONT_MEDIUM_SIZE = 16;
		constexpr float SYSTEM_FONT_BIG_SIZE = 24;

		float scale = frame_static.viewport_height / 600.0f;

		cgs.fontSystemTinySize = ceilf( SYSTEM_FONT_TINY_SIZE * scale );
		cgs.fontSystemExtraSmallSize = ceilf( SYSTEM_FONT_EXTRASMALL_SIZE * scale );
		cgs.fontSystemSmallSize = ceilf( SYSTEM_FONT_SMALL_SIZE * scale );
		cgs.fontSystemMediumSize = ceilf( SYSTEM_FONT_MEDIUM_SIZE * scale );
		cgs.fontSystemBigSize = ceilf( SYSTEM_FONT_BIG_SIZE * scale );

		scale *= 1.3f;
		cgs.textSizeTiny = SYSTEM_FONT_TINY_SIZE * scale;
		cgs.textSizeExtraSmall = SYSTEM_FONT_EXTRASMALL_SIZE * scale;
		cgs.textSizeSmall = SYSTEM_FONT_SMALL_SIZE * scale;
		cgs.textSizeMedium = SYSTEM_FONT_MEDIUM_SIZE * scale;
		cgs.textSizeBig = SYSTEM_FONT_BIG_SIZE * scale;
	}

	AllocateDecalBuffers();

	MaybeResetShadertoyTime( false );

	CG_UpdateChaseCam( &cmd );

	ViewType view_type = CG_DemoCamUpdate();
	CG_SetupViewDef( &cg.view, view_type, &cmd );

	RendererSetView( cg.view.origin, cg.view.angles, cg.view.fov_y );

	CG_LerpEntities();

	CG_CalcViewWeapon( &cg.weapon );

	CG_FireEvents( false );

	CG_ResetBombHUD();

	DoVisualEffect( "vfx/rain", cg.view.origin );

	DrawEntities();
	DrawOutlines();
	DrawSilhouettes();
	CG_AddViewWeapon( &cg.weapon );
	DrawGibs();
	DrawParticles();
	DrawPersistentBeams();
	DrawPersistentDecals();
	DrawPersistentDynamicLights();
	DrawTrails();
	DrawSkybox( cls.shadertoy_time );
	DrawSprays();

	DrawModelInstances();

	CG_ReleaseAnnouncerEvents();

	SoundFrame( cg.view.origin, cg.view.velocity, FromQFAxis( cg.view.axis, AXIS_FORWARD ), FromQFAxis( cg.view.axis, AXIS_UP ) );

	CG_Draw2D();

	UploadDecalBuffers();
}

void MaybeResetShadertoyTime( bool respawned ) {
	bool early_reset = respawned && cls.shadertoy_time > Hours( 1 );
	bool force_reset = cls.shadertoy_time > Hours( 1.5f );
	if( early_reset || force_reset ) {
		cls.shadertoy_time = { };
	}
}
