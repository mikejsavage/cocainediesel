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
#include "client/renderer/renderer.h"

#define MAX_LOCAL_ENTITIES  512

enum LocalEntityType {
	LE_FREE,
	LE_NO_FADE,
	LE_RGB_FADE,
	LE_ALPHA_FADE,
	LE_SCALE_ALPHA_FADE,
	LE_INVERSESCALE_ALPHA_FADE,
	LE_DASH_SCALE,
	LE_PUFF_SCALE,
	LE_PUFF_SHRINK,
};

struct LocalEntity {
	struct LocalEntity *prev, *next;

	LocalEntityType type;

	entity_t ent;
	Vec4 color;

	unsigned int start;

	float light;
	Vec3 lightcolor;

	Vec3 velocity;
	Vec3 angles;
	Vec3 accel;

	int frames;

	MatrixPalettes pose;
};

static LocalEntity cg_localents[MAX_LOCAL_ENTITIES];
static LocalEntity cg_localents_headnode, *cg_free_lents;

/*
* CG_ClearLocalEntities
*/
void CG_ClearLocalEntities( void ) {
	memset( cg_localents, 0, sizeof( cg_localents ) );

	// link local entities
	cg_free_lents = cg_localents;
	cg_localents_headnode.prev = &cg_localents_headnode;
	cg_localents_headnode.next = &cg_localents_headnode;
	for( int i = 0; i < MAX_LOCAL_ENTITIES - 1; i++ )
		cg_localents[i].next = &cg_localents[i + 1];
}

/*
* CG_AllocLocalEntity
*/
static LocalEntity *CG_AllocLocalEntity( LocalEntityType type, Vec4 color ) {
	LocalEntity *le;

	if( cg_free_lents ) { // take a free decal if possible
		le = cg_free_lents;
		cg_free_lents = le->next;
	} else {              // grab the oldest one otherwise
		le = cg_localents_headnode.prev;
		le->prev->next = le->next;
		le->next->prev = le->prev;
	}

	*le = { };
	le->type = type;
	le->start = cl.serverTime;
	le->color = color;

	switch( le->type ) {
		case LE_NO_FADE:
			break;
		case LE_RGB_FADE:
			le->ent.color.a = u8( 255 * color.w );
			break;
		case LE_SCALE_ALPHA_FADE:
		case LE_INVERSESCALE_ALPHA_FADE:
		case LE_PUFF_SHRINK:
			le->ent.color = RGBA8( color );
			break;
		case LE_PUFF_SCALE:
			le->ent.color = RGBA8( color );
			break;
		case LE_ALPHA_FADE:
			le->ent.color = RGBA8( color );
			le->ent.color.a = 0;
			break;
		default:
			break;
	}

	// put the decal at the start of the list
	le->prev = &cg_localents_headnode;
	le->next = cg_localents_headnode.next;
	le->next->prev = le;
	le->prev->next = le;

	return le;
}

/*
* CG_FreeLocalEntity
*/
static void CG_FreeLocalEntity( LocalEntity *le ) {
	if( le->pose.joint_poses.ptr ) {
		FREE( sys_allocator, le->pose.joint_poses.ptr );
		FREE( sys_allocator, le->pose.skinning_matrices.ptr );
		le->pose = { };
	}

	// remove from linked active list
	le->prev->next = le->next;
	le->next->prev = le->prev;

	// insert into linked free list
	le->next = cg_free_lents;
	cg_free_lents = le;
}

/*
* CG_AllocModel
*/
static LocalEntity *CG_AllocModel( LocalEntityType type, Vec3 origin, Vec3 angles, int frames,
								 Vec4 color, float light, float lr, float lg, float lb, const Model *model, const Material * material ) {
	LocalEntity * le = CG_AllocLocalEntity( type, color );
	le->frames = frames;
	le->light = light;
	le->lightcolor.x = lr;
	le->lightcolor.y = lg;
	le->lightcolor.z = lb;

	le->ent.model = model;
	le->ent.scale = 1.0f;

	le->angles = angles;
	AnglesToAxis( angles, le->ent.axis );
	le->ent.origin = origin;

	return le;
}

