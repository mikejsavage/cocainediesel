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

#include "cg_local.h"
#include "client/client.h"

static void CG_UpdateEntities( void );

/*
* CG_FixVolumeCvars
* Don't let the user go too far away with volumes
*/
static void CG_FixVolumeCvars( void ) {
	if( developer->integer ) {
		return;
	}

	if( cg_volume_players->value < 0.0f ) {
		trap_Cvar_SetValue( "cg_volume_players", 0.0f );
	} else if( cg_volume_players->value > 2.0f ) {
		trap_Cvar_SetValue( "cg_volume_players", 2.0f );
	}

	if( cg_volume_effects->value < 0.0f ) {
		trap_Cvar_SetValue( "cg_volume_effects", 0.0f );
	} else if( cg_volume_effects->value > 2.0f ) {
		trap_Cvar_SetValue( "cg_volume_effects", 2.0f );
	}

	if( cg_volume_announcer->value < 0.0f ) {
		trap_Cvar_SetValue( "cg_volume_announcer", 0.0f );
	} else if( cg_volume_announcer->value > 2.0f ) {
		trap_Cvar_SetValue( "cg_volume_announcer", 2.0f );
	}

	if( cg_volume_voicechats->value < 0.0f ) {
		trap_Cvar_SetValue( "cg_volume_voicechats", 0.0f );
	} else if( cg_volume_voicechats->value > 2.0f ) {
		trap_Cvar_SetValue( "cg_volume_voicechats", 2.0f );
	}

	if( cg_volume_hitsound->value < 0.0f ) {
		trap_Cvar_SetValue( "cg_volume_hitsound", 0.0f );
	} else if( cg_volume_hitsound->value > 10.0f ) {
		trap_Cvar_SetValue( "cg_volume_hitsound", 10.0f );
	}
}

static bool CG_UpdateLinearProjectilePosition( centity_t *cent ) {
	vec3_t origin;
	entity_state_t *state;
	int moveTime;
	int64_t serverTime;
#define MIN_DRAWDISTANCE_FIRSTPERSON 86
#define MIN_DRAWDISTANCE_THIRDPERSON 52

	state = &cent->current;

	if( !state->linearMovement ) {
		return false;
	}

	if( GS_MatchPaused() ) {
		serverTime = cg.frame.serverTime;
	} else {
		serverTime = cg.time + cgs.extrapolationTime;
	}

	if( state->solid != SOLID_BMODEL ) {
		// add a time offset to counter antilag visualization
		if( !cgs.demoPlaying && cg_projectileAntilagOffset->value > 0.0f &&
			!ISVIEWERENTITY( state->ownerNum ) && ( cgs.playerNum + 1 != cg.predictedPlayerState.POVnum ) ) {
			serverTime += state->modelindex2 * cg_projectileAntilagOffset->value;
		}
	}

	moveTime = GS_LinearMovement( state, serverTime, origin );
	VectorCopy( origin, state->origin );

	if( ( moveTime < 0 ) && ( state->solid != SOLID_BMODEL ) ) {
		// when flyTime is negative don't offset it backwards more than PROJECTILE_PRESTEP value
		// FIXME: is this still valid?
		float maxBackOffset;

		if( ISVIEWERENTITY( state->ownerNum ) ) {
			maxBackOffset = ( PROJECTILE_PRESTEP - MIN_DRAWDISTANCE_FIRSTPERSON );
		} else {
			maxBackOffset = ( PROJECTILE_PRESTEP - MIN_DRAWDISTANCE_THIRDPERSON );
		}

		if( DistanceFast( state->origin2, state->origin ) > maxBackOffset ) {
			return false;
		}
	}

	return true;
#undef MIN_DRAWDISTANCE_FIRSTPERSON
#undef MIN_DRAWDISTANCE_THIRDPERSON
}

