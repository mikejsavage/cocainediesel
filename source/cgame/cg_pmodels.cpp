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

#include "cgame/cg_local.h"
#include "qcommon/hash.h"
#include "qcommon/hashtable.h"
#include "client/assets.h"
#include "client/renderer/renderer.h"
#include "client/renderer/model.h"

constexpr u32 MAX_PLAYER_MODELS = 128;

pmodel_t cg_entPModels[ MAX_EDICTS ];

static PlayerModelMetadata player_model_metadatas[ MAX_PLAYER_MODELS ];
static u32 num_player_models;
static Hashtable< MAX_PLAYER_MODELS * 2 > player_models_hashtable;

static Mat4 EulerAnglesToMat4( float pitch, float yaw, float roll ) {
	mat3_t axis;
	AnglesToAxis( Vec3( pitch, yaw, roll ), axis );

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

static bool ParsePlayerModelConfig( PlayerModelMetadata * meta, const char * filename ) {
	int num_clips = 1;

	Span< const char > cursor = AssetString( filename );
	if( cursor.ptr == NULL ) {
		Com_Printf( "Couldn't find animation script: %s\n", filename );
		return false;
	}

	while( true ) {
		Span< const char > cmd = ParseToken( &cursor, Parse_DontStopOnNewLine );
		if( cmd == "" )
			break;

		if( cmd == "upper_rotator_joints" ) {
			Span< const char > node_name0 = ParseToken( &cursor, Parse_StopOnNewLine );
			FindNodeByName( meta->model, Hash32( node_name0 ), &meta->upper_rotator_nodes[ 0 ] );

			Span< const char > node_name1 = ParseToken( &cursor, Parse_StopOnNewLine );
			FindNodeByName( meta->model, Hash32( node_name1 ), &meta->upper_rotator_nodes[ 1 ] );
		}
		else if( cmd == "head_rotator_joint" ) {
			Span< const char > node_name = ParseToken( &cursor, Parse_StopOnNewLine );
			FindNodeByName( meta->model, Hash32( node_name ), &meta->head_rotator_node );
		}
		else if( cmd == "upper_root_joint" ) {
			Span< const char > node_name = ParseToken( &cursor, Parse_StopOnNewLine );
			FindNodeByName( meta->model, Hash32( node_name ), &meta->upper_root_node );
		}
		else if( cmd == "tag" ) {
			Span< const char > node_name = ParseToken( &cursor, Parse_StopOnNewLine );
			u8 node_idx;
			if( FindNodeByName( meta->model, Hash32( node_name ), &node_idx ) ) {
				Span< const char > tag_name = ParseToken( &cursor, Parse_StopOnNewLine );
				PlayerModelMetadata::Tag * tag = &meta->tag_bomb;
				if( tag_name == "tag_hat" )
					tag = &meta->tag_hat;
				else if( tag_name == "tag_weapon" )
					tag = &meta->tag_weapon;

				float forward = ParseFloat( &cursor, 0.0f, Parse_StopOnNewLine );
				float right = ParseFloat( &cursor, 0.0f, Parse_StopOnNewLine );
				float up = ParseFloat( &cursor, 0.0f, Parse_StopOnNewLine );
				float pitch = ParseFloat( &cursor, 0.0f, Parse_StopOnNewLine );
				float yaw = ParseFloat( &cursor, 0.0f, Parse_StopOnNewLine );
				float roll = ParseFloat( &cursor, 0.0f, Parse_StopOnNewLine );

				tag->node_idx = node_idx;
				tag->transform = Mat4Translation( forward, right, up ) * EulerAnglesToMat4( pitch, yaw, roll );
			}
			else {
				Com_GGPrint( "{}: Unknown node name: {}", filename, node_name );
				for( int i = 0; i < 7; i++ )
					ParseToken( &cursor, Parse_StopOnNewLine );
			}
		}
		else if( cmd == "clip" ) {
			if( num_clips == PMODEL_TOTAL_ANIMATIONS ) {
				Com_Printf( "%s: Too many animations\n", filename );
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

			meta->clips[ num_clips ] = clip;
			num_clips++;
		}
		else {
			Com_GGPrint( "{}: unrecognized cmd: {}", filename, cmd );
		}
	}

	if( num_clips < PMODEL_TOTAL_ANIMATIONS ) {
		Com_Printf( "%s: Not enough animations (%i)\n", filename, num_clips );
		return false;
	}

	meta->clips[ ANIM_NONE ].start_time = 0;
	meta->clips[ ANIM_NONE ].duration = 0;
	meta->clips[ ANIM_NONE ].loop_from = 0;

	return true;
}

static constexpr const char * PLAYER_SOUND_NAMES[] = {
	"death",
	"void_death",
	"jump",
	"pain25", "pain50", "pain75", "pain100",
	"walljump",
	"dash",
};

STATIC_ASSERT( ARRAY_COUNT( PLAYER_SOUND_NAMES ) == PlayerSound_Count );

static void FindPlayerSounds( PlayerModelMetadata * meta, Span< const char > dir ) {
	for( size_t i = 0; i < ARRAY_COUNT( meta->sounds ); i++ ) {
		TempAllocator temp = cls.frame_arena.temp();
		const char * path = temp( "{}/{}", dir, PLAYER_SOUND_NAMES[ i ] );
		meta->sounds[ i ] = StringHash( path );
	}
}

void InitPlayerModels() {
	num_player_models = 0;

	for( const char * path : AssetPaths() ) {
		if( num_player_models == ARRAY_COUNT( player_model_metadatas ) ) {
			Com_Printf( S_COLOR_RED "Too many player models!\n" );
			break;
		}

		Span< const char > ext = FileExtension( path );
		if( ext == ".glb" && StartsWith( path, "players/" ) ) {
			Span< const char > dir = BasePath( path );
			u64 hash = Hash64( StripExtension( path ) );

			PlayerModelMetadata * meta = &player_model_metadatas[ num_player_models ];
			meta->model = FindModel( StringHash( hash ) );

			TempAllocator temp = cls.frame_arena.temp();
			const char * config_path = temp( "{}/model.cfg", dir );
			if( !ParsePlayerModelConfig( meta, config_path ) )
				continue;

			FindPlayerSounds( meta, dir );

			player_models_hashtable.add( hash, num_player_models );
			num_player_models++;
		}
	}
}

static const PlayerModelMetadata * GetPlayerModelMetadata( StringHash name ) {
	u64 idx;
	if( !player_models_hashtable.get( name.hash, &idx ) )
		return NULL;
	return &player_model_metadatas[ idx ];
}

const PlayerModelMetadata * GetPlayerModelMetadata( int ent_num ) {
	return GetPlayerModelMetadata( "players/rigg/model" );
}

void CG_ResetPModels() {
	for( int i = 0; i < MAX_EDICTS; i++ ) {
		memset( &cg_entPModels[i].animState, 0, sizeof( pmodel_animationstate_t ) );
	}
	memset( &cg.weapon, 0, sizeof( cg.weapon ) );
}

/*
* CG_MoveToTag
* "move" tag must have an axis and origin set up. Use vec3_origin and axis_identity for "nothing"
*/
void CG_MoveToTag( Vec3 * move_origin,
				   mat3_t move_axis,
				   Vec3 space_origin,
				   const mat3_t space_axis,
				   Vec3 tag_origin,
				   const mat3_t tag_axis ) {
	mat3_t tmpAxis;

	*move_origin = space_origin;

	for( int i = 0; i < 3; i++ ) {
		Vec3 axis(
			space_axis[i * 3],
			space_axis[i * 3 + 1],
			space_axis[i * 3 + 2]
		);
		*move_origin += axis * tag_origin[i];
	}

	Matrix3_Multiply( move_axis, tag_axis, tmpAxis );
	Matrix3_Multiply( tmpAxis, space_axis, move_axis );
}

static float CG_OutlineScaleForDist( const InterpolatedEntity * e, float maxdist, float scale ) {
	Vec3 dir = e->origin - cg.view.origin;
	float dist = Length( dir ) * cg.view.fracDistFOV;
	if( dist > maxdist ) {
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
		case Weapon_Knife:
			return TORSO_HOLD_BLADE;
		case Weapon_Pistol:
		case Weapon_Laser:
		case Weapon_Deagle:
			return TORSO_HOLD_PISTOL;
		case Weapon_Shotgun:
		case Weapon_AssaultRifle:
		case Weapon_BubbleGun:
			return TORSO_HOLD_LIGHTWEAPON;
		case Weapon_BurstRifle:
		case Weapon_RocketLauncher:
		case Weapon_GrenadeLauncher:
			return TORSO_HOLD_HEAVYWEAPON;
		case Weapon_Railgun:
		case Weapon_Sniper:
		case Weapon_Rifle:
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

static PlayerModelAnimationSet CG_GetBaseAnims( SyncEntityState *state, Vec3 velocity ) {
	constexpr float MOVEDIREPSILON = 0.3f;
	constexpr float WALKEPSILON = 5.0f;
	constexpr float RUNEPSILON = 220.0f;

	uint32_t moveflags = 0;
	trace_t trace;

	if( state->type == ET_CORPSE ) {
		PlayerModelAnimationSet a;
		a.parts[ LOWER ] = ANIM_NONE;
		a.parts[ UPPER ] = ANIM_NONE;
		a.parts[ HEAD ] = ANIM_NONE;
		return a;
	}

	Vec3 mins = state->bounds.mins;
	Vec3 maxs = state->bounds.maxs;

	// determine if player is at ground, for walking or falling
	// this is not like having groundEntity, we are more generous with
	// the tracing size here to include small steps
	Vec3 point = state->origin;
	point.z -= 1.6f * STEPSIZE;
	client_gs.api.Trace( &trace, state->origin, mins, maxs, point, state->number, MASK_PLAYERSOLID, 0 );
	if( trace.ent == -1 || ( trace.fraction < 1.0f && !ISWALKABLEPLANE( &trace.plane ) && !trace.startsolid ) ) {
		moveflags |= ANIMMOVE_AIR;
	}

	// crouching : fixme? : it assumes the entity is using the player box sizes
	// if( maxs != playerbox_stand_maxs ) {
	// 	moveflags |= ANIMMOVE_DUCK;
	// }

	// find out the water level
	int waterlevel = GS_WaterLevel( &client_gs, state, mins, maxs );
	if( waterlevel >= 2 || ( waterlevel && ( moveflags & ANIMMOVE_AIR ) ) ) {
		moveflags |= ANIMMOVE_SWIM;
	}

	//find out what are the base movements the model is doing

	Vec2 hvel = velocity.xy();
	float xyspeedcheck = Length( hvel );
	Vec3 movedir = Vec3( SafeNormalize( hvel ), 0.0f );
	if( xyspeedcheck > WALKEPSILON ) {
		mat3_t viewaxis;
		Matrix3_FromAngles( Vec3( 0, state->angles.y, 0 ), viewaxis );

		// if it's moving to where is looking, it's moving forward
		if( Dot( movedir, FromQFAxis( viewaxis, AXIS_RIGHT ) ) > MOVEDIREPSILON ) {
			moveflags |= ANIMMOVE_RIGHT;
		} else if( -Dot( movedir, FromQFAxis( viewaxis, AXIS_RIGHT ) ) > MOVEDIREPSILON ) {
			moveflags |= ANIMMOVE_LEFT;
		}
		if( Dot( movedir, FromQFAxis( viewaxis, AXIS_FORWARD ) ) > MOVEDIREPSILON ) {
			moveflags |= ANIMMOVE_FRONT;
		} else if( -Dot( movedir, FromQFAxis( viewaxis, AXIS_FORWARD ) ) > MOVEDIREPSILON ) {
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
			float base = clip.duration - clip.loop_from;
			float loop_t = base == 0 ? 0 : PositiveMod( t - clip.loop_from, base );
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
static void CG_GetAnimationTimes( const PlayerModelMetadata * meta, pmodel_t * pmodel, int64_t curTime, float * lower_time, float * upper_time ) {
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

		times[ i ] = GetAnimationTime( meta, curTime, pmodel->animState.curAnims[ i ][ EVENT_CHANNEL ], false );
		if( times[ i ] == -1.0f ) {
			pmodel->animState.curAnims[ i ][ EVENT_CHANNEL ].anim = ANIM_NONE;
			times[ i ] = GetAnimationTime( meta, curTime, pmodel->animState.curAnims[ i ][ BASE_CHANNEL ], true );
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
	float speed;
	Vec3 leanAngles[PMODEL_PARTS];

	memset( leanAngles, 0, sizeof( leanAngles ) );

	Vec3 hvelocity = cent->animVelocity;
	hvelocity.z = 0.0f;

	float scale = 0.04f;

	if( ( speed = Length( hvelocity ) ) * scale > 1.0f ) {
		AnglesToAxis( Vec3( 0, cent->current.angles.y, 0 ), axis );

		float front = scale * Dot( hvelocity, FromQFAxis( axis, AXIS_FORWARD ) );
		if( front < -0.1f || front > 0.1f ) {
			leanAngles[LOWER].x += front;
			leanAngles[UPPER].x -= front * 0.25f;
			leanAngles[HEAD].x -= front * 0.5f;
		}

		float aside = ( front * 0.001f ) * cent->yawVelocity;

		if( aside ) {
			float asidescale = 75;
			leanAngles[LOWER].z -= aside * 0.5f * asidescale;
			leanAngles[UPPER].z += aside * 1.75f * asidescale;
			leanAngles[HEAD].z -= aside * 0.35f * asidescale;
		}

		float side = scale * Dot( hvelocity, FromQFAxis( axis, AXIS_RIGHT ) );

		if( side < -1 || side > 1 ) {
			leanAngles[LOWER].z -= side * 0.5f;
			leanAngles[UPPER].z += side * 0.5f;
			leanAngles[HEAD].z += side * 0.25f;
		}

		leanAngles[LOWER].x = Clamp( -45.0f, leanAngles[LOWER].x, 45.0f );
		leanAngles[LOWER].z = Clamp( -15.0f, leanAngles[LOWER].z, 15.0f );

		leanAngles[UPPER].x = Clamp( -45.0f, leanAngles[UPPER].x, 45.0f );
		leanAngles[UPPER].z = Clamp( -20.0f, leanAngles[UPPER].z, 20.0f );

		leanAngles[HEAD].x = Clamp( -45.0f, leanAngles[HEAD].x, 45.0f );
		leanAngles[HEAD].z = Clamp( -20.0f, leanAngles[HEAD].z, 20.0f );
	}

	for( int i = LOWER; i < PMODEL_PARTS; i++ ) {
		pmodel->angles[i].x = AngleNormalize180( pmodel->angles[i].x + leanAngles[i].x );
		pmodel->angles[i].y = AngleNormalize180( pmodel->angles[i].y + leanAngles[i].y );
		pmodel->angles[i].z = AngleNormalize180( pmodel->angles[i].z + leanAngles[i].z );
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
	pmodel_t * pmodel = &cg_entPModels[ cent->current.number ];

	// Spawning (teleported bit) forces nobacklerp and the interruption of EVENT_CHANNEL animations
	if( cent->current.teleported ) {
		CG_PModel_ClearEventAnimations( cent->current.number );
	}

	// update parts rotation angles
	for( int i = LOWER; i < PMODEL_PARTS; i++ )
		pmodel->oldangles[i] = pmodel->angles[i];

	if( cent->current.type == ET_CORPSE ) {
		cent->animVelocity = Vec3( 0.0f );
		cent->yawVelocity = 0;
	} else {
		// update smoothed velocities used for animations and leaning angles
		// rotational yaw velocity
		float adelta = AngleDelta( cent->current.angles.y, cent->prev.angles.y );
		adelta = Clamp( -35.0f, adelta, 35.0f );

		// smooth a velocity vector between the last snaps
		cent->lastVelocities[cg.frame.serverFrame & 3] = Vec4( cent->velocity.xy(), 0.0f, adelta );
		cent->lastVelocitiesFrames[cg.frame.serverFrame & 3] = cg.frame.serverFrame;

		int count = 0;
		cent->animVelocity = Vec3( 0.0f );
		cent->yawVelocity = 0;
		for( int i = cg.frame.serverFrame; ( i >= 0 ) && ( count < 3 ) && ( i == cent->lastVelocitiesFrames[i & 3] ); i-- ) {
			count++;
			cent->animVelocity += cent->lastVelocities[i & 3].xyz();
			cent->yawVelocity += cent->lastVelocities[i & 3].w;
		}

		// safety/static code analysis check
		if( count == 0 ) {
			count = 1;
		}

		cent->animVelocity = cent->animVelocity * ( 1.0f / (float)count );
		cent->yawVelocity /= (float)count;


		//
		// Calculate angles for each model part
		//

		// lower has horizontal direction, and zeroes vertical
		pmodel->angles[LOWER].x = 0;
		pmodel->angles[LOWER].y = cent->current.angles.y;
		pmodel->angles[LOWER].z = 0;

		// upper marks vertical direction (total angle, so it fits aim)
		if( cent->current.angles.x > 180 ) {
			pmodel->angles[UPPER].x = ( -360 + cent->current.angles.x );
		} else {
			pmodel->angles[UPPER].x = cent->current.angles.x;
		}

		pmodel->angles[UPPER].y = 0;
		pmodel->angles[UPPER].z = 0;

		// head adds a fraction of vertical angle again
		if( cent->current.angles.x > 180 ) {
			pmodel->angles[HEAD].x = ( -360 + cent->current.angles.x ) / 3;
		} else {
			pmodel->angles[HEAD].x = cent->current.angles.x / 3;
		}

		pmodel->angles[HEAD].y = 0;
		pmodel->angles[HEAD].z = 0;

		CG_PModel_LeanAngles( cent, pmodel );
	}


	// Spawning (teleported bit) forces nobacklerp and the interruption of EVENT_CHANNEL animations
	if( cent->current.teleported ) {
		for( int i = LOWER; i < PMODEL_PARTS; i++ )
			pmodel->oldangles[i] = pmodel->angles[i];
	}

	CG_UpdatePModelAnimations( cent );
}

static Quaternion EulerAnglesToQuaternion( EulerDegrees3 angles ) {
	float cp = cosf( Radians( angles.pitch ) * 0.5f );
	float sp = sinf( Radians( angles.pitch ) * 0.5f );
	float cy = cosf( Radians( angles.yaw ) * 0.5f );
	float sy = sinf( Radians( angles.yaw ) * 0.5f );
	float cr = cosf( Radians( angles.roll ) * 0.5f );
	float sr = sinf( Radians( angles.roll ) * 0.5f );

	return Quaternion(
		cp * cy * sr - sp * sy * cr,
		cp * sy * cr + sp * cy * sr,
		sp * cy * cr - cp * sy * sr,
		cp * cy * cr + sp * sy * sr
	);
}

static Mat4 TransformTag( const Model * model, const Mat4 & transform, const MatrixPalettes & pose, const PlayerModelMetadata::Tag & tag ) {
	return transform * model->transform * pose.node_transforms[ tag.node_idx ] * tag.transform;
}

void CG_DrawPlayer( centity_t * cent ) {
	pmodel_t * pmodel = &cg_entPModels[ cent->current.number ];
	const PlayerModelMetadata * meta = GetPlayerModelMetadata( cent->current.number );
	if( meta == NULL )
		return;

	// if viewer model, and casting shadows, offset the entity to predicted player position
	// for view and shadow accuracy

	if( ISVIEWERENTITY( cent->current.number ) ) {
		Vec3 origin;

		if( cg.view.playerPrediction ) {
			float backlerp = 1.0f - cg.lerpfrac;

			origin = cg.predictedPlayerState.pmove.origin - backlerp * cg.predictionError;

			CG_ViewSmoothPredictedSteps( &origin );
		}
		else {
			origin = cent->interpolated.origin;
		}

		cent->interpolated.origin = origin;
		cent->interpolated.origin2 = origin;
	}

	TempAllocator temp = cls.frame_arena.temp();

	float lower_time, upper_time;
	CG_GetAnimationTimes( meta, pmodel, cl.serverTime, &lower_time, &upper_time );
	Span< TRS > lower = SampleAnimation( &temp, meta->model, lower_time );
	Span< TRS > upper = SampleAnimation( &temp, meta->model, upper_time );
	MergeLowerUpperPoses( lower, upper, meta->model, meta->upper_root_node );

	// add skeleton effects (pose is unmounted yet)
	bool corpse = cent->current.type == ET_CORPSE;
	if( !corpse ) {
		Vec3 tmpangles;
		// if it's our client use the predicted angles
		if( cg.view.playerPrediction && ISVIEWERENTITY( cent->current.number ) ) {
			tmpangles.y = cg.predictedPlayerState.viewangles.y;
			tmpangles.x = 0;
			tmpangles.z = 0;
		}
		else {
			// apply interpolated LOWER angles to entity
			tmpangles = LerpAngles( pmodel->oldangles[LOWER], cg.lerpfrac, pmodel->angles[LOWER] );
		}

		AnglesToAxis( tmpangles, cent->interpolated.axis );

		// apply UPPER and HEAD angles to rotator nodes
		// also add rotations from velocity leaning
		{
			EulerDegrees3 angles = EulerDegrees3( LerpAngles( pmodel->oldangles[ UPPER ], cg.lerpfrac, pmodel->angles[ UPPER ] ) * 0.5f );
			Swap2( &angles.pitch, &angles.yaw ); // hack for rigg model

			Quaternion q = EulerAnglesToQuaternion( angles );
			lower[ meta->upper_rotator_nodes[ 0 ] ].rotation *= q;
			lower[ meta->upper_rotator_nodes[ 1 ] ].rotation *= q;
		}

		{
			EulerDegrees3 angles = EulerDegrees3( LerpAngles( pmodel->oldangles[ HEAD ], cg.lerpfrac, pmodel->angles[ HEAD ] ) );
			lower[ meta->head_rotator_node ].rotation *= EulerAnglesToQuaternion( angles );
		}
	}

	MatrixPalettes pose = ComputeMatrixPalettes( &temp, meta->model, lower );

	Mat4 transform = FromAxisAndOrigin( cent->interpolated.axis, cent->interpolated.origin ) * Mat4Scale( cent->interpolated.scale );

	Vec4 color = CG_TeamColorVec4( cent->current.team );
	if( corpse ) {
		color *= Vec4( 0.25f, 0.25f, 0.25f, 1.0 );
	}

	bool draw_model = !ISVIEWERENTITY( cent->current.number ) || cg.view.thirdperson;
	bool same_team = GS_TeamBasedGametype( &client_gs ) && cg.predictedPlayerState.team == cent->current.team;
	bool draw_silhouette = draw_model && ( ISREALSPECTATOR() || same_team );

	{
		DrawModelConfig config = { };
		config.draw_model.enabled = draw_model;
		config.draw_shadows.enabled = true;
		if( !corpse && draw_silhouette ) {
			config.draw_silhouette.enabled = true;
			config.draw_silhouette.silhouette_color = color;
		}

		if( draw_model ) {
			config.draw_outlines.enabled = true;
			float outline_height = CG_OutlineScaleForDist( &cent->interpolated, 4096, 1.0f );
			if( outline_height != 0.0f ) {
				config.draw_outlines.outline_height = outline_height;
				config.draw_outlines.outline_color = color * 0.5f;
			}
		}

		DrawModel( config, meta->model, transform, color, pose );
	}

	Mat4 inverse_scale = Mat4Scale( 1.0f / cent->interpolated.scale );

	// add weapon model
	{
		if( cent->current.weapon != Weapon_None ) {
			const Model * weapon_model = GetWeaponModelMetadata( cent->current.weapon )->model;
			if( weapon_model != NULL ) {
				Mat4 tag_transform = TransformTag( weapon_model, transform, pose, meta->tag_weapon ) * inverse_scale;

				DrawModelConfig config = { };
				config.draw_model.enabled = draw_model;
				config.draw_shadows.enabled = true;

				if( draw_silhouette ) {
					config.draw_silhouette.enabled = true;
					config.draw_silhouette.silhouette_color = color;
				}

				DrawModel( config, weapon_model, tag_transform, color );

				u8 muzzle;
				if( FindNodeByName( weapon_model, Hash32( "muzzle" ), &muzzle ) ) {
					pmodel->muzzle_transform = tag_transform * weapon_model->transform * weapon_model->nodes[ muzzle ].global_transform;
				}
				else {
					pmodel->muzzle_transform = tag_transform;
				}
			}
		}
	}

	// add bomb/hat
	{
		const Model * attached_model = FindModel( cent->current.model2 );
		if( attached_model != NULL ) {
			PlayerModelMetadata::Tag tag = meta->tag_bomb;
			if( cent->current.effects & EF_HAT )
				tag = meta->tag_hat;

			Mat4 tag_transform = TransformTag( meta->model, transform, pose, tag ) * inverse_scale;

			DrawModelConfig config = { };
			config.draw_model.enabled = draw_model;
			config.draw_shadows.enabled = true;
			config.draw_silhouette.enabled = draw_silhouette;
			config.draw_silhouette.silhouette_color = color;

			DrawModel( config, attached_model, tag_transform, vec4_white );
		}
	}
}
