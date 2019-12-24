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

#include "qcommon/assets.h"
#include "qcommon/string.h"
#include "cgame/cg_local.h"
#include "client/renderer/renderer.h"
#include "client/renderer/model.h"

pmodel_t cg_entPModels[MAX_EDICTS];
PlayerModelMetadata *cg_PModelInfos;

void CG_PModelsInit() {
	memset( cg_entPModels, 0, sizeof( cg_entPModels ) );
	cg_PModelInfos = NULL;
}

void CG_PModelsShutdown() {
	PlayerModelMetadata * next = cg_PModelInfos;
	while( next != NULL ) {
		PlayerModelMetadata * curr = next;
		next = next->next;
		CG_Free( curr );
	}
}

void CG_ResetPModels( void ) {
	for( int i = 0; i < MAX_EDICTS; i++ ) {
		cg_entPModels[i].flash_time = cg_entPModels[i].barrel_time = 0;
		memset( &cg_entPModels[i].animState, 0, sizeof( pmodel_animationstate_t ) );
	}
	memset( &cg.weapon, 0, sizeof( cg.weapon ) );
}

static Mat4 EulerAnglesToMat4( float pitch, float yaw, float roll ) {
	mat3_t axis;
	AnglesToAxis( tv( pitch, yaw, roll ), axis );

	Mat4 m = Mat4::Identity();

	m.col0.x = axis[ 0 ];
	m.col0.y = axis[ 1 ];
	m.col0.z = axis[ 2 ];
	m.col1.x = axis[ 3 ];
	m.col1.y = axis[ 4 ];
	m.col1.z = axis[ 5 ];
	m.col2.x = axis[ 6 ];
	m.col2.y = axis[ 7 ];
	m.col2.z = axis[ 8 ];

	return m;
}

/*
* CG_ParseAnimationScript
*
* Reads the animation config file.
*
* 0 = first frame
* 1 = lastframe
* 2 = looping frames
* 3 = fps
*
* Note: The animations count begins at 1, not 0. I preserve zero for "no animation change"
*/
static bool CG_ParseAnimationScript( PlayerModelMetadata * metadata, const char * filename ) {
	int num_clips = 1;

	Span< const char > cursor = AssetString( filename );
	if( cursor.ptr == NULL ) {
		CG_Printf( "Couldn't find animation script: %s\n", filename );
		return false;
	}

	while( true ) {
		Span< const char > cmd = ParseToken( &cursor, Parse_DontStopOnNewLine );
		if( cmd == "" )
			break;

		if( cmd == "upper_rotator_joints" ) {
			Span< const char > joint_name0 = ParseToken( &cursor, Parse_StopOnNewLine );
			FindJointByName( metadata->model, Hash32( joint_name0 ), &metadata->upper_rotator_joints[ 0 ] );

			Span< const char > joint_name1 = ParseToken( &cursor, Parse_StopOnNewLine );
			FindJointByName( metadata->model, Hash32( joint_name1 ), &metadata->upper_rotator_joints[ 1 ] );
		}
		else if( cmd == "head_rotator_joint" ) {
			Span< const char > joint_name = ParseToken( &cursor, Parse_StopOnNewLine );
			FindJointByName( metadata->model, Hash32( joint_name ), &metadata->head_rotator_joint );
		}
		else if( cmd == "upper_root_joint" ) {
			Span< const char > joint_name = ParseToken( &cursor, Parse_StopOnNewLine );
			FindJointByName( metadata->model, Hash32( joint_name ), &metadata->upper_root_joint );
		}
		else if( cmd == "tag" ) {
			Span< const char > joint_name = ParseToken( &cursor, Parse_StopOnNewLine );
			u8 joint_idx;
			if( FindJointByName( metadata->model, Hash32( joint_name ), &joint_idx ) ) {
				Span< const char > tag_name = ParseToken( &cursor, Parse_StopOnNewLine );
				PlayerModelMetadata::Tag * tag = &metadata->tag_backpack;
				if( tag_name == "tag_head" )
					tag = &metadata->tag_head;
				else if( tag_name == "tag_weapon" )
					tag = &metadata->tag_weapon;

				float forward = ParseFloat( &cursor, 0.0f, Parse_StopOnNewLine );
				float right = ParseFloat( &cursor, 0.0f, Parse_StopOnNewLine );
				float up = ParseFloat( &cursor, 0.0f, Parse_StopOnNewLine );
				float pitch = ParseFloat( &cursor, 0.0f, Parse_StopOnNewLine );
				float yaw = ParseFloat( &cursor, 0.0f, Parse_StopOnNewLine );
				float roll = ParseFloat( &cursor, 0.0f, Parse_StopOnNewLine );

				tag->joint_idx = joint_idx;
				tag->transform = Mat4Translation( forward, right, up ) * EulerAnglesToMat4( pitch, yaw, roll );
			}
			else {
				String< 64 > meh( "{}", joint_name );
				CG_Printf( "%s: Unknown joint name: %s\n", filename, meh.c_str() );
				for( int i = 0; i < 7; i++ )
					ParseToken( &cursor, Parse_StopOnNewLine );
			}
		}
		else if( cmd == "clip" ) {
			if( num_clips == PMODEL_TOTAL_ANIMATIONS ) {
				CG_Printf( "%s: Too many animations\n", filename );
				break;
			}

			int start_frame = ParseInt( &cursor, 0, Parse_StopOnNewLine );
			int end_frame = ParseInt( &cursor, 0, Parse_StopOnNewLine );
			int loop_frames = ParseInt( &cursor, 0, Parse_StopOnNewLine );
			int fps = ParseInt( &cursor, 0, Parse_StopOnNewLine );

			PlayerModelMetadata::AnimationClip clip;
			clip.start_time = float( start_frame ) / float( fps );
			clip.duration = float( end_frame - start_frame ) / float( fps );
			clip.loop_from = clip.duration - float( loop_frames ) / float( fps );

			metadata->clips[ num_clips ] = clip;
			num_clips++;
		}
		else {
			String< 64 > meh( "{}", cmd );
			CG_Printf( "%s: unrecognized cmd: %s\n", filename, meh.c_str() );
		}
	}

	if( num_clips < PMODEL_TOTAL_ANIMATIONS ) {
		CG_Printf( "%s: Not enough animations (%i)\n", filename, num_clips );
		return false;
	}

	metadata->clips[ ANIM_NONE ].start_time = 0;
	metadata->clips[ ANIM_NONE ].duration = 0;
	metadata->clips[ ANIM_NONE ].loop_from = 0;

	return true;
}