/*
* CG_NewPacketEntityState
*/
static void CG_NewPacketEntityState( entity_state_t *state ) {
	centity_t *cent;

	cent = &cg_entities[state->number];

	VectorClear( cent->prevVelocity );
	cent->canExtrapolatePrev = false;

	if( ISEVENTENTITY( state ) ) {
		cent->prev = cent->current;
		cent->current = *state;
		cent->serverFrame = cg.frame.serverFrame;

		VectorClear( cent->velocity );
		cent->canExtrapolate = false;
	} else if( state->linearMovement ) {
		if( cent->serverFrame != cg.oldFrame.serverFrame || state->teleported ||
			state->linearMovement != cent->current.linearMovement || state->linearMovementTimeStamp != cent->current.linearMovementTimeStamp ) {
			cent->prev = *state;
		} else {
			cent->prev = cent->current;
		}

		cent->current = *state;
		cent->serverFrame = cg.frame.serverFrame;

		VectorClear( cent->velocity );
		cent->canExtrapolate = false;

		cent->linearProjectileCanDraw = CG_UpdateLinearProjectilePosition( cent );

		VectorCopy( cent->current.linearMovementVelocity, cent->velocity );
		VectorCopy( cent->current.origin, cent->trailOrigin );
	} else {
		// if it moved too much force the teleported bit
		if(  abs( (int)( cent->current.origin[0] - state->origin[0] ) ) > 512
			 || abs( (int)( cent->current.origin[1] - state->origin[1] ) ) > 512
			 || abs( (int)( cent->current.origin[2] - state->origin[2] ) ) > 512 ) {
			cent->serverFrame = -99;
		}

		// some data changes will force no lerping
		if( state->modelindex != cent->current.modelindex
			|| state->teleported
			|| state->linearMovement != cent->current.linearMovement ) {
			cent->serverFrame = -99;
		}

		if( cent->serverFrame != cg.oldFrame.serverFrame ) {
			// wasn't in last update, so initialize some things
			// duplicate the current state so lerping doesn't hurt anything
			cent->prev = *state;

			memset( cent->localEffects, 0, sizeof( cent->localEffects ) );

			// Init the animation when new into PVS
			if( cg.frame.valid && ( state->type == ET_PLAYER || state->type == ET_CORPSE ) ) {
				memset( cent->lastVelocities, 0, sizeof( cent->lastVelocities ) );
				memset( cent->lastVelocitiesFrames, 0, sizeof( cent->lastVelocitiesFrames ) );
				CG_PModel_ClearEventAnimations( state->number );
				memset( &cg_entPModels[state->number].animState, 0, sizeof( cg_entPModels[state->number].animState ) );

				// reset the weapon animation timers for new players
				cg_entPModels[state->number].flash_time = 0;
				cg_entPModels[state->number].barrel_time = 0;
			}
		} else {   // shuffle the last state to previous
			cent->prev = cent->current;
		}

		if( cent->serverFrame != cg.oldFrame.serverFrame ) {
			cent->microSmooth = 0;
		}

		cent->current = *state;
		VectorCopy( state->origin, cent->trailOrigin );
		VectorCopy( cent->velocity, cent->prevVelocity );

		//VectorCopy( cent->extrapolatedOrigin, cent->prevExtrapolatedOrigin );
		cent->canExtrapolatePrev = cent->canExtrapolate;
		cent->canExtrapolate = false;
		VectorClear( cent->velocity );
		cent->serverFrame = cg.frame.serverFrame;

		// set up velocities for this entity
		if( cgs.extrapolationTime &&
			( cent->current.type == ET_PLAYER || cent->current.type == ET_CORPSE ) ) {
			VectorCopy( cent->current.origin2, cent->velocity );
			VectorCopy( cent->prev.origin2, cent->prevVelocity );
			cent->canExtrapolate = cent->canExtrapolatePrev = true;
		} else if( !VectorCompare( cent->prev.origin, cent->current.origin ) ) {
			float snapTime = ( cg.frame.serverTime - cg.oldFrame.serverTime );

			if( !snapTime ) {
				snapTime = cgs.snapFrameTime;
			}

			VectorSubtract( cent->current.origin, cent->prev.origin, cent->velocity );
			VectorScale( cent->velocity, 1000.0f / snapTime, cent->velocity );
		}

		if( ( cent->current.type == ET_GENERIC || cent->current.type == ET_PLAYER
			  || cent->current.type == ET_GIB || cent->current.type == ET_GRENADE
			  || cent->current.type == ET_CORPSE ) ) {
			cent->canExtrapolate = true;
		}

		if( ISBRUSHMODEL( cent->current.modelindex ) ) { // disable extrapolation on movers
			cent->canExtrapolate = false;
		}
	}
}

int CG_LostMultiviewPOV( void ) {
	int best, value, fallback;
	int i, index;

	best = gs.maxclients;
	index = fallback = -1;

	for( i = 0; i < cg.frame.numplayers; i++ ) {
		value = abs( (int)cg.frame.playerStates[i].playerNum - (int)cg.multiviewPlayerNum );
		if( value == best && i > index ) {
			continue;
		}

		if( value < best ) {
			if( cg.frame.playerStates[i].pmove.pm_type == PM_SPECTATOR ) {
				fallback = i;
				continue;
			}

			best = value;
			index = i;
		}
	}

	return index > -1 ? index : fallback;
}

static void CG_SetFramePlayerState( snapshot_t *frame, int index ) {
	frame->playerState = frame->playerStates[index];
	if( cgs.demoPlaying || cg.frame.multipov ) {
		frame->playerState.pmove.pm_flags |= PMF_NO_PREDICTION;
		if( frame->playerState.pmove.pm_type != PM_SPECTATOR ) {
			frame->playerState.pmove.pm_type = PM_CHASECAM;
		}
	}
}

static void CG_UpdatePlayerState( void ) {
	int i;
	int index = 0;

	if( cg.frame.multipov ) {
		// find the playerState containing our current POV, then cycle playerStates
		index = -1;
		for( i = 0; i < cg.frame.numplayers; i++ ) {
			if( cg.frame.playerStates[i].playerNum < (unsigned)gs.maxclients
				&& cg.frame.playerStates[i].playerNum == cg.multiviewPlayerNum ) {
				index = i;
				break;
			}
		}

		// the POV was lost, find the closer one (may go up or down, but who cares)
		if( index < 0 || cg.frame.playerStates[index].pmove.pm_type == PM_SPECTATOR ) {
			index = CG_LostMultiviewPOV();
		}
		if( index < 0 ) {
			index = 0;
		}
	}

	cg.multiviewPlayerNum = cg.frame.playerStates[index].playerNum;

	// set up the playerstates

	// current
	CG_SetFramePlayerState( &cg.frame, index );

	// old
	index = -1;
	for( i = 0; i < cg.oldFrame.numplayers; i++ ) {
		if( cg.oldFrame.playerStates[i].playerNum == cg.multiviewPlayerNum ) {
			index = i;
			break;
		}
	}

	// use the current one for old frame too, if correct POV wasn't found
	if( index == -1 ) {
		cg.oldFrame.playerState = cg.frame.playerState;
	} else {
		CG_SetFramePlayerState( &cg.oldFrame, index );
	}

	cg.predictedPlayerState = cg.frame.playerState;
}