/*
* CG_AllocSprite
*/
static LocalEntity *CG_AllocSprite( LocalEntityType type, Vec3 origin, float radius, int frames,
								  Vec4 color, float light, float lr, float lg, float lb, const Material * material ) {
	LocalEntity * le = CG_AllocLocalEntity( type, color );
	le->frames = frames;
	le->light = light;
	le->lightcolor.x = lr;
	le->lightcolor.y = lg;
	le->lightcolor.z = lb;

	le->ent.radius = radius;
	le->ent.scale = 1.0f;

	Matrix3_Identity( le->ent.axis );
	le->ent.origin = origin;

	return le;
}

void CG_EBBeam( Vec3 start, Vec3 end, Vec4 team_color ) {
	AddPersistentBeam( start, end, 16.0f, team_color, cgs.media.shaderEBBeam, 0.25f, 0.1f );
	CG_EBIonsTrail( start, end, team_color );
}

/*
* CG_ImpactSmokePuff
*/
void CG_ImpactSmokePuff( Vec3 origin, Vec3 dir, float radius, float alpha, int time, int speed ) {
#define SMOKEPUFF_MAXVIEWDIST 700
	const Material * material = cgs.media.shaderSmokePuff;
	Vec3 local_origin, local_dir;

	if( CG_PointContents( origin ) & MASK_WATER ) {
		return;
	}

	if( Length( cg.view.origin - origin ) * cg.view.fracDistFOV > SMOKEPUFF_MAXVIEWDIST ) {
		return;
	}

	if( !Length( dir ) ) {
		local_dir = -FromQFAxis( cg.view.axis, AXIS_FORWARD );
	} else {
		local_dir = Normalize( dir );
	}

	// offset the origin by half of the radius
	local_origin = origin + local_dir * ( radius * 0.5f );

	LocalEntity * le = CG_AllocSprite( LE_SCALE_ALPHA_FADE, local_origin, radius + random_float11( &cls.rng ), time,
						 Vec4( 1, 1, 1, alpha ), 0, 0, 0, 0, material );

	le->ent.rotation = random_float01( &cls.rng ) * 360;
	le->velocity = local_dir * ( speed );
}

/*
* CG_BubbleTrail
*/
void CG_BubbleTrail( Vec3 start, Vec3 end, int dist ) {
	Vec3 move, vec;
	move = start;
	vec = end - start;
	float len = Length( vec );
	vec = Normalize( vec );
	if( !len ) {
		return;
	}

	vec = vec * ( dist );
	const Material * material = cgs.media.shaderWaterBubble;

	for( int i = 0; i < len; i += dist ) {
		LocalEntity * le = CG_AllocSprite( LE_ALPHA_FADE, move, 3, 10,
							 vec4_white,
							 0, 0, 0, 0,
							 material );
		le->velocity = Vec3( random_float11( &cls.rng ) * 5, random_float11( &cls.rng ) * 5, random_float11( &cls.rng ) * 5 + 6 );
		move = move + vec;
	}
}

/*
* CG_PlasmaExplosion
*/
void CG_PlasmaExplosion( Vec3 pos, Vec3 dir, Vec4 team_color ) {
	// CG_ImpactPuffParticles( pos, dir, 15, 0.75f, color[0], color[1], color[2], color[3], NULL );

	// CG_SpawnDecal( pos, dir, random_float01( &cls.rng ) * 360, 16,
	// 			   color[0], color[1], color[2], color[3],
	// 			   4, 1, true,
	// 			   cgs.media.shaderPlasmaMark );
	CG_ParticlePlasmaExplosionEffect( pos, dir, team_color.xyz() );

	S_StartFixedSound( cgs.media.sfxPlasmaHit, pos, CHAN_AUTO, 1.0f );
}

void CG_BubbleExplosion( Vec3 pos, Vec4 team_color ) {
	// CG_ImpactPuffParticles( pos, dir, 15, 0.75f, color[0], color[1], color[2], color[3], NULL );

	// CG_SpawnDecal( pos, dir, random_float01( &cls.rng ) * 360, 16,
	// 			   color[0], color[1], color[2], color[3],
	// 			   4, 1, true,
	// 			   cgs.media.shaderPlasmaMark );

	CG_ParticleBubbleExplosionEffect( pos, team_color.xyz() );

	S_StartFixedSound( cgs.media.sfxBubbleHit, pos, CHAN_AUTO, 1.0f );
}