/*
* CG_LoadPlayerModel
*/
static bool CG_LoadPlayerModel( PlayerModelMetadata *metadata, const char *filename ) {
	ZoneScoped;

	bool loaded_model = false;
	char anim_filename[MAX_QPATH];

	metadata->model = FindModel( filename );

	// load animations script
	if( metadata->model ) {
		snprintf( anim_filename, sizeof( anim_filename ), "%s.cfg", filename );
		loaded_model = CG_ParseAnimationScript( metadata, anim_filename );
	}

	// clean up if failed
	if( !loaded_model ) {
		metadata->model = NULL;
		return false;
	}

	metadata->name_hash = Hash64( filename );

	CG_RegisterPlayerSounds( metadata, filename );

	return true;
}

/*
* CG_RegisterPModel
* PModel is not exactly the model, but the indexes of the
* models contained in the pmodel and it's animation data
*/
PlayerModelMetadata *CG_RegisterPlayerModel( const char *filename ) {
	PlayerModelMetadata *metadata;

	u64 hash = Hash64( filename );
	for( metadata = cg_PModelInfos; metadata; metadata = metadata->next ) {
		if( hash == metadata->name_hash ) {
			return metadata;
		}
	}

	metadata = ( PlayerModelMetadata * )CG_Malloc( sizeof( PlayerModelMetadata ) );
	if( !CG_LoadPlayerModel( metadata, filename ) ) {
		CG_Free( metadata );
		return NULL;
	}

	metadata->next = cg_PModelInfos;
	cg_PModelInfos = metadata;

	return metadata;
}

//======================================================================
//							tools
//======================================================================

/*
* CG_GrabTag
*/
bool CG_GrabTag( orientation_t *tag, entity_t *ent, const char *tagname ) {
	return false;
}

/*
* CG_PlaceRotatedModelOnTag
*/
void CG_PlaceRotatedModelOnTag( entity_t *ent, entity_t *dest, orientation_t *tag ) {
	mat3_t tmpAxis;

	VectorCopy( dest->origin, ent->origin );

	for( int i = 0; i < 3; i++ )
		VectorMA( ent->origin, tag->origin[i] * ent->scale, &dest->axis[i * 3], ent->origin );

	VectorCopy( ent->origin, ent->origin2 );
	Matrix3_Multiply( ent->axis, tag->axis, tmpAxis );
	Matrix3_Multiply( tmpAxis, dest->axis, ent->axis );
}

/*
* CG_PlaceModelOnTag
*/
void CG_PlaceModelOnTag( entity_t *ent, entity_t *dest, const orientation_t *tag ) {
	VectorCopy( dest->origin, ent->origin );

	for( int i = 0; i < 3; i++ )
		VectorMA( ent->origin, tag->origin[i] * ent->scale, &dest->axis[i * 3], ent->origin );

	VectorCopy( ent->origin, ent->origin2 );
	Matrix3_Multiply( tag->axis, dest->axis, ent->axis );
}

/*
* CG_MoveToTag
* "move" tag must have an axis and origin set up. Use vec3_origin and axis_identity for "nothing"
*/
void CG_MoveToTag( vec3_t move_origin,
				   mat3_t move_axis,
				   const vec3_t space_origin,
				   const mat3_t space_axis,
				   const vec3_t tag_origin,
				   const mat3_t tag_axis ) {
	mat3_t tmpAxis;

	VectorCopy( space_origin, move_origin );

	for( int i = 0; i < 3; i++ )
		VectorMA( move_origin, tag_origin[i], &space_axis[i * 3], move_origin );

	Matrix3_Multiply( move_axis, tag_axis, tmpAxis );
	Matrix3_Multiply( tmpAxis, space_axis, move_axis );
}