/*
* CG_NewFrameSnap
* a new frame snap has been received from the server
*/
bool CG_NewFrameSnap( snapshot_t *frame, snapshot_t *lerpframe ) {
	int i;

	assert( frame );

	if( lerpframe ) {
		cg.oldFrame = *lerpframe;
	} else {
		cg.oldFrame = *frame;
	}

	cg.frame = *frame;
	gs.gameState = frame->gameState;

	if( cg_projectileAntilagOffset->value > 1.0f || cg_projectileAntilagOffset->value < 0.0f ) {
		trap_Cvar_ForceSet( "cg_projectileAntilagOffset", cg_projectileAntilagOffset->dvalue );
	}

	CG_UpdatePlayerState();

	for( i = 0; i < frame->numEntities; i++ )
		CG_NewPacketEntityState( &frame->parsedEntities[i & ( MAX_PARSE_ENTITIES - 1 )] );

	if( lerpframe && ( memcmp( cg.oldFrame.areabits, cg.frame.areabits, cg.frame.areabytes ) == 0 ) ) {
		cg.oldAreabits = true;
	} else {
		cg.oldAreabits = false;
	}

	if( !cgs.precacheDone || !cg.frame.valid ) {
		return false;
	}

	// a new server frame begins now
	CG_FixVolumeCvars();

	CG_BuildSolidList();
	CG_UpdateEntities();
	CG_CheckPredictionError();

	cg.predictFrom = 0; // force the prediction to be restarted from the new snapshot
	cg.fireEvents = true;

	for( i = 0; i < cg.frame.numgamecommands; i++ ) {
		int target = cg.frame.playerState.POVnum - 1;
		if( cg.frame.gamecommands[i].all || cg.frame.gamecommands[i].targets[target >> 3] & ( 1 << ( target & 7 ) ) ) {
			CG_GameCommand( cg.frame.gamecommandsData + cg.frame.gamecommands[i].commandOffset );
		}
	}

	CG_FireEvents( true );

	cg.firstFrame = false; // not the first frame anymore
	return true;
}


//=============================================================


/*
==========================================================================

ADD INTERPOLATED ENTITIES TO RENDERING LIST

==========================================================================
*/

/*
* CG_CModelForEntity
*  get the collision model for the given entity, no matter if box or brush-model.
*/
struct cmodel_s *CG_CModelForEntity( int entNum ) {
	int x, zd, zu;
	centity_t *cent;
	struct cmodel_s *cmodel = NULL;
	vec3_t bmins, bmaxs;

	if( entNum < 0 || entNum >= MAX_EDICTS ) {
		return NULL;
	}

	cent = &cg_entities[entNum];

	if( cent->serverFrame != cg.frame.serverFrame ) { // not present in current frame
		return NULL;
	}

	// find the cmodel
	if( cent->current.solid == SOLID_BMODEL ) { // special value for bmodel
		cmodel = trap_CM_InlineModel( cent->current.modelindex );
	} else if( cent->current.solid ) {   // encoded bbox
		x = 8 * ( cent->current.solid & 31 );
		zd = 8 * ( ( cent->current.solid >> 5 ) & 31 );
		zu = 8 * ( ( cent->current.solid >> 10 ) & 63 ) - 32;

		bmins[0] = bmins[1] = -x;
		bmaxs[0] = bmaxs[1] = x;
		bmins[2] = -zd;
		bmaxs[2] = zu;
		if( cent->type == ET_PLAYER || cent->type == ET_CORPSE ) {
			cmodel = trap_CM_OctagonModelForBBox( bmins, bmaxs );
		} else {
			cmodel = trap_CM_ModelForBBox( bmins, bmaxs );
		}
	}

	return cmodel;
}

/*
* CG_EntAddBobEffect
*/
static void CG_EntAddBobEffect( centity_t *cent ) {
	float scale = 0.005f + cent->current.number * 0.00001f;
	float bob = 4 + cosf( ( cg.time + 1000 ) * scale ) * 4;

	cent->ent.origin2[2] += bob;
	cent->ent.origin[2] += bob;
}

/*
* CG_EntAddTeamColorTransitionEffect
*/
static void CG_EntAddTeamColorTransitionEffect( centity_t *cent ) {
	byte_vec4_t currentcolor;
	vec4_t scaledcolor, newcolor;
	const vec4_t neutralcolor = { 1.0f, 1.0f, 1.0f, 1.0f };

	float f = Clamp01( (float)cent->current.counterNum / 255.0f );

	CG_TeamColorForEntity( cent->current.number, currentcolor );

	Vector4Scale( currentcolor, 1.0 / 255.0, scaledcolor );
	VectorLerp( neutralcolor, f, scaledcolor, newcolor );

	cent->ent.shaderRGBA[0] = uint8_t( newcolor[0] * 255 );
	cent->ent.shaderRGBA[1] = uint8_t( newcolor[1] * 255 );
	cent->ent.shaderRGBA[2] = uint8_t( newcolor[2] * 255 );
	cent->ent.shaderRGBA[3] = 255;
}

//==========================================================================
//		ET_GENERIC
//==========================================================================

/*
* CG_UpdateGenericEnt
*/
static void CG_UpdateGenericEnt( centity_t *cent ) {
	// start from clean
	memset( &cent->ent, 0, sizeof( cent->ent ) );
	cent->ent.scale = 1.0f;

	// set entity color based on team
	CG_TeamColorForEntity( cent->current.number, cent->ent.shaderRGBA );
	Vector4Set( cent->outlineColor, 0, 0, 0, 255 );

	// set up the model
	int modelindex = cent->current.modelindex;
	if( modelindex > 0 && modelindex < MAX_MODELS ) {
		cent->ent.model = cgs.modelDraw[modelindex];
	}
}

/*
* CG_ExtrapolateLinearProjectile
*/
void CG_ExtrapolateLinearProjectile( centity_t *cent ) {
	cent->linearProjectileCanDraw = CG_UpdateLinearProjectilePosition( cent );

	for( int i = 0; i < 3; i++ )
		cent->ent.origin[i] = cent->ent.origin2[i] = cent->current.origin[i];

	AnglesToAxis( cent->current.angles, cent->ent.axis );
}