void CG_EBImpact( Vec3 pos, Vec3 dir, int surfFlags, Vec4 team_color ) {
	if( surfFlags & ( SURF_SKY | SURF_NOMARKS | SURF_NOIMPACT ) ) {
		return;
	}

	// CG_SpawnDecal( pos, dir, 0, 3, color[0], color[1], color[2], color[3],
	// 	10, 1, true, cgs.media.shaderEBImpact );

	Vec3 angles = VecToAngles( dir );

	LocalEntity * le = CG_AllocModel( LE_INVERSESCALE_ALPHA_FADE, pos, angles, 6, // 6 is time
						vec4_white,
						250, 0.75, 0.75, 0.75, //white dlight
						cgs.media.modElectroBoltWallHit, NULL );

	le->ent.rotation = random_float01( &cls.rng ) * 360;
	le->ent.scale = 1.5f;

	// CG_ImpactPuffParticles( pos, dir, 15, 0.75f, color[0], color[1], color[2], color[3], NULL );

	S_StartFixedSound( cgs.media.sfxElectroboltHit, pos, CHAN_AUTO, 1.0f );
}

/*
* CG_RocketExplosionMode
*/
void CG_RocketExplosionMode( Vec3 pos, Vec3 dir, Vec4 team_color ) {
	// CG_SpawnDecal( pos, dir, random_float01( &cls.rng ) * 360, radius * 0.5, 1, 1, 1, 1, 10, 1, false, cgs.media.shaderExplosionMark );

	// Explosion particles
	CG_ParticleRocketExplosionEffect( pos, dir, team_color.xyz() );

	S_StartFixedSound( cgs.media.sfxRocketLauncherHit, pos, CHAN_AUTO, 1.0f );
}

/*
* CG_BladeImpact
*/
void CG_BladeImpact( Vec3 pos, Vec3 dir ) {
	LocalEntity *le;
	Vec3 angles;
	Vec3 end;
	Vec3 local_pos, local_dir;
	trace_t trace;

	//find what are we hitting
	local_pos = pos;
	local_dir = Normalize( dir );
	end = pos + local_dir * ( -1.0 );
	CG_Trace( &trace, local_pos, Vec3( 0.0f ), Vec3( 0.0f ), end, cg.view.POVent, MASK_SHOT );
	if( trace.fraction == 1.0 ) {
		return;
	}

	angles = VecToAngles( local_dir );

	if( trace.surfFlags & SURF_FLESH ||
		( trace.ent > 0 && cg_entities[trace.ent].current.type == ET_PLAYER )
		|| ( trace.ent > 0 && cg_entities[trace.ent].current.type == ET_CORPSE ) ) {
		le = CG_AllocModel( LE_ALPHA_FADE, pos, angles, 3, //3 frames for weak
							vec4_white,
							0, 0, 0, 0, //dlight
							cgs.media.modBladeWallHit, NULL );
		le->ent.rotation = random_float01( &cls.rng ) * 360;
		le->ent.scale = 1.0f;

		S_StartFixedSound( cgs.media.sfxBladeFleshHit, pos, CHAN_AUTO, 1.0f );
	} else if( trace.surfFlags & SURF_DUST ) {
		// throw particles on dust
		// CG_ParticleEffect( trace.endpos, trace.plane.normal, 0.30f, 0.30f, 0.25f, 30 );

		//fixme? would need a dust sound
		S_StartFixedSound( cgs.media.sfxBladeWallHit, pos, CHAN_AUTO, 1.0f );
	} else {
		le = CG_AllocModel( LE_ALPHA_FADE, pos, angles, 3, //3 frames for weak
							vec4_white,
							0, 0, 0, 0, //dlight
							cgs.media.modBladeWallHit, NULL );
		le->ent.rotation = random_float01( &cls.rng ) * 360;
		le->ent.scale = 1.0f;

		// CG_ParticleEffect( trace.endpos, trace.plane.normal, 0.30f, 0.30f, 0.25f, 15 );

		S_StartFixedSound( cgs.media.sfxBladeWallHit, pos, CHAN_AUTO, 1.0f );
		if( !( trace.surfFlags & SURF_NOMARKS ) ) {
			// CG_SpawnDecal( pos, dir, random_float01( &cls.rng ) * 45, 8, 1, 1, 1, 1, 10, 1, false, cgs.media.shaderBladeMark );
		}
	}
}