/*
* CG_PModel_GetProjectionSource
* It asumes the player entity is up to date
*/
bool CG_PModel_GetProjectionSource( int entnum, orientation_t *tag_result ) {
	centity_t *cent;
	pmodel_t *pmodel;

	if( !tag_result ) {
		return false;
	}

	if( entnum < 1 || entnum >= MAX_EDICTS ) {
		return false;
	}

	cent = &cg_entities[entnum];
	if( cent->serverFrame != cg.frame.serverFrame ) {
		return false;
	}

	// see if it's the view weapon
	if( ISVIEWERENTITY( entnum ) && !cg.view.thirdperson ) {
		VectorCopy( cg.weapon.projectionSource.origin, tag_result->origin );
		Matrix3_Copy( cg.weapon.projectionSource.axis, tag_result->axis );
		return true;
	}

	return false;

	// it's a 3rd person model
	pmodel = &cg_entPModels[entnum];
	VectorCopy( pmodel->projectionSource.origin, tag_result->origin );
	Matrix3_Copy( pmodel->projectionSource.axis, tag_result->axis );
	return true;
}

/*
* CG_OutlineScaleForDist
*/
static float CG_OutlineScaleForDist( const entity_t * e, float maxdist, float scale ) {
	float dist;
	vec3_t dir;

	// if( e->renderfx & RenderFX_WeaponModel ) {
	// 	return 0.14f;
	// }

	// Kill if behind the view or if too far away
	VectorSubtract( e->origin, cg.view.origin, dir );
	dist = VectorNormalize2( dir, dir ) * cg.view.fracDistFOV;
	if( dist > maxdist ) {
		return 0;
	}

	if( DotProduct( dir, &cg.view.axis[AXIS_FORWARD] ) < 0 ) {
		return 0;
	}

	dist *= scale;

	if( dist < 64 ) {
		return 0.14f;
	}
	if( dist < 128 ) {
		return 0.30f;
	}
	if( dist < 256 ) {
		return 0.42f;
	}
	if( dist < 512 ) {
		return 0.56f;
	}
	if( dist < 768 ) {
		return 0.70f;
	}

	return 1.0f;
}

void CG_AddOutline( entity_t *ent, int effects, RGBA8 color ) {
	ent->outlineHeight = CG_OutlineScaleForDist( ent, 4096, 1.0f );
	ent->outlineColor = ( effects & EF_GODMODE ) ? RGBA8( 255, 255, 255, color.a ) : color;
}

//======================================================================
//							animations
//======================================================================

// movement flags for animation control
#define ANIMMOVE_FRONT      ( 1 << 0 )  // Player is pressing fordward
#define ANIMMOVE_BACK       ( 1 << 1 )  // Player is pressing backpedal
#define ANIMMOVE_LEFT       ( 1 << 2 )  // Player is pressing sideleft
#define ANIMMOVE_RIGHT      ( 1 << 3 )  // Player is pressing sideright
#define ANIMMOVE_WALK       ( 1 << 4 )  // Player is pressing the walk key
#define ANIMMOVE_RUN        ( 1 << 5 )  // Player is running
#define ANIMMOVE_DUCK       ( 1 << 6 )  // Player is crouching
#define ANIMMOVE_SWIM       ( 1 << 7 )  // Player is swimming
#define ANIMMOVE_AIR        ( 1 << 8 )  // Player is at air, but not jumping
#define ANIMMOVE_DEAD       ( 1 << 9 )  // Player is a corpse

static int CG_MoveFlagsToUpperAnimation( uint32_t moveflags, int carried_weapon ) {
	if( moveflags & ANIMMOVE_DEAD )
		return ANIM_NONE;
	if( moveflags & ANIMMOVE_SWIM )
		return TORSO_SWIM;

	switch( carried_weapon ) {
		case WEAP_NONE:
			return TORSO_HOLD_BLADE; // fixme: a special animation should exist
		case WEAP_GUNBLADE:
			return TORSO_HOLD_BLADE;
		case WEAP_LASERGUN:
			return TORSO_HOLD_PISTOL;
		case WEAP_RIOTGUN:
		case WEAP_PLASMAGUN:
			return TORSO_HOLD_LIGHTWEAPON;
		case WEAP_ROCKETLAUNCHER:
		case WEAP_GRENADELAUNCHER:
			return TORSO_HOLD_HEAVYWEAPON;
		case WEAP_ELECTROBOLT:
			return TORSO_HOLD_AIMWEAPON;
	}

	return TORSO_HOLD_LIGHTWEAPON;
}