/*
* CG_LerpGenericEnt
*/
void CG_LerpGenericEnt( centity_t *cent ) {
	int i;
	vec3_t ent_angles = { 0, 0, 0 };

	if( ISVIEWERENTITY( cent->current.number ) || cg.view.POVent == cent->current.number ) {
		VectorCopy( cg.predictedPlayerState.viewangles, ent_angles );
	} else {
		// interpolate angles
		for( i = 0; i < 3; i++ )
			ent_angles[i] = LerpAngle( cent->prev.angles[i], cent->current.angles[i], cg.lerpfrac );
	}

	if( ent_angles[0] || ent_angles[1] || ent_angles[2] ) {
		AnglesToAxis( ent_angles, cent->ent.axis );
	} else {
		Matrix3_Copy( axis_identity, cent->ent.axis );
	}

	if( ISVIEWERENTITY( cent->current.number ) || cg.view.POVent == cent->current.number ) {
		VectorCopy( cg.predictedPlayerState.pmove.origin, cent->ent.origin );
		VectorCopy( cent->ent.origin, cent->ent.origin2 );
	} else {
		if( cgs.extrapolationTime && cent->canExtrapolate ) { // extrapolation
			vec3_t origin, xorigin1, xorigin2;

			float lerpfrac = Clamp01( cg.lerpfrac );

			// extrapolation with half-snapshot smoothing
			if( cg.xerpTime >= 0 || !cent->canExtrapolatePrev ) {
				VectorMA( cent->current.origin, cg.xerpTime, cent->velocity, xorigin1 );
			} else {
				VectorMA( cent->current.origin, cg.xerpTime, cent->velocity, xorigin1 );
				if( cent->canExtrapolatePrev ) {
					vec3_t oldPosition;

					VectorMA( cent->prev.origin, cg.oldXerpTime, cent->prevVelocity, oldPosition );
					VectorLerp( oldPosition, cg.xerpSmoothFrac, xorigin1, xorigin1 );
				}
			}


			// extrapolation with full-snapshot smoothing
			VectorMA( cent->current.origin, cg.xerpTime, cent->velocity, xorigin2 );
			if( cent->canExtrapolatePrev ) {
				vec3_t oldPosition;

				VectorMA( cent->prev.origin, cg.oldXerpTime, cent->prevVelocity, oldPosition );
				VectorLerp( oldPosition, lerpfrac, xorigin2, xorigin2 );
			}

			VectorLerp( xorigin1, 0.5f, xorigin2, origin );

			if( cent->microSmooth == 2 ) {
				vec3_t oldsmoothorigin;

				VectorLerp( cent->microSmoothOrigin2, 0.65f, cent->microSmoothOrigin, oldsmoothorigin );
				VectorLerp( origin, 0.5f, oldsmoothorigin, cent->ent.origin );
			} else if( cent->microSmooth == 1 ) {
				VectorLerp( origin, 0.5f, cent->microSmoothOrigin, cent->ent.origin );
			} else {
				VectorCopy( origin, cent->ent.origin );
			}

			if( cent->microSmooth ) {
				VectorCopy( cent->microSmoothOrigin, cent->microSmoothOrigin2 );
			}

			VectorCopy( origin, cent->microSmoothOrigin );
			cent->microSmooth = Min2( 2, cent->microSmooth + 1 );

			VectorCopy( cent->ent.origin, cent->ent.origin2 );
		} else {   // plain interpolation
			for( i = 0; i < 3; i++ )
				cent->ent.origin[i] = cent->ent.origin2[i] = cent->prev.origin[i] + cg.lerpfrac *
															 ( cent->current.origin[i] - cent->prev.origin[i] );
		}
	}
}

/*
* CG_AddGenericEnt
*/
static void CG_AddGenericEnt( centity_t *cent ) {
	if( !cent->ent.scale ) {
		return;
	}

	if( cent->ent.model == NULL ) {
		return;
	}

	// bobbing & auto-rotation
	if( cent->effects & EF_ROTATE_AND_BOB ) {
		CG_EntAddBobEffect( cent );
		Matrix3_Copy( cg.autorotateAxis, cent->ent.axis );
	}

	if( cent->effects & EF_TEAMCOLOR_TRANSITION ) {
		CG_EntAddTeamColorTransitionEffect( cent );
	}

	const Model * model = cent->ent.model;
	Mat4 transform = FromQFAxisAndOrigin( cent->ent.axis, cent->ent.origin );

	Vec4 color;
	for( int i = 0; i < 4; i++ )
		color.ptr()[ i ] = cent->ent.shaderRGBA[ i ] / 255.0f;

	DrawModel( model, transform, color );

	if( cent->effects & EF_WORLD_MODEL ) {
		UniformBlock model_uniforms = UploadModelUniforms( transform * model->transform );
		for( u32 i = 0; i < model->num_primitives; i++ ) {
			if( model->primitives[ i ].material->blend_func == BlendFunc_Disabled ) {
				PipelineState pipeline;
				pipeline.pass = frame_static.world_write_gbuffer_pass;
				pipeline.shader = &shaders.world_write_gbuffer;
				pipeline.set_uniform( "u_View", frame_static.view_uniforms );
				pipeline.set_uniform( "u_Model", model_uniforms );

				DrawModelPrimitive( model, &model->primitives[ i ], pipeline );
			}
		}
	}
}

//==========================================================================
//		ET_PLAYER
//==========================================================================

/*
* CG_AddPlayerEnt
*/
static void CG_AddPlayerEnt( centity_t *cent ) {
	if( ISVIEWERENTITY( cent->current.number ) ) {
		cg.effects = cent->effects;
		if( !cg.view.thirdperson && cent->current.modelindex ) {
			CG_AllocPlayerShadow( cent->current.number, cent->ent.origin, playerbox_stand_mins, playerbox_stand_maxs );
			return;
		}
	}

	// if set to invisible, skip
	if( !cent->current.modelindex || cent->current.team == TEAM_SPECTATOR ) {
		return;
	}

	CG_DrawPlayer( cent );
}