/*
* CG_LasertGunImpact
*/
void CG_LaserGunImpact( Vec3 pos, Vec3 laser_dir, RGBA8 color ) {
	entity_t ent;
	Vec3 ndir;
	Vec3 angles;

	memset( &ent, 0, sizeof( ent ) );
	ent.origin = pos;
	ent.scale = 1.45f;
	ent.color = color;
	ent.model = cgs.media.modLasergunWallExplo;
	ndir = -laser_dir;
	angles = VecToAngles( ndir );
	AnglesToAxis( angles, ent.axis );

	// trap_R_AddEntityToScene( &ent );
}

/*
* CG_ProjectileTrail
*/
void CG_ProjectileTrail( const centity_t * cent ) {
	float radius = 8;
	float alpha = 0.45f;

	// didn't move
	Vec3 vec = cent->ent.origin - cent->trailOrigin;
	float len = Length( vec );
	if( len == 0 )
		return;

	vec = Normalize( vec );

	ParticleEmitter emitter = { };

	emitter.position = cent->ent.origin;
	emitter.position_distribution.type = RandomDistribution3DType_Line;
	emitter.position_distribution.line.end = cent->trailOrigin;

	emitter.start_speed = 5.0f;
	emitter.end_speed = 5.0f;

	emitter.start_color = Vec4( CG_TeamColorVec4( cent->current.team ).xyz(), 0.5f );
	emitter.end_color = Lerp( emitter.start_color.xyz(), 0.2f, Vec3( 1.0f ) );

	emitter.start_size = 8.0f;
	emitter.end_size = 16.0f;

	emitter.lifetime = 0.25f;

	emitter.emission_rate = 128.0f;

	EmitParticles( &cgs.ions, emitter );
}

/*
* CG_NewBloodTrail
*/
void CG_NewBloodTrail( centity_t *cent ) {
	float radius = 2.5f;
	float alpha = 1.0f;
	const Material * material = cgs.media.shaderBloodTrailPuff;

	// didn't move
	Vec3 vec = cent->ent.origin - cent->trailOrigin;
	float len = Length( vec );
	vec = Normalize( vec );
	if( !len ) {
		return;
	}

	// we don't add more than one sprite each frame. If frame
	// ratio is too slow, people will prefer having less sprites on screen
	if( cent->localEffects[LOCALEFFECT_BLOODTRAIL_LAST_DROP] + 100 < cl.serverTime ) {
		cent->localEffects[LOCALEFFECT_BLOODTRAIL_LAST_DROP] = cl.serverTime;

		int contents = ( CG_PointContents( cent->trailOrigin ) & CG_PointContents( cent->ent.origin ) );
		if( contents & MASK_WATER ) {
			material = cgs.media.shaderBloodTrailLiquidPuff;
			radius = 4 + random_float11( &cls.rng );
			alpha = 0.5f;
		}

		LocalEntity * le = CG_AllocSprite( LE_SCALE_ALPHA_FADE, cent->trailOrigin, radius, 8,
							 Vec4( 1, 1, 1, alpha ),
							 0, 0, 0, 0,
							 material );
		le->velocity = Vec3( -vec.x * 5 + random_float11( &cls.rng ) * 5, -vec.y * 5 + random_float11( &cls.rng ) * 5, -vec.z * 5 + random_float11( &cls.rng ) * 5 + 3 );
		le->ent.rotation = random_float01( &cls.rng ) * 360;
	}
}