static int CG_MoveFlagsToLowerAnimation( uint32_t moveflags ) {
	if( moveflags & ANIMMOVE_DEAD )
		return ANIM_NONE;

	if( moveflags & ANIMMOVE_SWIM )
		return ( moveflags & ANIMMOVE_FRONT ) ? LEGS_SWIM_FORWARD : LEGS_SWIM_NEUTRAL;

	if( moveflags & ANIMMOVE_DUCK ) {
		if( moveflags & ( ANIMMOVE_WALK | ANIMMOVE_RUN ) )
			return LEGS_CROUCH_WALK;
		return LEGS_CROUCH_IDLE;
	}

	if( moveflags & ANIMMOVE_AIR )
		return LEGS_JUMP_NEUTRAL;

	if( moveflags & ANIMMOVE_RUN ) {
		// front/backward has priority over side movements
		if( moveflags & ANIMMOVE_FRONT )
			return LEGS_RUN_FORWARD;
		if( moveflags & ANIMMOVE_BACK )
			return LEGS_RUN_BACK;
		if( moveflags & ANIMMOVE_RIGHT )
			return LEGS_RUN_RIGHT;
		if( moveflags & ANIMMOVE_LEFT )
			return LEGS_RUN_LEFT;
		return LEGS_WALK_FORWARD;
	}

	if( moveflags & ANIMMOVE_WALK ) {
		// front/backward has priority over side movements
		if( moveflags & ANIMMOVE_FRONT )
			return LEGS_WALK_FORWARD;
		if( moveflags & ANIMMOVE_BACK )
			return LEGS_WALK_BACK;
		if( moveflags & ANIMMOVE_RIGHT )
			return LEGS_WALK_RIGHT;
		if( moveflags & ANIMMOVE_LEFT )
			return LEGS_WALK_LEFT;
		return LEGS_WALK_FORWARD;
	}

	return LEGS_STAND_IDLE;
}

static PlayerModelAnimationSet CG_GetBaseAnims( entity_state_t *state, const vec3_t velocity ) {
	constexpr float MOVEDIREPSILON = 0.3f;
	constexpr float WALKEPSILON = 5.0f;
	constexpr float RUNEPSILON = 220.0f;

	uint32_t moveflags = 0;
	vec3_t movedir;
	vec3_t hvel;
	mat3_t viewaxis;
	float xyspeedcheck;
	int waterlevel;
	vec3_t mins, maxs;
	vec3_t point;
	trace_t trace;

	if( state->type == ET_CORPSE ) {
		PlayerModelAnimationSet a;
		a.parts[ LOWER ] = ANIM_NONE;
		a.parts[ UPPER ] = ANIM_NONE;
		a.parts[ HEAD ] = ANIM_NONE;
		return a;
	}

	CG_BBoxForEntityState( state, mins, maxs );

	// determine if player is at ground, for walking or falling
	// this is not like having groundEntity, we are more generous with
	// the tracing size here to include small steps
	point[0] = state->origin[0];
	point[1] = state->origin[1];
	point[2] = state->origin[2] - ( 1.6 * STEPSIZE );
	client_gs.api.Trace( &trace, state->origin, mins, maxs, point, state->number, MASK_PLAYERSOLID, 0 );
	if( trace.ent == -1 || ( trace.fraction < 1.0f && !ISWALKABLEPLANE( &trace.plane ) && !trace.startsolid ) ) {
		moveflags |= ANIMMOVE_AIR;
	}

	// crouching : fixme? : it assumes the entity is using the player box sizes
	if( VectorCompare( maxs, playerbox_crouch_maxs ) ) {
		moveflags |= ANIMMOVE_DUCK;
	}

	// find out the water level
	waterlevel = GS_WaterLevel( &client_gs, state, mins, maxs );
	if( waterlevel >= 2 || ( waterlevel && ( moveflags & ANIMMOVE_AIR ) ) ) {
		moveflags |= ANIMMOVE_SWIM;
	}

	//find out what are the base movements the model is doing

	hvel[0] = velocity[0];
	hvel[1] = velocity[1];
	hvel[2] = 0;
	xyspeedcheck = VectorNormalize2( hvel, movedir );
	if( xyspeedcheck > WALKEPSILON ) {
		Matrix3_FromAngles( tv( 0, state->angles[YAW], 0 ), viewaxis );

		// if it's moving to where is looking, it's moving forward
		if( DotProduct( movedir, &viewaxis[AXIS_RIGHT] ) > MOVEDIREPSILON ) {
			moveflags |= ANIMMOVE_RIGHT;
		} else if( -DotProduct( movedir, &viewaxis[AXIS_RIGHT] ) > MOVEDIREPSILON ) {
			moveflags |= ANIMMOVE_LEFT;
		}
		if( DotProduct( movedir, &viewaxis[AXIS_FORWARD] ) > MOVEDIREPSILON ) {
			moveflags |= ANIMMOVE_FRONT;
		} else if( -DotProduct( movedir, &viewaxis[AXIS_FORWARD] ) > MOVEDIREPSILON ) {
			moveflags |= ANIMMOVE_BACK;
		}

		if( xyspeedcheck > RUNEPSILON ) {
			moveflags |= ANIMMOVE_RUN;
		} else if( xyspeedcheck > WALKEPSILON ) {
			moveflags |= ANIMMOVE_WALK;
		}
	}

	PlayerModelAnimationSet a;
	a.parts[ LOWER ] = CG_MoveFlagsToLowerAnimation( moveflags );
	a.parts[ UPPER ] = CG_MoveFlagsToUpperAnimation( moveflags, state->weapon );
	a.parts[ HEAD ] = ANIM_NONE;
	return a;
}