//==========================================================================
//		ET_DECAL
//==========================================================================

/*
* CG_AddDecalEnt
*/
static void CG_AddDecalEnt( centity_t *cent ) {
	// if set to invisible, skip
	if( !cent->current.modelindex ) {
		return;
	}

	if( cent->effects & EF_TEAMCOLOR_TRANSITION ) {
		CG_EntAddTeamColorTransitionEffect( cent );
	}

	CG_AddFragmentedDecal( cent->ent.origin, cent->ent.origin2,
						   cent->ent.rotation, cent->ent.radius,
						   cent->ent.shaderRGBA[0] * ( 1.0 / 255.0 ), cent->ent.shaderRGBA[1] * ( 1.0 / 255.0 ), cent->ent.shaderRGBA[2] * ( 1.0 / 255.0 ),
						   cent->ent.shaderRGBA[3] * ( 1.0 / 255.0 ), cent->ent.override_material );
}

/*
* CG_LerpDecalEnt
*/
static void CG_LerpDecalEnt( centity_t *cent ) {
	int i;
	float a1, a2;

	// interpolate origin
	for( i = 0; i < 3; i++ )
		cent->ent.origin[i] = cent->prev.origin[i] + cg.lerpfrac * ( cent->current.origin[i] - cent->prev.origin[i] );

	cent->ent.radius = Lerp( cent->prev.radius, cg.lerpfrac, cent->current.radius );

	a1 = cent->prev.modelindex2 / 255.0 * 360;
	a2 = cent->current.modelindex2 / 255.0 * 360;
	cent->ent.rotation = LerpAngle( a1, a2, cg.lerpfrac );
}

/*
* CG_UpdateDecalEnt
*/
static void CG_UpdateDecalEnt( centity_t *cent ) {
	// set entity color based on team
	CG_TeamColorForEntity( cent->current.number, cent->ent.shaderRGBA );

	// set up the null model, may be potentially needed for linked model
	cent->ent.model = NULL;
	cent->ent.override_material = cgs.imagePrecache[ cent->current.modelindex ];
	cent->ent.radius = cent->prev.radius;
	cent->ent.rotation = cent->prev.modelindex2 / 255.0 * 360;
	VectorCopy( cent->prev.origin, cent->ent.origin );
	VectorCopy( cent->prev.origin2, cent->ent.origin2 );
}

//==========================================================================
// ET_LASER
//==========================================================================

static void CG_LerpLaser( centity_t *cent ) {
	for( int i = 0; i < 3; i++ ) {
		cent->ent.origin[i] = Lerp( cent->prev.origin[i], cg.lerpfrac, cent->current.origin[i] );
		cent->ent.origin2[i] = Lerp( cent->prev.origin2[i], cg.lerpfrac, cent->current.origin2[i] );
	}
}

static void CG_AddLaserEnt( centity_t *cent ) {
	DrawBeam( FromQF3( cent->ent.origin ), FromQF3( cent->ent.origin2 ), cent->current.radius, vec4_white, cgs.media.shaderLaser );
}

//==========================================================================
//		ET_LASERBEAM
//==========================================================================

/*
* CG_UpdateLaserbeamEnt
*/
static void CG_UpdateLaserbeamEnt( centity_t *cent ) {
	centity_t *owner;

	if( cg.view.playerPrediction && ISVIEWERENTITY( cent->current.ownerNum ) ) {
		return;
	}

	owner = &cg_entities[cent->current.ownerNum];
	if( owner->serverFrame != cg.frame.serverFrame ) {
		CG_Error( "CG_UpdateLaserbeamEnt: owner is not in the snapshot\n" );
	}

	owner->localEffects[LOCALEFFECT_LASERBEAM] = cg.time + 10;

	// laser->s.origin is beam start
	// laser->s.origin2 is beam end

	VectorCopy( cent->prev.origin, owner->laserOriginOld );
	VectorCopy( cent->prev.origin2, owner->laserPointOld );

	VectorCopy( cent->current.origin, owner->laserOrigin );
	VectorCopy( cent->current.origin2, owner->laserPoint );
}

/*
* CG_LerpLaserbeamEnt
*/
static void CG_LerpLaserbeamEnt( centity_t *cent ) {
	centity_t *owner = &cg_entities[cent->current.ownerNum];

	if( cg.view.playerPrediction && ISVIEWERENTITY( cent->current.ownerNum ) ) {
		return;
	}

	owner->localEffects[LOCALEFFECT_LASERBEAM] = cg.time + 1;
}

//==================================================
// ET_PARTICLES
//==================================================