/*
* CG_BloodDamageEffect
*/
void CG_BloodDamageEffect( Vec3 origin, Vec3 dir, int damage, Vec4 team_color ) {
	float radius = 5.0f, alpha = 1.0f;
	int time = 8;
	const Material * material = cgs.media.shaderBloodImpactPuff;
	Vec3 local_dir;

	int count = Clamp( 1, int( damage * 0.25f ), 10 );

	if( CG_PointContents( origin ) & MASK_WATER ) {
		material = cgs.media.shaderBloodTrailLiquidPuff;
		radius += ( 1 + random_float11( &cls.rng ) );
		alpha = 0.5f;
	}

	if( !Length( dir ) ) {
		local_dir = -FromQFAxis( cg.view.axis, AXIS_FORWARD );
	} else {
		local_dir = Normalize( dir );
	}

	for( int i = 0; i < count; i++ ) {
		LocalEntity *le = CG_AllocSprite( LE_PUFF_SHRINK, origin, radius + random_float11( &cls.rng ), time,
			Vec4( team_color.xyz(), alpha ),
			0, 0, 0, 0, material );

		le->ent.rotation = random_float01( &cls.rng ) * 360;

		// randomize dir
		le->velocity = Vec3(
			-local_dir.x * 5 + random_float11( &cls.rng ) * 5,
			-local_dir.y * 5 + random_float11( &cls.rng ) * 5,
			-local_dir.z * 5 + random_float11( &cls.rng ) * 5 + 3
		);
		le->velocity = local_dir + Min2( 6, count ) * le->velocity;
	}
}

/*
* CG_PModel_SpawnTeleportEffect
*/
void CG_PModel_SpawnTeleportEffect( centity_t * cent, MatrixPalettes temp_pose ) {
	for( int i = LOCALEFFECT_EV_PLAYER_TELEPORT_IN; i <= LOCALEFFECT_EV_PLAYER_TELEPORT_OUT; i++ ) {
		if( !cent->localEffects[i] )
			continue;

		cent->localEffects[i] = 0;

		Vec3 teleportOrigin;
		Vec4 color = Vec4( 0.5f, 0.5f, 0.5f, 1.0f );
		if( i == LOCALEFFECT_EV_PLAYER_TELEPORT_OUT ) {
			teleportOrigin = cent->teleportedFrom;
		}
		else {
			teleportOrigin = cent->teleportedTo;
			if( ISVIEWERENTITY( cent->current.number ) ) {
				color = Vec4( 0.1f, 0.1f, 0.1f, 1.0f );
			}
		}

		LocalEntity * le = CG_AllocModel( LE_RGB_FADE, teleportOrigin, Vec3( 0.0f ), 10,
							color, 0, 0, 0, 0, cent->ent.model,
							cgs.media.shaderTeleportShellGfx );

		MatrixPalettes pose;
		pose.joint_poses = ALLOC_SPAN( sys_allocator, Mat4, temp_pose.joint_poses.n );
		pose.skinning_matrices = ALLOC_SPAN( sys_allocator, Mat4, temp_pose.skinning_matrices.n );
		memcpy( pose.joint_poses.ptr, temp_pose.joint_poses.ptr, pose.joint_poses.num_bytes() );
		memcpy( pose.skinning_matrices.ptr, temp_pose.skinning_matrices.ptr, pose.skinning_matrices.num_bytes() );

		le->pose = pose;

		Matrix3_Copy( cent->ent.axis, le->ent.axis );
	}
}

/*
* CG_GrenadeExplosionMode
*/
void CG_GrenadeExplosionMode( Vec3 pos, Vec3 dir, Vec4 team_color ) {
	// CG_SpawnDecal( pos, decaldir, random_float01( &cls.rng ) * 360, radius * 0.5, 1, 1, 1, 1, 10, 1, false, cgs.media.shaderExplosionMark );

	// Explosion particles
	CG_ParticleRocketExplosionEffect( pos, dir, team_color.xyz() );

	S_StartFixedSound( cgs.media.sfxGrenadeExplosion, pos, CHAN_AUTO, 1.0f );
}

/*
* CG_GenericExplosion
*/
void CG_GenericExplosion( Vec3 pos, Vec3 dir, float radius ) {
	LocalEntity *le;
	Vec3 angles;
	Vec3 decaldir;
	Vec3 origin, vec;
	float expvelocity = 8.0f;

	decaldir = dir;
	angles = VecToAngles( dir );

	//if( CG_PointContents( pos ) & MASK_WATER )
	//jalfixme: (shouldn't we do the water sound variation?)

	// CG_SpawnDecal( pos, decaldir, random_float01( &cls.rng ) * 360, radius * 0.5, 1, 1, 1, 1, 10, 1, false, cgs.media.shaderExplosionMark );

	// animmap shader of the explosion
	origin = pos + dir * ( radius * 0.15f );
	le = CG_AllocSprite( LE_ALPHA_FADE, origin, radius * 0.5f, 8,
						 vec4_white,
						 radius * 4, 0.75f, 0.533f, 0, // yellow dlight
						 cgs.media.shaderRocketExplosion );

	vec = Vec3( random_float11( &cls.rng ) * expvelocity, random_float11( &cls.rng ) * expvelocity, random_float11( &cls.rng ) * expvelocity );
	le->velocity = dir * expvelocity;
	le->velocity = le->velocity + vec;
	le->ent.rotation = random_float01( &cls.rng ) * 360;

	// use the rocket explosion sounds
	S_StartFixedSound( cgs.media.sfxRocketLauncherHit, pos, CHAN_AUTO, 1.0f );
}