static float GetAnimationTime( const PlayerModelMetadata * metadata, int64_t curTime, animstate_t state, bool loop ) {
	if( state.anim == ANIM_NONE )
		return -1.0f;

	PlayerModelMetadata::AnimationClip clip = metadata->clips[ state.anim ];

	float t = Max2( 0.0f, ( curTime - state.startTimestamp ) / 1000.0f );
	if( loop ) {
		if( clip.duration == 0 )
			return clip.start_time;

		if( t > clip.loop_from ) {
			float loop_t = PositiveMod( t - clip.loop_from, clip.duration - clip.loop_from );
			t = clip.loop_from + loop_t;
		}
	}
	else if( t >= clip.duration ) {
		return -1.0f;
	}

	return clip.start_time + t;
}

/*
*CG_PModel_AnimToFrame
*
* BASE_CHANEL plays continuous animations forced to loop.
* if the same animation is received twice it will *not* restart
* but continue looping.
*
* EVENT_CHANNEL overrides base channel and plays until
* the animation is finished. Then it returns to base channel.
* If an animation is received twice, it will be restarted.
* If an event channel animation has a loop setting, it will
* continue playing it until a new event chanel animation
* is fired.
*/
static void CG_GetAnimationTimes( pmodel_t * pmodel, int64_t curTime, float * lower_time, float * upper_time ) {
	float times[ PMODEL_PARTS ];

	for( int i = LOWER; i < PMODEL_PARTS; i++ ) {
		for( int channel = BASE_CHANNEL; channel < PLAYERANIM_CHANNELS; channel++ ) {
			animstate_t *currAnim = &pmodel->animState.curAnims[i][channel];

			// see if there are new animations to be played
			if( pmodel->animState.pending[channel].parts[i] != ANIM_NONE ) {
				if( channel == EVENT_CHANNEL || ( channel == BASE_CHANNEL && pmodel->animState.pending[channel].parts[i] != currAnim->anim ) ) {
					currAnim->anim = pmodel->animState.pending[channel].parts[i];
					currAnim->startTimestamp = curTime;
				}

				pmodel->animState.pending[channel].parts[i] = ANIM_NONE;
			}
		}

		times[ i ] = GetAnimationTime( pmodel->metadata, curTime, pmodel->animState.curAnims[ i ][ EVENT_CHANNEL ], false );
		if( times[ i ] == -1.0f ) {
			pmodel->animState.curAnims[ i ][ EVENT_CHANNEL ].anim = ANIM_NONE;
			times[ i ] = GetAnimationTime( pmodel->metadata, curTime, pmodel->animState.curAnims[ i ][ BASE_CHANNEL ], true );
		}
	}

	*lower_time = times[ LOWER ];
	*upper_time = times[ UPPER ];
}

void CG_PModel_ClearEventAnimations( int entNum ) {
	pmodel_animationstate_t & animState = cg_entPModels[ entNum ].animState;
	for( int i = LOWER; i < PMODEL_PARTS; i++ ) {
		animState.curAnims[ i ][ EVENT_CHANNEL ].anim = ANIM_NONE;
		animState.pending[ EVENT_CHANNEL ].parts[ i ] = ANIM_NONE;
	}
}

void CG_PModel_AddAnimation( int entNum, int loweranim, int upperanim, int headanim, int channel ) {
	PlayerModelAnimationSet & pending = cg_entPModels[entNum].animState.pending[ channel ];
	if( loweranim != ANIM_NONE )
		pending.parts[ LOWER ] = loweranim;
	if( upperanim != ANIM_NONE )
		pending.parts[ UPPER ] = upperanim;
	if( headanim != ANIM_NONE )
		pending.parts[ HEAD ] = headanim;
}


//======================================================================
//							player model
//======================================================================