static void CG_AddParticlesEnt( centity_t *cent ) {
	// origin = origin
	// angles = angles
	// sound = sound
	// light = light color
	// frame = speed
	// team = RGBA
	// modelindex = shader
	// modelindex2 = radius (spread)
	// effects & 0xFF = size
	// skinNum/counterNum = time (fade in seconds);
	// effects = spherical, bounce, gravity,
	// weapon = frequency

	vec3_t dir;
	float speed;
	int spriteTime;
	int spriteRadius;
	int mintime;
	vec3_t accel;
	int bounce = 0;
	bool expandEffect = false;
	bool shrinkEffect = false;
	vec3_t angles;
	int i;

	// duration of each particle
	spriteTime = cent->current.counterNum;
	if( !spriteTime ) {
		return;
	}

	spriteRadius = cent->current.effects & 0xFF;
	if( !spriteRadius ) {
		return;
	}

	if( !cent->current.weapon ) { // weapon is count per second
		return;
	}

	mintime = 1000 / cent->current.weapon;

	if( cent->localEffects[LOCALEFFECT_ROCKETTRAIL_LAST_DROP] + mintime > cg.time ) { // just reusing a define
		return;
	}

	cent->localEffects[LOCALEFFECT_ROCKETTRAIL_LAST_DROP] = cg.time;

	speed = cent->current.radius;

	if( ( cent->current.effects >> 8 ) & 1 ) { // SPHERICAL DROP
		angles[0] = brandom( 0, 360 );
		angles[1] = brandom( 0, 360 );
		angles[2] = brandom( 0, 360 );

		AngleVectors( angles, dir, NULL, NULL );
		VectorNormalizeFast( dir );
		VectorScale( dir, speed, dir );
	} else {   // DIRECTIONAL DROP
		int spread = (unsigned)cent->current.modelindex2 * 25;

		// interpolate dropping angles
		for( i = 0; i < 3; i++ )
			angles[i] = LerpAngle( cent->prev.angles[i], cent->current.angles[i], cg.lerpfrac );

		Matrix3_FromAngles( angles, cent->ent.axis );

		float alpha = float( M_PI ) * random_float11( &cls.rng );
		float s = random_float01( &cls.rng );
		float r = s * cosf( alpha ) * spread;
		float u = s * sinf( alpha ) * spread;

		// apply spread on the direction
		VectorMA( vec3_origin, 1024, &cent->ent.axis[AXIS_FORWARD], dir );
		VectorMA( dir, r, &cent->ent.axis[AXIS_RIGHT], dir );
		VectorMA( dir, u, &cent->ent.axis[AXIS_UP], dir );

		VectorNormalizeFast( dir );
		VectorScale( dir, speed, dir );
	}

	// interpolate origin
	for( i = 0; i < 3; i++ )
		cent->ent.origin[i] = cent->ent.origin2[i] = cent->prev.origin[i] + cg.lerpfrac * ( cent->current.origin[i] - cent->prev.origin[i] );

	if( ( cent->current.effects >> 9 ) & 1 ) { // BOUNCES ON WALLS/FLOORS
		bounce = 35;
	}

	VectorClear( accel );
	if( ( cent->current.effects >> 10 ) & 1 ) { // GRAVITY
		VectorSet( accel, -0.2f, -0.2f, -175.0f );
	}

	if( ( cent->current.effects >> 11 ) & 1 ) { // EXPAND_EFFECT
		expandEffect = true;
	}

	if( ( cent->current.effects >> 12 ) & 1 ) { // SHRINK_EFFECT
		shrinkEffect = true;
	}

	CG_SpawnSprite( cent->ent.origin, dir, accel,
					spriteRadius, spriteTime, bounce, expandEffect, shrinkEffect,
					cent->ent.shaderRGBA[0] / 255.0f,
					cent->ent.shaderRGBA[1] / 255.0f,
					cent->ent.shaderRGBA[2] / 255.0f,
					cent->ent.shaderRGBA[3] / 255.0f,
					cent->current.light ? spriteRadius * 4 : 0, // light radius
					COLOR_R( cent->current.light ) / 255.0f,
					COLOR_G( cent->current.light ) / 255.0f,
					COLOR_B( cent->current.light ) / 255.0f,
					cent->ent.override_material );
}

void CG_UpdateParticlesEnt( centity_t *cent ) {
	// set entity color based on team
	CG_TeamColorForEntity( cent->current.number, cent->ent.shaderRGBA );

	// set up the data in the old position
	cent->ent.model = NULL;
	cent->ent.override_material = cgs.imagePrecache[ cent->current.modelindex ];
	VectorCopy( cent->prev.origin, cent->ent.origin );
	VectorCopy( cent->prev.origin2, cent->ent.origin2 );
}

//==================================================
// ET_SOUNDEVENT
//==================================================

void CG_SoundEntityNewState( centity_t *cent ) {
	int channel, soundindex, owner;
	float attenuation;
	bool fixed;

	soundindex = cent->current.sound;
	owner = cent->current.ownerNum;
	channel = cent->current.channel & ~CHAN_FIXED;
	fixed = ( cent->current.channel & CHAN_FIXED ) ? true : false;
	attenuation = cent->current.attenuation;

	if( attenuation == ATTN_NONE ) {
		if( cgs.soundPrecache[soundindex] ) {
			S_StartGlobalSound( cgs.soundPrecache[soundindex], channel & ~CHAN_FIXED, 1.0f );
		}
		return;
	}

	if( owner ) {
		if( owner < 0 || owner >= MAX_EDICTS ) {
			CG_Printf( "CG_SoundEntityNewState: bad owner number" );
			return;
		}
		if( cg_entities[owner].serverFrame != cg.frame.serverFrame ) {
			owner = 0;
		}
	}

	if( !owner ) {
		fixed = true;
	}

	if( fixed ) {
		S_StartFixedSound( cgs.soundPrecache[soundindex], cent->current.origin, channel, 1.0f, attenuation );
	} else if( ISVIEWERENTITY( owner ) ) {
		S_StartGlobalSound( cgs.soundPrecache[soundindex], channel, 1.0f );
	} else {
		S_StartEntitySound( cgs.soundPrecache[soundindex], owner, channel, 1.0f, attenuation );
	}
}

//==================================================
// ET_SPIKES
//==================================================

