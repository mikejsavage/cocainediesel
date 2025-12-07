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
#include "gameshared/collision.h"

constexpr u32 MAX_PLAYER_MODELS = 128;

pmodel_t cg_entPModels[ MAX_EDICTS ];

static PlayerModelMetadata player_model_metadatas[ MAX_PLAYER_MODELS ];
static u32 num_player_models;
static Hashtable< MAX_PLAYER_MODELS * 2 > player_models_hashtable;

static bool ParsePlayerModelConfig( PlayerModelMetadata * meta, Span< const char > filename ) {
	int num_clips = 1;

	Span< const char > cursor = AssetString( filename );
	if( cursor.ptr == NULL ) {
		Com_GGPrint( "Couldn't find animation script: {}", filename );
		return false;
	}

	const GLTFRenderData * model = FindGLTFRenderData( meta->model );
	if( model == NULL )
		return false;

	while( true ) {
		Span< const char > cmd = ParseToken( &cursor, Parse_DontStopOnNewLine );
		if( cmd == "" )
			break;

		if( cmd == "upper_rotator_joints" ) {
			Span< const char > node_name0 = ParseToken( &cursor, Parse_StopOnNewLine );
			FindNodeByName( model, StringHash( node_name0 ), &meta->upper_rotator_nodes[ 0 ] );

			Span< const char > node_name1 = ParseToken( &cursor, Parse_StopOnNewLine );
			FindNodeByName( model, StringHash( node_name1 ), &meta->upper_rotator_nodes[ 1 ] );
		}
		else if( cmd == "head_rotator_joint" ) {
			Span< const char > node_name = ParseToken( &cursor, Parse_StopOnNewLine );
			FindNodeByName( model, StringHash( node_name ), &meta->head_rotator_node );
		}
		else if( cmd == "upper_root_joint" ) {
			Span< const char > node_name = ParseToken( &cursor, Parse_StopOnNewLine );
			FindNodeByName( model, StringHash( node_name ), &meta->upper_root_node );
		}
		else if( cmd == "tag" ) {
			Span< const char > node_name = ParseToken( &cursor, Parse_StopOnNewLine );
			u8 node_idx;
			if( FindNodeByName( model, StringHash( node_name ), &node_idx ) ) {
				Span< const char > tag_name = ParseToken( &cursor, Parse_StopOnNewLine );
				PlayerModelMetadata::Tag * tag = &meta->tag_bomb;
				if( tag_name == "tag_hat" )
					tag = &meta->tag_hat;
				else if( tag_name == "tag_mask" )
					tag = &meta->tag_mask;
				else if( tag_name == "tag_weapon" )
					tag = &meta->tag_weapon;
				else if( tag_name == "tag_gadget" )
					tag = &meta->tag_gadget;

				float forward = ParseFloat( &cursor, 0.0f, Parse_StopOnNewLine );
				float right = ParseFloat( &cursor, 0.0f, Parse_StopOnNewLine );
				float up = ParseFloat( &cursor, 0.0f, Parse_StopOnNewLine );
				float pitch = ParseFloat( &cursor, 0.0f, Parse_StopOnNewLine );
				float yaw = ParseFloat( &cursor, 0.0f, Parse_StopOnNewLine );
				float roll = ParseFloat( &cursor, 0.0f, Parse_StopOnNewLine );

				tag->node_idx = node_idx;
				tag->transform = Mat4Translation( forward, right, up ) * Mat4Rotation( EulerDegrees3( pitch, yaw, roll ) );
			}
			else {
				Com_GGPrint( "{}: Unknown node name: {}", filename, node_name );
				for( int i = 0; i < 7; i++ ) {
					ParseToken( &cursor, Parse_StopOnNewLine );
				}
			}
		}
		else if( cmd == "clip" ) {
			if( num_clips == PMODEL_TOTAL_ANIMATIONS ) {
				Com_GGPrint( "{}: Too many animations", filename );
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
		Com_GGPrint( "{}: Not enough animations ({})", filename, num_clips );
		return false;
	}

	meta->clips[ ANIM_NONE ].start_time = 0;
	meta->clips[ ANIM_NONE ].duration = 0;
	meta->clips[ ANIM_NONE ].loop_from = 0;

	return true;
}

static bool FindTag( const GLTFRenderData * model, PlayerModelMetadata::Tag * tag, StringHash node_name, Span< const char > model_name ) {
	u8 idx;
	if( !FindNodeByName( model, node_name, &idx ) ) {
		Com_GGPrint( S_COLOR_YELLOW "Can't find node {} in {}", node_name, model_name );
		return false;
	}

	tag->node_idx = idx;
	tag->transform = Mat3x4::Identity(); // TODO: use node local transform

	return true;
}

static bool LoadPlayerModelMetadata( PlayerModelMetadata * meta, Span< const char > model_name ) {
	const GLTFRenderData * model = FindGLTFRenderData( meta->model );
	if( model == NULL )
		return false;

	bool ok = true;
	ok = ok && FindTag( model, &meta->tag_bomb, "bomb", model_name );
	ok = ok && FindTag( model, &meta->tag_hat, "hat", model_name );
	ok = ok && FindTag( model, &meta->tag_mask, "mask", model_name );
	ok = ok && FindTag( model, &meta->tag_weapon, "weapon", model_name );
	ok = ok && FindTag( model, &meta->tag_gadget, "gadget", model_name );
	return ok;
}

static constexpr Span< const char > PLAYER_SOUND_NAMES[] = {
	"death",
	"void_death",
	"smackdown",
	"jump",
	"pain25", "pain50", "pain75", "pain100",
	"walljump",
	"dash",
};

STATIC_ASSERT( ARRAY_COUNT( PLAYER_SOUND_NAMES ) == PlayerSound_Count );

static void FindPlayerSounds( PlayerModelMetadata * meta, Span< const char > dir ) {
	for( size_t i = 0; i < ARRAY_COUNT( meta->sounds ); i++ ) {
		TempAllocator temp = cls.frame_arena.temp();
		Span< const char > path = temp.sv( "{}/{}", dir, PLAYER_SOUND_NAMES[ i ] );
		meta->sounds[ i ] = StringHash( path );
	}
}

void InitPlayerModels() {
	num_player_models = 0;

	for( Span< const char > path : AssetPaths() ) {
		if( num_player_models == ARRAY_COUNT( player_model_metadatas ) ) {
			Com_Printf( S_COLOR_RED "Too many player models!\n" );
			break;
		}

		Span< const char > ext = FileExtension( path );
		if( ext == ".glb" && StartsWith( path, "players/" ) ) {
			Span< const char > dir = BasePath( path );
			u64 hash = Hash64( StripExtension( path ) );

			PlayerModelMetadata * meta = &player_model_metadatas[ num_player_models ];
			*meta = { };
			meta->model = StringHash( hash );

			TempAllocator temp = cls.frame_arena.temp();
			Span< const char > config_path = temp.sv( "{}/model.cfg", dir );
			if( AssetString( config_path ).ptr == NULL ) {
				if( !LoadPlayerModelMetadata( meta, path ) ) {
					continue;
				}
			}
			else if( !ParsePlayerModelConfig( meta, config_path ) ) {
				continue;
			}

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
	const SyncEntityState * ent = &cg_entities[ ent_num ].current;
	switch( ent->perk ) {
		case Perk_Jetpack:
			return GetPlayerModelMetadata( "players/jetpack/model" );
		default:
			return GetPlayerModelMetadata( "players/rigg/model" );
	}
}

void CG_ResetPModels() {
	for( int i = 0; i < MAX_EDICTS; i++ ) {
		memset( &cg_entPModels[i].animState, 0, sizeof( pmodel_animationstate_t ) );
	}
	memset( &cg.weapon, 0, sizeof( cg.weapon ) );
}

static float CG_OutlineScaleForDist( const InterpolatedEntity * e ) {
	Vec3 dir = e->origin - cg.view.origin;
	float dist = Length( dir ) * cg.view.fracDistFOV;
	if( dist > 4096.0f ) {
		return 0.0f;
	}

	if( dist < 64.0f ) {
		return 0.14f;
	}
	if( dist < 128.0f ) {
		return 0.30f;
	}
	if( dist < 256.0f ) {
		return 0.42f;
	}
	if( dist < 512.0f ) {
		return 0.56f;
	}
	if( dist < 768.0f ) {
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
#define ANIMMOVE_AIR        ( 1 << 7 )  // Player is at air, but not jumping
#define ANIMMOVE_DEAD       ( 1 << 8 )  // Player is a corpse

static int CG_MoveFlagsToUpperAnimation( uint32_t moveflags, int carried_weapon ) {
	if( moveflags & ANIMMOVE_DEAD )
		return ANIM_NONE;

	switch( carried_weapon ) {
		case Weapon_Knife:
			return TORSO_HOLD_BLADE;
		case Weapon_9mm:
		case Weapon_Deagle:
			return TORSO_HOLD_PISTOL;
		case Weapon_Shotgun:
		case Weapon_Assault:
		case Weapon_Bubble:
			return TORSO_HOLD_LIGHTWEAPON;
		case Weapon_Burst:
		case Weapon_Bazooka:
		case Weapon_Launcher:
			return TORSO_HOLD_HEAVYWEAPON;
		case Weapon_Rail:
		case Weapon_Sniper:
		case Weapon_Rifle:
			return TORSO_HOLD_AIMWEAPON;
	}

	return TORSO_HOLD_LIGHTWEAPON;
}

static int CG_MoveFlagsToLowerAnimation( uint32_t moveflags ) {
	if( moveflags & ANIMMOVE_DEAD )
		return ANIM_NONE;

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

static PlayerModelAnimationSet CG_GetBaseAnims( const SyncEntityState * state, Vec3 velocity ) {
	constexpr float MOVEDIREPSILON = 0.3f;
	constexpr float WALKEPSILON = 5.0f;
	constexpr float RUNEPSILON = 220.0f;

	u32 moveflags = 0;

	if( state->type == ET_CORPSE ) {
		PlayerModelAnimationSet a;
		a.parts[ LOWER ] = ANIM_NONE;
		a.parts[ UPPER ] = ANIM_NONE;
		a.parts[ HEAD ] = ANIM_NONE;
		return a;
	}

	MinMax3 bounds = EntityBounds( ClientCollisionModelStorage(), state );

	// determine if player is at ground, for walking or falling
	// this is not like having groundEntity, we are more generous with
	// the tracing size here to include small steps
	Vec3 point = state->origin;
	point.z -= 1.6f * STEPSIZE;
	trace_t trace = client_gs.api.Trace( state->origin, bounds, point, state->number, Solid_PlayerClip, 0 );
	if( trace.HitNothing() || ( trace.HitSomething() && !ISWALKABLEPLANE( trace.normal ) ) ) {
		moveflags |= ANIMMOVE_AIR;
	}

	// crouching : fixme? : it assumes the entity is using the player box sizes
	// if( maxs != playerbox_stand_maxs ) {
	// 	moveflags |= ANIMMOVE_DUCK;
	// }

	//find out what are the base movements the model is doing

	Vec2 hvel = velocity.xy();
	float xyspeedcheck = Length( hvel );
	Vec3 movedir = Vec3( SafeNormalize( hvel ), 0.0f );
	if( xyspeedcheck > WALKEPSILON ) {
		mat3_t viewaxis;
		Matrix3_FromAngles( state->angles.yaw_only(), viewaxis );

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
	EulerDegrees3 leanAngles[PMODEL_PARTS];

	memset( leanAngles, 0, sizeof( leanAngles ) );

	Vec3 hvelocity = cent->animVelocity;
	hvelocity.z = 0.0f;

	float scale = 0.04f;

	if( ( speed = Length( hvelocity ) ) * scale > 1.0f ) {
		AnglesToAxis( cent->current.angles.yaw_only(), axis );

		float front = scale * Dot( hvelocity, FromQFAxis( axis, AXIS_FORWARD ) );
		if( front < -0.1f || front > 0.1f ) {
			leanAngles[LOWER].pitch += front;
			leanAngles[UPPER].pitch -= front * 0.25f;
			leanAngles[HEAD].pitch -= front * 0.5f;
		}

		float aside = ( front * 0.001f ) * cent->yawVelocity;

		if( aside ) {
			float asidescale = 75;
			leanAngles[LOWER].roll -= aside * 0.5f * asidescale;
			leanAngles[UPPER].roll += aside * 1.75f * asidescale;
			leanAngles[HEAD].roll -= aside * 0.35f * asidescale;
		}

		float side = scale * Dot( hvelocity, FromQFAxis( axis, AXIS_RIGHT ) );

		if( side < -1 || side > 1 ) {
			leanAngles[LOWER].roll -= side * 0.5f;
			leanAngles[UPPER].roll += side * 0.5f;
			leanAngles[HEAD].roll += side * 0.25f;
		}

		leanAngles[LOWER].pitch = Clamp( -45.0f, leanAngles[LOWER].pitch, 45.0f );
		leanAngles[LOWER].roll = Clamp( -15.0f, leanAngles[LOWER].roll, 15.0f );

		leanAngles[UPPER].pitch = Clamp( -45.0f, leanAngles[UPPER].pitch, 45.0f );
		leanAngles[UPPER].roll = Clamp( -20.0f, leanAngles[UPPER].roll, 20.0f );

		leanAngles[HEAD].pitch = Clamp( -45.0f, leanAngles[HEAD].pitch, 45.0f );
		leanAngles[HEAD].roll = Clamp( -20.0f, leanAngles[HEAD].roll, 20.0f );
	}

	for( int i = LOWER; i < PMODEL_PARTS; i++ ) {
		pmodel->angles[i].pitch = AngleNormalize180( pmodel->angles[i].pitch + leanAngles[i].pitch );
		pmodel->angles[i].yaw = AngleNormalize180( pmodel->angles[i].yaw + leanAngles[i].yaw );
		pmodel->angles[i].roll = AngleNormalize180( pmodel->angles[i].roll + leanAngles[i].roll );
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
	for( int i = LOWER; i < PMODEL_PARTS; i++ ) {
		pmodel->oldangles[i] = pmodel->angles[i];
	}

	if( cent->current.type == ET_CORPSE ) {
		cent->animVelocity = Vec3( 0.0f );
		cent->yawVelocity = 0;
	} else {
		// update smoothed velocities used for animations and leaning angles
		// rotational yaw velocity
		float adelta = AngleDelta( cent->current.angles.yaw, cent->prev.angles.yaw );
		adelta = Clamp( -35.0f, adelta, 35.0f );

		// smooth a velocity vector between the last snaps
		cent->lastVelocities[cg.frame.serverFrame % ARRAY_COUNT( cent->lastVelocities )] = Vec4( cent->velocity.xy(), 0.0f, adelta );
		cent->lastVelocitiesFrames[cg.frame.serverFrame % ARRAY_COUNT( cent->lastVelocitiesFrames )] = cg.frame.serverFrame;

		int count = 0;
		cent->animVelocity = Vec3( 0.0f );
		cent->yawVelocity = 0;
		for( int i = cg.frame.serverFrame; ( i >= 0 ) && ( count < 3 ) && ( i == cent->lastVelocitiesFrames[i % ARRAY_COUNT( cent->lastVelocitiesFrames )] ); i-- ) {
			count++;
			cent->animVelocity += cent->lastVelocities[i % ARRAY_COUNT( cent->lastVelocities )].xyz();
			cent->yawVelocity += cent->lastVelocities[i % ARRAY_COUNT( cent->lastVelocities )].w;
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

		pmodel->angles[LOWER] = cent->current.angles.yaw_only();
		pmodel->angles[UPPER] = EulerDegrees3( AngleNormalize180( cent->current.angles.pitch ), 0.0f, 0.0f );
		pmodel->angles[HEAD] = EulerDegrees3( AngleNormalize180( cent->current.angles.pitch ) / 3.0f, 0.0f, 0.0f );

		CG_PModel_LeanAngles( cent, pmodel );
	}


	// Spawning (teleported bit) forces nobacklerp and the interruption of EVENT_CHANNEL animations
	if( cent->current.teleported ) {
		for( int i = LOWER; i < PMODEL_PARTS; i++ ) {
			pmodel->oldangles[i] = pmodel->angles[i];
		}
	}

	CG_UpdatePModelAnimations( cent );
}

static Mat3x4 TagTransform( const GLTFRenderData * model, const MatrixPalettes & pose, const PlayerModelMetadata::Tag & tag ) {
	if( pose.node_transforms.ptr != NULL ) {
		return model->transform * pose.node_transforms[ tag.node_idx ] * tag.transform;
	}

	return model->transform * model->nodes[ tag.node_idx ].global_transform * tag.transform;
}

static Mat3x4 InverseNodeTransform( const GLTFRenderData * model, StringHash node ) {
	u8 idx;
	return FindNodeByName( model, node, &idx ) ? model->nodes[ idx ].inverse_global_transform : Mat3x4::Identity();
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

	const GLTFRenderData * model = FindGLTFRenderData( meta->model );
	bool corpse = cent->current.type == ET_CORPSE;
	MatrixPalettes pose = { };

	if( model->animations.n > 0 ) {
		float lower_time, upper_time;
		CG_GetAnimationTimes( meta, pmodel, cl.serverTime, &lower_time, &upper_time );
		Span< Transform > lower = SampleAnimation( &temp, model, lower_time );
		Span< Transform > upper = SampleAnimation( &temp, model, upper_time );
		MergeLowerUpperPoses( lower, upper, model, meta->upper_root_node );

		if( !corpse ) {
			EulerDegrees3 tmpangles;
			// if it's our client use the predicted angles
			if( cg.view.playerPrediction && ISVIEWERENTITY( cent->current.number ) ) {
				tmpangles = cg.predictedPlayerState.viewangles.yaw_only();
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

				Quaternion q = EulerDegrees3ToQuaternion( angles );
				lower[ meta->upper_rotator_nodes[ 0 ] ].rotation *= q;
				lower[ meta->upper_rotator_nodes[ 1 ] ].rotation *= q;
			}

			{
				EulerDegrees3 angles = EulerDegrees3( LerpAngles( pmodel->oldangles[ HEAD ], cg.lerpfrac, pmodel->angles[ HEAD ] ) );
				lower[ meta->head_rotator_node ].rotation *= EulerDegrees3ToQuaternion( angles );
			}
		}

		pose = ComputeMatrixPalettes( &temp, model, lower );
	}

	Mat3x4 unscaled_transform = FromAxisAndOrigin( cent->interpolated.axis, cent->interpolated.origin );
	Mat3x4 transform = unscaled_transform * Mat4Scale( cent->interpolated.scale );

	Vec4 color = CG_TeamColorVec4( cent->current.team );
	if( corpse ) {
		color *= Vec4( 0.25f, 0.25f, 0.25f, 1.0f );
	}

	bool draw_model = !ISVIEWERENTITY( cent->current.number ) || cg.view.thirdperson;
	bool same_team = cg.predictedPlayerState.team == cent->current.team;
	bool draw_silhouette = !corpse && draw_model && ( ISREALSPECTATOR() || same_team );

	{
		DrawModelConfig config = { };
		config.draw_model.enabled = draw_model;
		config.draw_shadows.enabled = true;
		if( draw_silhouette ) {
			config.draw_silhouette.enabled = true;
			config.draw_silhouette.silhouette_color = color;
		}

		if( draw_model && cent->current.perk != Perk_Jetpack ) {
			float outline_height = CG_OutlineScaleForDist( &cent->interpolated );
			if( outline_height > 0.0f ) {
				config.draw_outlines.enabled = true;
				config.draw_outlines.outline_height = outline_height;
				config.draw_outlines.outline_color = color * 0.5f;
			}
		}

		DrawGLTFModel( config, model, transform, color, pose );
	}

	// add weapon model
	{
		const GLTFRenderData * weapon_model = GetEquippedItemRenderData( &cent->current );
		if( weapon_model != NULL ) {
			PlayerModelMetadata::Tag tag = cent->current.gadget == Gadget_None ? meta->tag_weapon : meta->tag_gadget;
			Mat3x4 weapon_transform = unscaled_transform * TagTransform( model, pose, tag ) * InverseNodeTransform( weapon_model, "hand" );

			DrawModelConfig config = { };
			config.draw_model.enabled = draw_model;
			config.draw_shadows.enabled = true;

			if( draw_silhouette ) {
				config.draw_silhouette.enabled = true;
				config.draw_silhouette.silhouette_color = color;
			}

			DrawGLTFModel( config, weapon_model, weapon_transform, color );

			u8 muzzle;
			if( FindNodeByName( weapon_model, "muzzle", &muzzle ) ) {
				pmodel->muzzle_transform = weapon_transform * weapon_model->transform * weapon_model->nodes[ muzzle ].global_transform;
			}
			else {
				pmodel->muzzle_transform = weapon_transform;
			}
		}
	}

	// add bomb/hat
	{
		const GLTFRenderData * attached_model = FindGLTFRenderData( cent->current.model2 );
		if( attached_model != NULL ) {
			PlayerModelMetadata::Tag tag = meta->tag_bomb;
			if( cent->current.effects & EF_HAT )
				tag = meta->tag_hat;

			Mat3x4 tag_transform = unscaled_transform * TagTransform( model, pose, tag );

			DrawModelConfig config = { };
			config.draw_model.enabled = draw_model;
			config.draw_shadows.enabled = true;
			config.draw_silhouette.enabled = draw_silhouette;
			config.draw_silhouette.silhouette_color = color;

			DrawGLTFModel( config, attached_model, tag_transform, white.linear );
		}
	}

	// add mask
	{
		const GLTFRenderData * mask_model = FindGLTFRenderData( cent->current.mask );
		if( mask_model != NULL ) {
			PlayerModelMetadata::Tag tag = meta->tag_mask;

			Mat3x4 tag_transform = unscaled_transform * TagTransform( model, pose, tag );

			DrawModelConfig config = { };
			config.draw_model.enabled = draw_model;
			config.draw_shadows.enabled = true;
			config.draw_silhouette.enabled = draw_silhouette;
			config.draw_silhouette.silhouette_color = color;

			DrawGLTFModel( config, mask_model, tag_transform, white.linear );
		}
	}
}