void CG_PModel_LeanAngles( centity_t *cent, pmodel_t *pmodel ) {
	mat3_t axis;
	vec3_t hvelocity;
	float speed;
	vec3_t leanAngles[PMODEL_PARTS];

	memset( leanAngles, 0, sizeof( leanAngles ) );

	hvelocity[0] = cent->animVelocity[0];
	hvelocity[1] = cent->animVelocity[1];
	hvelocity[2] = 0;

	float scale = 0.04f;

	if( ( speed = VectorLengthFast( hvelocity ) ) * scale > 1.0f ) {
		AnglesToAxis( tv( 0, cent->current.angles[YAW], 0 ), axis );

		float front = scale * DotProduct( hvelocity, &axis[AXIS_FORWARD] );
		if( front < -0.1 || front > 0.1 ) {
			leanAngles[LOWER][PITCH] += front;
			leanAngles[UPPER][PITCH] -= front * 0.25;
			leanAngles[HEAD][PITCH] -= front * 0.5;
		}

		float aside = ( front * 0.001f ) * cent->yawVelocity;

		if( aside ) {
			float asidescale = 75;
			leanAngles[LOWER][ROLL] -= aside * 0.5 * asidescale;
			leanAngles[UPPER][ROLL] += aside * 1.75 * asidescale;
			leanAngles[HEAD][ROLL] -= aside * 0.35 * asidescale;
		}

		float side = scale * DotProduct( hvelocity, &axis[AXIS_RIGHT] );

		if( side < -1 || side > 1 ) {
			leanAngles[LOWER][ROLL] -= side * 0.5;
			leanAngles[UPPER][ROLL] += side * 0.5;
			leanAngles[HEAD][ROLL] += side * 0.25;
		}

		leanAngles[LOWER][PITCH] = Clamp( -45.0f, leanAngles[LOWER][PITCH], 45.0f );
		leanAngles[LOWER][ROLL] = Clamp( -15.0f, leanAngles[LOWER][ROLL], 15.0f );

		leanAngles[UPPER][PITCH] = Clamp( -45.0f, leanAngles[UPPER][PITCH], 45.0f );
		leanAngles[UPPER][ROLL] = Clamp( -20.0f, leanAngles[UPPER][ROLL], 20.0f );

		leanAngles[HEAD][PITCH] = Clamp( -45.0f, leanAngles[HEAD][PITCH], 45.0f );
		leanAngles[HEAD][ROLL] = Clamp( -20.0f, leanAngles[HEAD][ROLL], 20.0f );
	}

	for( int j = LOWER; j < PMODEL_PARTS; j++ ) {
		for( int i = 0; i < 3; i++ )
			pmodel->angles[i][j] = AngleNormalize180( pmodel->angles[i][j] + leanAngles[i][j] );
	}
}

/*
* CG_UpdatePModelAnimations
* It's better to delay this set up until the other entities are linked, so they
* can be detected as groundentities by the animation checks
*/
static void CG_UpdatePModelAnimations( centity_t *cent ) {
	PlayerModelAnimationSet a = CG_GetBaseAnims( &cent->current, cent->animVelocity );
	CG_PModel_AddAnimation( cent->current.number, a.parts[LOWER], a.parts[UPPER], a.parts[HEAD], BASE_CHANNEL );
}

/*
* CG_UpdatePlayerModelEnt
* Called each new serverframe
*/
void CG_UpdatePlayerModelEnt( centity_t *cent ) {
	// start from clean
	memset( &cent->ent, 0, sizeof( cent->ent ) );
	cent->ent.scale = 1.0f;

	pmodel_t * pmodel = &cg_entPModels[cent->current.number];
	pmodel->metadata = CG_PModelForCentity( cent );

	cent->ent.color = RGBA8( CG_TeamColor( cent->current.number ) );

	// Spawning (teleported bit) forces nobacklerp and the interruption of EVENT_CHANNEL animations
	if( cent->current.teleported ) {
		CG_PModel_ClearEventAnimations( cent->current.number );
	}

	// update parts rotation angles
	for( int i = LOWER; i < PMODEL_PARTS; i++ )
		VectorCopy( pmodel->angles[i], pmodel->oldangles[i] );

	if( cent->current.type == ET_CORPSE ) {
		VectorClear( cent->animVelocity );
		cent->yawVelocity = 0;
	} else {
		// update smoothed velocities used for animations and leaning angles
		// rotational yaw velocity
		float adelta = AngleDelta( cent->current.angles[YAW], cent->prev.angles[YAW] );
		adelta = Clamp( -35.0f, adelta, 35.0f );

		// smooth a velocity vector between the last snaps
		cent->lastVelocities[cg.frame.serverFrame & 3][0] = cent->velocity[0];
		cent->lastVelocities[cg.frame.serverFrame & 3][1] = cent->velocity[1];
		cent->lastVelocities[cg.frame.serverFrame & 3][2] = 0;
		cent->lastVelocities[cg.frame.serverFrame & 3][3] = adelta;
		cent->lastVelocitiesFrames[cg.frame.serverFrame & 3] = cg.frame.serverFrame;

		int count = 0;
		VectorClear( cent->animVelocity );
		cent->yawVelocity = 0;
		for( int i = cg.frame.serverFrame; ( i >= 0 ) && ( count < 3 ) && ( i == cent->lastVelocitiesFrames[i & 3] ); i-- ) {
			count++;
			cent->animVelocity[0] += cent->lastVelocities[i & 3][0];
			cent->animVelocity[1] += cent->lastVelocities[i & 3][1];
			cent->animVelocity[2] += cent->lastVelocities[i & 3][2];
			cent->yawVelocity += cent->lastVelocities[i & 3][3];
		}

		// safety/static code analysis check
		if( count == 0 ) {
			count = 1;
		}

		VectorScale( cent->animVelocity, 1.0f / (float)count, cent->animVelocity );
		cent->yawVelocity /= (float)count;


		//
		// Calculate angles for each model part
		//

		// lower has horizontal direction, and zeroes vertical
		pmodel->angles[LOWER][PITCH] = 0;
		pmodel->angles[LOWER][YAW] = cent->current.angles[YAW];
		pmodel->angles[LOWER][ROLL] = 0;

		// upper marks vertical direction (total angle, so it fits aim)
		if( cent->current.angles[PITCH] > 180 ) {
			pmodel->angles[UPPER][PITCH] = ( -360 + cent->current.angles[PITCH] );
		} else {
			pmodel->angles[UPPER][PITCH] = cent->current.angles[PITCH];
		}

		pmodel->angles[UPPER][YAW] = 0;
		pmodel->angles[UPPER][ROLL] = 0;

		// head adds a fraction of vertical angle again
		if( cent->current.angles[PITCH] > 180 ) {
			pmodel->angles[HEAD][PITCH] = ( -360 + cent->current.angles[PITCH] ) / 3;
		} else {
			pmodel->angles[HEAD][PITCH] = cent->current.angles[PITCH] / 3;
		}

		pmodel->angles[HEAD][YAW] = 0;
		pmodel->angles[HEAD][ROLL] = 0;

		CG_PModel_LeanAngles( cent, pmodel );
	}


	// Spawning (teleported bit) forces nobacklerp and the interruption of EVENT_CHANNEL animations
	if( cent->current.teleported ) {
		for( int i = LOWER; i < PMODEL_PARTS; i++ )
			VectorCopy( pmodel->angles[i], pmodel->oldangles[i] );
	}

	CG_UpdatePModelAnimations( cent );
}