static void CG_LerpSpikes( centity_t *cent ) {
	constexpr float retracted = -48;
	constexpr float primed = -36;
	constexpr float extended = 0;

	float position = retracted;

	if( cent->current.radius == 1 ) {
		position = extended;
	}
	else if( cent->current.linearMovementTimeStamp != 0 ) {
		int64_t delta = Lerp( cg.oldFrame.serverTime, cg.lerpfrac, cg.frame.serverTime ) - cent->current.linearMovementTimeStamp;
		if( delta > 0 ) {
			// 0-100: jump to primed
			// 1000-1050: fully extend
			// 1500-2000: retract
			if( delta < 1000 ) {
				float t = Min2( 1.0f, Unlerp( int64_t( 0 ), delta, int64_t( 100 ) ) );
				position = Lerp( retracted, t, primed );
			}
			else if( delta < 1050 ) {
				float t = Min2( 1.0f, Unlerp( int64_t( 1000 ), delta, int64_t( 1050 ) ) );
				position = Lerp( primed, t, extended );
			}
			else {
				float t = Max2( 0.0f, Unlerp( int64_t( 1500 ), delta, int64_t( 2000 ) ) );
				position = Lerp( extended, t, retracted );
			}
		}
	}

	vec3_t up;
	AngleVectors( cent->current.angles, NULL, NULL, up );

	AnglesToAxis( cent->current.angles, cent->ent.axis );
	VectorMA( cent->current.origin, position, up, cent->ent.origin );
	VectorCopy( cent->ent.origin, cent->ent.origin2 );
}

static void CG_UpdateSpikes( centity_t *cent ) {
	CG_UpdateGenericEnt( cent );

	if( cent->current.linearMovementTimeStamp == 0 )
		return;

	int64_t old_delta = cg.oldFrame.serverTime - cent->current.linearMovementTimeStamp;
	int64_t delta = cg.frame.serverTime - cent->current.linearMovementTimeStamp;

	if( old_delta < 0 && delta >= 0 ) {
		S_StartEntitySound( cgs.media.sfxSpikesArm, cent->current.number, CHAN_AUTO, cg_volume_effects->value, ATTN_NORM );
	}
	else if( old_delta < 1000 && delta >= 1000 ) {
		S_StartEntitySound( cgs.media.sfxSpikesDeploy, cent->current.number, CHAN_AUTO, cg_volume_effects->value, ATTN_NORM );
	}
	else if( old_delta < 1050 && delta >= 1050 ) {
		S_StartEntitySound( cgs.media.sfxSpikesGlint, cent->current.number, CHAN_AUTO, cg_volume_effects->value * 0.05f, ATTN_NORM );
	}
	else if( old_delta < 1500 && delta >= 1500 ) {
		S_StartEntitySound( cgs.media.sfxSpikesRetract, cent->current.number, CHAN_AUTO, cg_volume_effects->value, ATTN_NORM );
	}
}

//==========================================================================
//		PACKET ENTITIES
//==========================================================================

void CG_EntityLoopSound( entity_state_t *state, float attenuation ) {
	if( !state->sound ) {
		return;
	}

	S_ImmediateSound( cgs.soundPrecache[state->sound], state->number, cg_volume_effects->value, ISVIEWERENTITY( state->number ) ? ATTN_NONE : ATTN_IDLE );
}

/*
* CG_AddPacketEntitiesToScene
* Add the entities to the rendering list
*/
void CG_AddEntities( void ) {
	ZoneScoped;

	entity_state_t *state;
	vec3_t autorotate;
	int pnum;
	centity_t *cent;
	bool canLight;

	// bonus items rotate at a fixed rate
	VectorSet( autorotate, 0, ( cg.time % 3600 ) * 0.1, 0 );
	AnglesToAxis( autorotate, cg.autorotateAxis );

	for( pnum = 0; pnum < cg.frame.numEntities; pnum++ ) {
		state = &cg.frame.parsedEntities[pnum & ( MAX_PARSE_ENTITIES - 1 )];
		cent = &cg_entities[state->number];

		if( cent->current.linearMovement ) {
			if( !cent->linearProjectileCanDraw ) {
				continue;
			}
		}

		canLight = !state->linearMovement;

		switch( cent->type ) {
			case ET_GENERIC:
				CG_AddGenericEnt( cent );
				CG_EntityLoopSound( state, ATTN_STATIC );
				canLight = true;
				break;
			case ET_GIB:
				CG_AddGenericEnt( cent );
				CG_EntityLoopSound( state, ATTN_STATIC );
				canLight = true;
				break;

			case ET_ROCKET:
				CG_AddGenericEnt( cent );
				CG_ProjectileTrail( cent );
				CG_EntityLoopSound( state, ATTN_NORM );
				CG_AddLightToScene( cent->ent.origin, 300, 0.8f, 0.6f, 0 );
				break;
			case ET_GRENADE:
				CG_AddGenericEnt( cent );
				CG_EntityLoopSound( state, ATTN_STATIC );
				CG_ProjectileTrail( cent );
				canLight = true;
				break;
			case ET_PLASMA:
				CG_AddGenericEnt( cent );
				CG_EntityLoopSound( state, ATTN_STATIC );
				break;

			case ET_PLAYER:
				CG_AddPlayerEnt( cent );
				CG_EntityLoopSound( state, ATTN_IDLE );
				CG_LaserBeamEffect( cent );
				CG_WeaponBeamEffect( cent );
				canLight = true;
				break;

			case ET_CORPSE:
				CG_AddPlayerEnt( cent );
				CG_EntityLoopSound( state, ATTN_IDLE );
				canLight = true;
				break;

			case ET_LASERBEAM:
				break;

			case ET_DECAL:
				CG_AddDecalEnt( cent );
				CG_EntityLoopSound( state, ATTN_STATIC );
				break;

			case ET_PUSH_TRIGGER:
				CG_EntityLoopSound( state, ATTN_STATIC );
				break;

			case ET_EVENT:
			case ET_SOUNDEVENT:
				break;

			case ET_PARTICLES:
				CG_AddParticlesEnt( cent );
				CG_EntityLoopSound( state, ATTN_STATIC );
				break;

			case ET_HUD:
				CG_AddBombHudEntity( cent );
				break;

			case ET_LASER:
				CG_AddLaserEnt( cent );
				CG_EntityLoopSound( state, ATTN_STATIC );
				break;

			case ET_SPIKES:
				CG_AddGenericEnt( cent );
				break;

			default:
				CG_Error( "CG_AddEntities: unknown entity type" );
				break;
		}

		// glow if light is set
		if( canLight && state->light ) {
			CG_AddLightToScene( cent->ent.origin,
								COLOR_A( state->light ) * 4.0,
								COLOR_R( state->light ) * ( 1.0 / 255.0 ),
								COLOR_G( state->light ) * ( 1.0 / 255.0 ),
								COLOR_B( state->light ) * ( 1.0 / 255.0 ) );
		}

		VectorCopy( cent->ent.origin, cent->trailOrigin );
	}
}