void CG_Dash( const SyncEntityState *state ) {
	LocalEntity *le;
	Vec3 pos, dvect, angle = { 0, 0, 0 };

	// KoFFiE: Calculate angle based on relative position of the previous origin state of the player entity
	dvect = state->origin - cg_entities[state->number].prev.origin;

	// ugly inline define -> Ignore when difference between 2 positions was less than this value.
#define IGNORE_DASH 6.0

	if( ( dvect.x > -IGNORE_DASH ) && ( dvect.x < IGNORE_DASH ) &&
		( dvect.y > -IGNORE_DASH ) && ( dvect.y < IGNORE_DASH ) ) {
		return;
	}

	angle = VecToAngles( dvect );
	pos = state->origin;
	angle.y += 270; // Adjust angle
	pos.z -= 24; // Adjust the position to ground height

	if( CG_PointContents( pos ) & MASK_WATER ) {
		return; // no smoke under water :)
	}

	le = CG_AllocModel( LE_DASH_SCALE, pos, angle, 7,
						Vec4( 1.0f, 1.0f, 1.0f, 0.2f ),
						0, 0, 0, 0,
						cgs.media.modDash,
						NULL
						);
	le->ent.scale = 0.01f;
	le->ent.axis[AXIS_UP + 2] *= 2.0f;
}

void CG_Explosion_Puff( Vec3 pos, float radius, int frame ) {
	const Material * material = cgs.media.shaderSmokePuff1;
	Vec3 local_pos;

	switch( (int)floorf( random_float11( &cls.rng ) * 3.0f ) ) {
		case 0:
			material = cgs.media.shaderSmokePuff1;
			break;
		case 1:
			material = cgs.media.shaderSmokePuff2;
			break;
		case 2:
			material = cgs.media.shaderSmokePuff3;
			break;
	}

	local_pos = pos;
	local_pos.x += random_float11( &cls.rng ) * 4;
	local_pos.y += random_float11( &cls.rng ) * 4;
	local_pos.z += random_float11( &cls.rng ) * 4;

	LocalEntity * le = CG_AllocSprite( LE_PUFF_SCALE, local_pos, radius, frame,
						 vec4_white,
						 0, 0, 0, 0,
						 material );
	le->ent.rotation = random_float01( &cls.rng ) * 360;
}

void CG_Explosion_Puff_2( Vec3 pos, Vec3 vel, int radius ) {

	const Material * material = cgs.media.shaderSmokePuff3;
	if( radius == 0 ) {
		radius = (int)floorf( 35.0f + random_float11( &cls.rng ) * 5 );
	}

	LocalEntity * le = CG_AllocSprite( LE_PUFF_SHRINK, pos, radius, 7,
						 Vec4( 1, 1, 1, 0.2f ),
						 0, 0, 0, 0,
						 material );
	le->velocity = vel;

	//le->ent.rotation = rand () % 360;
}

void CG_DustCircle( Vec3 pos, Vec3 dir, float radius, int count ) {
	// Vec3 dir_per1;
	// Vec3 dir_per2;
	// Vec3 dir_temp = { 0.0f, 0.0f, 0.0f };
	// int i;
	// float angle;
	//
	// if( CG_PointContents( pos ) & MASK_WATER ) {
	// 	return; // no smoke under water :)
	// }
	// PerpendicularVector( &dir_per2, dir );
	// dir_per1 = Cross( dir, dir_per2 );
	//
	// dir_per1 *= Length( dir_per1 );
	// dir_per2 *= Length( dir_per2 );
	// Normalize( dir_per1 );
	// Normalize( dir_per2 );
	//
	// for( i = 0; i < count; i++ ) {
	// 	angle = (float)( PI * 2.0f / count * i );
	// 	dir_temp = Vec3( 0.0f, 0.0f, 0.0f );
	// 	dir_temp = dir_temp + dir_per1 * ( sinf( angle ) );
	// 	dir_temp = dir_temp + dir_per2 * ( cosf( angle ) );
	//
	// 	//dir_temp = dir_temp * ( dir_temp) = Normalize( dir_temp) );
	// 	dir_temp = dir_temp * ( random_float11( &cls.rng ) * 10 + radius );
	// 	CG_Explosion_Puff_2( pos, dir_temp, 10 );
	// }
}