static Quaternion EulerAnglesToQuaternion( EulerDegrees3 angles ) {
	float cp = cosf( DEG2RAD( angles.pitch ) * 0.5f );
	float sp = sinf( DEG2RAD( angles.pitch ) * 0.5f );
	float cy = cosf( DEG2RAD( angles.yaw ) * 0.5f );
	float sy = sinf( DEG2RAD( angles.yaw ) * 0.5f );
	float cr = cosf( DEG2RAD( angles.roll ) * 0.5f );
	float sr = sinf( DEG2RAD( angles.roll ) * 0.5f );

	return Quaternion(
		cp * cy * sr - sp * sy * cr,
		cp * sy * cr + sp * cy * sr,
		sp * cy * cr - cp * sy * sr,
		cp * cy * cr + sp * sy * sr
	);
}

static Mat4 TransformTag( const Model * model, const Mat4 & transform, const MatrixPalettes & pose, const PlayerModelMetadata::Tag & tag ) {
	return transform * model->transform * pose.joint_poses[ tag.joint_idx ] * tag.transform;
}

static orientation_t Mat4ToOrientation( const Mat4 & m ) {
	orientation_t o;

	o.axis[ 0 ] = m.col0.x;
	o.axis[ 1 ] = m.col0.y;
	o.axis[ 2 ] = m.col0.z;
	o.axis[ 3 ] = m.col1.x;
	o.axis[ 4 ] = m.col1.y;
	o.axis[ 5 ] = m.col1.z;
	o.axis[ 6 ] = m.col2.x;
	o.axis[ 7 ] = m.col2.y;
	o.axis[ 8 ] = m.col2.z;

	o.origin[ 0 ] = m.col3.x;
	o.origin[ 1 ] = m.col3.y;
	o.origin[ 2 ] = m.col3.z;

	return o;
}