/*
* CG_LerpEntities
* Interpolate the entity states positions into the entity_t structs
*/
void CG_LerpEntities( void ) {
	ZoneScoped;

	entity_state_t *state;
	int pnum;
	centity_t *cent;

	for( pnum = 0; pnum < cg.frame.numEntities; pnum++ ) {
		int number;
		bool spatialize;

		state = &cg.frame.parsedEntities[pnum & ( MAX_PARSE_ENTITIES - 1 )];
		number = state->number;
		cent = &cg_entities[number];
		spatialize = true;

		switch( cent->type ) {
			case ET_GENERIC:
			case ET_GIB:
			case ET_ROCKET:
			case ET_PLASMA:
			case ET_GRENADE:
			case ET_PLAYER:
			case ET_CORPSE:
				if( state->linearMovement ) {
					CG_ExtrapolateLinearProjectile( cent );
				} else {
					CG_LerpGenericEnt( cent );
				}
				break;

			case ET_DECAL:
				CG_LerpDecalEnt( cent );
				break;

			case ET_LASERBEAM:
				CG_LerpLaserbeamEnt( cent );
				break;

			case ET_PUSH_TRIGGER:
				break;

			case ET_EVENT:
			case ET_SOUNDEVENT:
				break;

			case ET_PARTICLES:
				break;

			case ET_HUD:
				break;

			case ET_LASER:
				CG_LerpLaser( cent );
				break;

			case ET_SPIKES:
				CG_LerpSpikes( cent );
				break;

			default:
				CG_Error( "CG_LerpEntities: unknown entity type" );
				break;
		}

		if( spatialize ) {
			vec3_t origin, velocity;
			CG_GetEntitySpatilization( number, origin, velocity );
			S_UpdateEntity( number, origin, velocity );
		}
	}
}

/*
* CG_UpdateEntities
* Called at receiving a new serverframe. Sets up the model, type, etc to be drawn later on
*/
void CG_UpdateEntities( void ) {
	ZoneScoped;

	entity_state_t *state;
	int pnum;
	centity_t *cent;

	for( pnum = 0; pnum < cg.frame.numEntities; pnum++ ) {
		state = &cg.frame.parsedEntities[pnum & ( MAX_PARSE_ENTITIES - 1 )];
		cent = &cg_entities[state->number];
		cent->type = state->type;
		cent->effects = state->effects;

		switch( cent->type ) {
			case ET_GENERIC:
				CG_UpdateGenericEnt( cent );
				break;
			case ET_GIB:
				CG_UpdateGenericEnt( cent );

				// set the gib model ignoring the modelindex one
				cent->ent.model = cgs.media.modGib;
				break;

			// projectiles with linear trajectories
			case ET_ROCKET:
			case ET_PLASMA:
			case ET_GRENADE:
				CG_UpdateGenericEnt( cent );
				break;

			case ET_PLAYER:
			case ET_CORPSE:
				CG_UpdatePlayerModelEnt( cent );
				break;

			case ET_LASERBEAM:
				CG_UpdateLaserbeamEnt( cent );
				break;

			case ET_DECAL:
				CG_UpdateDecalEnt( cent );
				break;

			case ET_PUSH_TRIGGER:
				break;

			case ET_EVENT:
			case ET_SOUNDEVENT:
				break;

			case ET_PARTICLES:
				CG_UpdateParticlesEnt( cent );
				break;

			case ET_HUD:
				break;

			case ET_LASER:
				break;

			case ET_SPIKES:
				CG_UpdateSpikes( cent );
				break;

			default:
				CG_Error( "CG_UpdateEntities: unknown entity type %i", cent->type );
				break;
		}
	}
}

//=============================================================

/*
* CG_GetEntitySpatilization
*
* Called to get the sound spatialization origin and velocity
*/
void CG_GetEntitySpatilization( int entNum, vec3_t origin, vec3_t velocity ) {
	centity_t *cent;
	struct cmodel_s *cmodel;
	vec3_t mins, maxs;

	if( entNum < -1 || entNum >= MAX_EDICTS ) {
		CG_Error( "CG_GetEntitySpatilization: bad entnum" );
		return;
	}

	// hack for client side floatcam
	if( entNum == -1 ) {
		if( origin != NULL ) {
			VectorCopy( cg.frame.playerState.pmove.origin, origin );
		}
		if( velocity != NULL ) {
			VectorCopy( cg.frame.playerState.pmove.velocity, velocity );
		}
		return;
	}

	cent = &cg_entities[entNum];

	// normal
	if( cent->current.solid != SOLID_BMODEL ) {
		if( origin != NULL ) {
			VectorCopy( cent->ent.origin, origin );
		}
		if( velocity != NULL ) {
			VectorCopy( cent->velocity, velocity );
		}
		return;
	}

	// bmodel
	if( origin != NULL ) {
		cmodel = trap_CM_InlineModel( cent->current.modelindex );
		trap_CM_InlineModelBounds( cmodel, mins, maxs );
		VectorAdd( maxs, mins, origin );
		VectorMA( cent->ent.origin, 0.5f, origin, origin );
	}
	if( velocity != NULL ) {
		VectorCopy( cent->velocity, velocity );
	}
}