/*
* CG_AddLocalEntities
*/
void CG_AddLocalEntities( void ) {
#define FADEINFRAMES 2
	int f;
	LocalEntity *le, *next, *hnode;
	entity_t *ent;
	float scale, frac, fade, time, scaleIn, fadeIn;
	Vec3 angles;

	time = (float)cls.frametime * 0.001f;

	hnode = &cg_localents_headnode;
	for( le = hnode->next; le != hnode; le = next ) {
		next = le->next;

		frac = ( cl.serverTime - le->start ) * 0.01f;
		f = Max2( 0, int( floorf( frac ) ) );

		// it's time to DIE
		if( f >= le->frames - 1 ) {
			le->type = LE_FREE;
			CG_FreeLocalEntity( le );
			continue;
		}

		if( le->frames > 1 ) {
			scale = Clamp01( 1.0f - frac / ( le->frames - 1 ) );
			fade = scale * 255.0f;

			// quick fade in, if time enough
			if( le->frames > FADEINFRAMES * 2 ) {
				scaleIn = Clamp01( frac / (float)FADEINFRAMES );
				fadeIn = scaleIn * 255.0f;
			} else {
				fadeIn = 255.0f;
			}
		} else {
			scale = 1.0f;
			fade = 255.0f;
			fadeIn = 255.0f;
		}

		ent = &le->ent;

		if( le->light && scale ) {
			// CG_AddLightToScene( ent->origin, le->light * scale, le->lightcolor[0], le->lightcolor[1], le->lightcolor[2] );
		}

		if( le->type == LE_DASH_SCALE ) {
			if( f < 1 ) {
				ent->scale = 0.15 * frac;
			} else {
				angles = VecToAngles( FromQFAxis( ent->axis, AXIS_RIGHT ) );
				ent->axis[1 * 3 + 1] += 0.005f * sinf( Radians( angles.y ) ); //length
				ent->axis[1 * 3 + 0] += 0.005f * cosf( Radians( angles.y ) ); //length
				ent->axis[0 * 3 + 1] += 0.008f * cosf( Radians( angles.y ) ); //width
				ent->axis[0 * 3 + 0] -= 0.008f * sinf( Radians( angles.y ) ); //width
				ent->axis[2 * 3 + 2] -= 0.052f;              //height

				if( ent->axis[AXIS_UP + 2] <= 0 ) {
					le->type = LE_FREE;
					CG_FreeLocalEntity( le );
				}
			}
		}
		if( le->type == LE_PUFF_SCALE ) {
			if( le->frames - f < 4 ) {
				ent->scale = 1.0f - 1.0f * ( frac - Abs( 4 - le->frames ) ) / 4;
			}
		}
		if( le->type == LE_PUFF_SHRINK ) {
			if( frac < 3 ) {
				ent->scale = 1.0f - 0.2f * frac / 4;
			} else {
				ent->scale = 0.8 - 0.8 * ( frac - 3 ) / 3;
				le->velocity = le->velocity * ( 0.85f );
			}
		}

		switch( le->type ) {
			case LE_NO_FADE:
				break;
			case LE_RGB_FADE:
				fade = Min2( fade, fadeIn );
				ent->color.r = ( uint8_t )( fade * le->color.x );
				ent->color.g = ( uint8_t )( fade * le->color.y );
				ent->color.b = ( uint8_t )( fade * le->color.z );
				break;
			case LE_SCALE_ALPHA_FADE:
				fade = Min2( fade, fadeIn );
				ent->scale = 1.0f + 1.0f / scale;
				ent->scale = Min2( ent->scale, 5.0f );
				ent->color.a = ( uint8_t )( fade * le->color.w );
				break;
			case LE_INVERSESCALE_ALPHA_FADE:
				fade = Min2( fade, fadeIn );
				ent->scale = Clamp( 0.1f, scale + 0.1f, 1.0f );
				ent->color.a = ( uint8_t )( fade * le->color.w );
				break;
			case LE_ALPHA_FADE:
				fade = Min2( fade, fadeIn );
				ent->color.a = ( uint8_t )( fade * le->color.w );
				break;
			default:
				break;
		}

		ent->origin2 = ent->origin;
		ent->origin = ent->origin + le->velocity * ( time );
		le->velocity = le->velocity + le->accel * ( time );

		CG_AddEntityToScene( ent );
	}
}