void CG_DrawPlayer( centity_t *cent ) {
	pmodel_t * pmodel = &cg_entPModels[cent->current.number];
	const PlayerModelMetadata * meta = pmodel->metadata;

	// if viewer model, and casting shadows, offset the entity to predicted player position
	// for view and shadow accuracy

	if( ISVIEWERENTITY( cent->current.number ) ) {
		vec3_t origin;

		if( cg.view.playerPrediction ) {
			float backlerp = 1.0f - cg.lerpfrac;

			for( int i = 0; i < 3; i++ )
				origin[i] = cg.predictedPlayerState.pmove.origin[i] - backlerp * cg.predictionError[i];

			CG_ViewSmoothPredictedSteps( origin );
		}
		else {
			VectorCopy( cent->ent.origin, origin );
		}

		VectorCopy( origin, cent->ent.origin );
		VectorCopy( origin, cent->ent.origin2 );
	}

	TempAllocator temp = cls.frame_arena.temp();

	float lower_time, upper_time;
	CG_GetAnimationTimes( pmodel, cl.serverTime, &lower_time, &upper_time );
	Span< TRS > lower = SampleAnimation( &temp, meta->model, lower_time );
	Span< TRS > upper = SampleAnimation( &temp, meta->model, upper_time );
	MergeLowerUpperPoses( lower, upper, meta->model, meta->upper_root_joint );

	// add skeleton effects (pose is unmounted yet)
	bool corpse = cent->current.type == ET_CORPSE;
	if( !corpse ) {
		vec3_t tmpangles;
		// if it's our client use the predicted angles
		if( cg.view.playerPrediction && ISVIEWERENTITY( cent->current.number ) ) {
			tmpangles[ YAW ] = cg.predictedPlayerState.viewangles[YAW];
			tmpangles[ PITCH ] = 0;
			tmpangles[ ROLL ] = 0;
		}
		else {
			// apply interpolated LOWER angles to entity
			for( int i = 0; i < 3; i++ ) {
				tmpangles[i] = LerpAngle( pmodel->oldangles[LOWER][i], pmodel->angles[LOWER][i], cg.lerpfrac );
			}
		}

		AnglesToAxis( tmpangles, cent->ent.axis );

		// apply UPPER and HEAD angles to rotator joints
		// also add rotations from velocity leaning
		{
			EulerDegrees3 angles;
			angles.pitch = LerpAngle( pmodel->oldangles[ UPPER ][ PITCH ], pmodel->angles[ UPPER ][ PITCH ], cg.lerpfrac ) / 2.0f;
			angles.yaw = LerpAngle( pmodel->oldangles[ UPPER ][ YAW ], pmodel->angles[ UPPER ][ YAW ], cg.lerpfrac ) / 2.0f;
			angles.roll = LerpAngle( pmodel->oldangles[ UPPER ][ ROLL ], pmodel->angles[ UPPER ][ ROLL ], cg.lerpfrac ) / 2.0f;

			Quaternion q = EulerAnglesToQuaternion( angles );
			lower[ meta->upper_rotator_joints[ 0 ] ].rotation *= q;
			lower[ meta->upper_rotator_joints[ 1 ] ].rotation *= q;
		}

		{
			EulerDegrees3 angles;
			angles.pitch = LerpAngle( pmodel->oldangles[ HEAD ][ PITCH ], pmodel->angles[ HEAD ][ PITCH ], cg.lerpfrac );
			angles.yaw = LerpAngle( pmodel->oldangles[ HEAD ][ YAW ], pmodel->angles[ HEAD ][ YAW ], cg.lerpfrac );
			angles.roll = LerpAngle( pmodel->oldangles[ HEAD ][ ROLL ], pmodel->angles[ HEAD ][ ROLL ], cg.lerpfrac );

			lower[ meta->head_rotator_joint ].rotation *= EulerAnglesToQuaternion( angles );
		}
	}

	MatrixPalettes pose = ComputeMatrixPalettes( &temp, meta->model, lower );

	// CG_AllocPlayerShadow( cent->current.number, cent->ent.origin, playerbox_stand_mins, playerbox_stand_maxs );

	Mat4 transform = FromQFAxisAndOrigin( cent->ent.axis, cent->ent.origin );

	Vec4 color = CG_TeamColorVec4( cent->current.team );
	if( corpse ) {
		color *= Vec4( 0.25f, 0.25f, 0.25f, 1.0 );
	}

	DrawModel( meta->model, transform, color, pose.skinning_matrices );

	bool speccing = cg.predictedPlayerState.stats[ STAT_REALTEAM ] == TEAM_SPECTATOR;
	bool same_team = GS_TeamBasedGametype( &client_gs ) && cg.predictedPlayerState.stats[ STAT_TEAM ] == cent->current.team;
	if( !corpse && ( speccing || same_team ) ) {
		DrawModelSilhouette( meta->model, transform, color, pose.skinning_matrices );
	}

	float outline_height = CG_OutlineScaleForDist( &cent->ent, 4096, 1.0f );
	DrawOutlinedModel( meta->model, transform, vec4_black, outline_height, pose.skinning_matrices );

	CG_PModel_SpawnTeleportEffect( cent, pose );

	// add weapon model
	if( cent->current.weapon ) {
		orientation_t tag_weapon = Mat4ToOrientation( TransformTag( meta->model, transform, pose, meta->tag_weapon ) );
		CG_AddWeaponOnTag( &cent->ent, &tag_weapon, cent->current.weapon, cent->effects,
			&pmodel->projectionSource, pmodel->flash_time, pmodel->barrel_time );
	}

	// add backpack/hat
	if( cent->current.modelindex2 ) {
		const Model * attached_model = cgs.modelDraw[ cent->current.modelindex2 ];
		if( attached_model != NULL ) {
			PlayerModelMetadata::Tag tag = meta->tag_backpack;
			if( cent->current.effects & EF_HAT )
				tag = meta->tag_head;
			Mat4 tag_transform = TransformTag( meta->model, transform, pose, tag );
			DrawModel( attached_model, tag_transform, vec4_white );

			if( speccing || same_team ) {
				DrawModelSilhouette( attached_model, tag_transform, color );
			}
		}
	}
}