/*
* CG_FreeLocalEntities
*/
void CG_FreeLocalEntities( void ) {
	LocalEntity *le, *next, *hnode;

	hnode = &cg_localents_headnode;
	for( le = hnode->next; le != hnode; le = next ) {
		next = le->next;

		le->type = LE_FREE;
		CG_FreeLocalEntity( le );
	}

	CG_ClearLocalEntities();
}

struct Gib {
	Vec3 origin;
	Vec3 velocity;
	float scale;
	float lifetime;
	Vec4 color;
};

static Gib gibs[ 512 ];
static u32 num_gibs;

void InitGibs() {
	num_gibs = 0;
}

void SpawnGibs( Vec3 origin, Vec3 velocity, int damage, Vec4 color ) {
	ZoneScoped;

	int count = Min2( damage * 3 / 2, 60 );
	float player_radius = playerbox_stand_maxs.x;

	for( int i = 0; i < count; i++ ) {
		if( num_gibs == ARRAY_COUNT( gibs ) )
			break;

		Gib * gib = &gibs[ num_gibs ];
		num_gibs++;

		Vec3 dir = Vec3( UniformSampleDisk( &cls.rng ), 0.0f );
		gib->origin = origin + dir * player_radius;

		dir.z = random_float01( &cls.rng );
		gib->velocity = velocity * 0.5f + dir * Length( velocity ) * 0.5f;

		gib->scale = random_uniform_float( &cls.rng, 0.25f, 0.5f );
		gib->lifetime = 10.0f;
		gib->color = color;
	}
}

void DrawGibs() {
	ZoneScoped;

	float dt = cls.frametime * 0.001f;

	const Model * model = cgs.media.modGib;
	Vec3 gravity = Vec3( 0, 0, -GRAVITY );

	for( u32 i = 0; i < num_gibs; i++ ) {
		Gib * gib = &gibs[ i ];

		gib->velocity += gravity * dt;
		Vec3 next_origin = gib->origin + gib->velocity * dt;

		MinMax3 bounds = model->bounds * gib->scale;

		Vec3 qf_mins = bounds.mins;
		Vec3 qf_maxs = bounds.maxs;
		Vec3 qf_origin = gib->origin;
		Vec3 qf_next_origin = next_origin;

		trace_t trace;
		CG_Trace( &trace, qf_origin, qf_mins, qf_maxs, qf_next_origin, 0, MASK_SOLID );

		if( ( trace.contents & CONTENTS_NODROP ) || ( trace.surfFlags & SURF_SKY ) ) {
			gib->lifetime = 0;
		}
		else if( trace.fraction != 1.0f ) {
			gib->lifetime = 0;

			ParticleEmitter emitter = { };
			emitter.position = trace.endpos;

			emitter.use_cone_direction = true;
			emitter.direction_cone.normal = trace.plane.normal;
			emitter.direction_cone.theta = 90.0f;

			emitter.start_speed = 128.0f;
			emitter.end_speed = 128.0f;

			emitter.start_color = gib->color;
			emitter.end_color = gib->color.xyz();

			emitter.start_size = 4.0f;
			emitter.end_size = 4.0f;

			emitter.lifetime = 1.0f;

			emitter.n = 64;

			EmitParticles( &cgs.sparks, emitter );
		}

		gib->lifetime -= dt;
		if( gib->lifetime <= 0 ) {
			num_gibs--;
			i--;
			Swap2( gib, &gibs[ num_gibs ] );
			continue;
		}

		Mat4 transform = Mat4Translation( gib->origin ) * Mat4Scale( gib->scale );
		DrawModel( model, transform, gib->color );

		gib->origin = next_origin;
	}
}
