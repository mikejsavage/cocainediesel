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

// cg_lents.c -- client side temporary entities

#include "cg_local.h"
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
	vec4_t color;

	unsigned int start;

	float light;
	vec3_t lightcolor;

	vec3_t velocity;
	vec3_t avelocity;
	vec3_t angles;
	vec3_t accel;

	int bounce;     //is activator and bounceability value at once

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
static LocalEntity *CG_AllocLocalEntity( LocalEntityType type, float r, float g, float b, float a ) {
	LocalEntity *le;

	if( cg_free_lents ) { // take a free decal if possible
		le = cg_free_lents;
		cg_free_lents = le->next;
	} else {              // grab the oldest one otherwise
		le = cg_localents_headnode.prev;
		le->prev->next = le->next;
		le->next->prev = le->prev;
	}

	memset( le, 0, sizeof( *le ) );
	le->type = type;
	le->start = cg.time;
	le->color[0] = r;
	le->color[1] = g;
	le->color[2] = b;
	le->color[3] = a;

	switch( le->type ) {
		case LE_NO_FADE:
			break;
		case LE_RGB_FADE:
			le->ent.shaderRGBA[3] = ( uint8_t )( 255 * a );
			break;
		case LE_SCALE_ALPHA_FADE:
		case LE_INVERSESCALE_ALPHA_FADE:
		case LE_PUFF_SHRINK:
			le->ent.shaderRGBA[0] = ( uint8_t )( 255 * r );
			le->ent.shaderRGBA[1] = ( uint8_t )( 255 * g );
			le->ent.shaderRGBA[2] = ( uint8_t )( 255 * b );
			le->ent.shaderRGBA[3] = ( uint8_t )( 255 * a );
			break;
		case LE_PUFF_SCALE:
			le->ent.shaderRGBA[0] = ( uint8_t )( 255 * r );
			le->ent.shaderRGBA[1] = ( uint8_t )( 255 * g );
			le->ent.shaderRGBA[2] = ( uint8_t )( 255 * b );
			le->ent.shaderRGBA[3] = ( uint8_t )( 255 * a );
			break;
		case LE_ALPHA_FADE:
			le->ent.shaderRGBA[0] = ( uint8_t )( 255 * r );
			le->ent.shaderRGBA[1] = ( uint8_t )( 255 * g );
			le->ent.shaderRGBA[2] = ( uint8_t )( 255 * b );
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
static LocalEntity *CG_AllocModel( LocalEntityType type, const vec3_t origin, const vec3_t angles, int frames,
								 float r, float g, float b, float a, float light, float lr, float lg, float lb, const Model *model, const Material * material ) {
	LocalEntity * le = CG_AllocLocalEntity( type, r, g, b, a );
	le->frames = frames;
	le->light = light;
	le->lightcolor[0] = lr;
	le->lightcolor[1] = lg;
	le->lightcolor[2] = lb;

	le->ent.model = model;
	le->ent.override_material = material;
	le->ent.shaderTime = cg.time;
	le->ent.scale = 1.0f;

	VectorCopy( angles, le->angles );
	AnglesToAxis( angles, le->ent.axis );
	VectorCopy( origin, le->ent.origin );

	return le;
}

/*
* CG_AllocSprite
*/
static LocalEntity *CG_AllocSprite( LocalEntityType type, const vec3_t origin, float radius, int frames,
								  float r, float g, float b, float a, float light, float lr, float lg, float lb, const Material * material ) {
	LocalEntity * le = CG_AllocLocalEntity( type, r, g, b, a );
	le->frames = frames;
	le->light = light;
	le->lightcolor[0] = lr;
	le->lightcolor[1] = lg;
	le->lightcolor[2] = lb;

	le->ent.radius = radius;
	le->ent.override_material = material;
	le->ent.shaderTime = cg.time;
	le->ent.scale = 1.0f;

	Matrix3_Identity( le->ent.axis );
	VectorCopy( origin, le->ent.origin );

	return le;
}

void CG_SpawnSprite( const vec3_t origin, const vec3_t velocity, const vec3_t accel,
					 float radius, int time, int bounce, bool expandEffect, bool shrinkEffect,
					 float r, float g, float b, float a,
					 float light, float lr, float lg, float lb,
					 const Material * material ) {
	LocalEntity *le;
	int numFrames;
	LocalEntityType type = LE_ALPHA_FADE;

	if( !radius || !material || !origin ) {
		return;
	}

	numFrames = (int)( (float)( time * 1000 ) * 0.01f );
	if( !numFrames ) {
		return;
	}

	if( expandEffect && !shrinkEffect ) {
		type = LE_SCALE_ALPHA_FADE;
	}

	if( !expandEffect && shrinkEffect ) {
		type = LE_INVERSESCALE_ALPHA_FADE;
	}

	le = CG_AllocSprite( type, origin, radius, numFrames,
						 r, g, b, a,
						 light, lr, lg, lb,
						 material );

	if( velocity != NULL ) {
		VectorCopy( velocity, le->velocity );
	}
	if( accel != NULL ) {
		VectorCopy( accel, le->accel );
	}

	le->bounce = bounce;
	le->ent.rotation = rand() % 360;
}

void CG_EBBeam( Vec3 start, Vec3 end, int team ) {
	Vec4 color = CG_TeamColorVec4( team );
	AddPersistentBeam( start, end, 16.0f, color, cgs.media.shaderEBBeam, 0.25f, 0.1f );
	CG_EBIonsTrail( start, end, color );
}

/*
* CG_ImpactSmokePuff
*/
void CG_ImpactSmokePuff( const vec3_t origin, const vec3_t dir, float radius, float alpha, int time, int speed ) {
#define SMOKEPUFF_MAXVIEWDIST 700
	const Material * material = cgs.media.shaderSmokePuff;
	vec3_t local_origin, local_dir;

	if( CG_PointContents( origin ) & MASK_WATER ) {
		return;
	}

	if( DistanceFast( origin, cg.view.origin ) * cg.view.fracDistFOV > SMOKEPUFF_MAXVIEWDIST ) {
		return;
	}

	if( !VectorLength( dir ) ) {
		VectorNegate( &cg.view.axis[AXIS_FORWARD], local_dir );
	} else {
		VectorNormalize2( dir, local_dir );
	}

	// offset the origin by half of the radius
	VectorMA( origin, radius * 0.5f, local_dir, local_origin );

	LocalEntity * le = CG_AllocSprite( LE_SCALE_ALPHA_FADE, local_origin, radius + crandom(), time,
						 1, 1, 1, alpha, 0, 0, 0, 0, material );

	le->ent.rotation = rand() % 360;
	VectorScale( local_dir, speed, le->velocity );
}

/*
* CG_BulletExplosion
*/
void CG_BulletExplosion( const vec3_t pos, const vec_t *dir, const trace_t *trace ) {
	LocalEntity *le;
	vec3_t angles;
	vec3_t local_dir, end;
	trace_t local_trace;
	const trace_t *tr;

	assert( dir || trace );

	if( dir ) {
		vec3_t local_pos;

		// find what are we hitting
		VectorCopy( pos, local_pos );
		VectorMA( pos, -1.0, dir, end );
		CG_Trace( &local_trace, local_pos, vec3_origin, vec3_origin, end, cg.view.POVent, MASK_SHOT );
		if( local_trace.fraction == 1.0 ) {
			return;
		}

		tr = &local_trace;
		VectorCopy( dir, local_dir );
	} else {
		tr = trace;
		VectorCopy( tr->plane.normal, local_dir );
	}

	VecToAngles( local_dir, angles );

	if( tr->ent > 0 && cg_entities[tr->ent].current.type == ET_PLAYER ) {
		return;
	} else if( ISVIEWERENTITY( tr->ent ) ) {
		return;
	} else if( tr->surfFlags & SURF_FLESH || ( tr->ent > 0 && cg_entities[tr->ent].current.type == ET_CORPSE ) ) {
		le = CG_AllocModel( LE_ALPHA_FADE, pos, angles, 3, //3 frames for weak
							1, 0, 0, 1, //full white no inducted alpha
							0, 0, 0, 0, //dlight
							cgs.media.modBulletExplode,
							NULL );
		le->ent.rotation = rand() % 360;
		le->ent.scale = 1.0f;
	} else if( cg_particles->integer && ( tr->surfFlags & SURF_DUST ) ) {
		// throw particles on dust
		CG_ImpactSmokePuff( tr->endpos, tr->plane.normal, 4, 0.6f, 6, 8 );
	} else {
		le = CG_AllocModel( LE_ALPHA_FADE, pos, angles, 3, //3 frames for weak
							1, 1, 1, 1, //full white no inducted alpha
							0, 0, 0, 0, //dlight
							cgs.media.modBulletExplode,
							NULL );
		le->ent.rotation = rand() % 360;
		le->ent.scale = 1.0f;
		if( cg_particles->integer ) {
			CG_ImpactSmokePuff( tr->endpos, tr->plane.normal, 2, 0.6f, 6, 8 );
		}

		if( !( tr->surfFlags & SURF_NOMARKS ) ) {
			CG_SpawnDecal( pos, local_dir, random() * 360, 8, 1, 1, 1, 1, 10, 1, false, cgs.media.shaderBulletMark );
		}
	}
}

/*
* CG_BubbleTrail
*/
void CG_BubbleTrail( const vec3_t start, const vec3_t end, int dist ) {
	vec3_t move, vec;
	VectorCopy( start, move );
	VectorSubtract( end, start, vec );
	float len = VectorNormalize( vec );
	if( !len ) {
		return;
	}

	VectorScale( vec, dist, vec );
	const Material * material = cgs.media.shaderWaterBubble;

	for( int i = 0; i < len; i += dist ) {
		LocalEntity * le = CG_AllocSprite( LE_ALPHA_FADE, move, 3, 10,
							 1, 1, 1, 1,
							 0, 0, 0, 0,
							 material );
		VectorSet( le->velocity, crandom() * 5, crandom() * 5, crandom() * 5 + 6 );
		VectorAdd( move, vec, move );
	}
}

/*
* CG_PlasmaExplosion
*/
void CG_PlasmaExplosion( const vec3_t pos, const vec3_t dir, int team, float radius ) {
	LocalEntity *le;
	vec3_t angles;
	float model_radius = PLASMA_EXPLOSION_MODEL_RADIUS;

	VecToAngles( dir, angles );

	vec4_t color;
	CG_TeamColor( team, color );

	le = CG_AllocModel( LE_ALPHA_FADE, pos, angles, 4,
						color[0], color[1], color[2], color[3],
						150, 0, 0.75, 0,
						cgs.media.modPlasmaExplosion,
						NULL );
	le->ent.scale = radius / model_radius;

	CG_ImpactPuffParticles( pos, dir, 15, 0.75f, color[0], color[1], color[2], color[3], NULL );

	le->ent.rotation = rand() % 360;

	CG_SpawnDecal( pos, dir, random() * 360, 16,
				   color[0], color[1], color[2], color[3],
				   4, 1, true,
				   cgs.media.shaderPlasmaMark );
}

void CG_EBImpact( const vec3_t pos, const vec3_t dir, int surfFlags, int team ) {
	LocalEntity *le;
	vec3_t angles;

	vec4_t color;
	CG_TeamColor( team, color );

	if( team != TEAM_SPECTATOR ) {
		bool decal = CG_SpawnDecal( pos, dir, 0, 3, color[0], color[1], color[2], color[3],
			10, 1, true, cgs.media.shaderEBImpact );
		if( !decal && ( surfFlags & ( SURF_SKY | SURF_NOMARKS | SURF_NOIMPACT ) ) ) {
			return;
		}
	}

	VecToAngles( dir, angles );

	le = CG_AllocModel( LE_INVERSESCALE_ALPHA_FADE, pos, angles, 6, // 6 is time
						1, 1, 1, 1, //full white no inducted alpha
						250, 0.75, 0.75, 0.75, //white dlight
						cgs.media.modElectroBoltWallHit, NULL );

	le->ent.rotation = rand() % 360;
	le->ent.scale = 1.5f;

	CG_ImpactPuffParticles( pos, dir, 15, 0.75f, color[0], color[1], color[2], color[3], NULL );

	S_StartFixedSound( cgs.media.sfxElectroboltHit, pos, CHAN_AUTO, cg_volume_effects->value, ATTN_STATIC );
}

/*
* CG_RocketExplosionMode
*/
void CG_RocketExplosionMode( const vec3_t pos, const vec3_t dir, float radius, int team ) {
	LocalEntity *le;
	vec3_t angles, vec;
	float expvelocity = 8.0f;

	VecToAngles( dir, angles );

	CG_SpawnDecal( pos, dir, random() * 360, radius * 0.5, 1, 1, 1, 1, 10, 1, false, cgs.media.shaderExplosionMark );

	vec4_t color;
	CG_TeamColor( team, color );

	// animmap shader of the explosion
	le = CG_AllocSprite( LE_ALPHA_FADE, pos, radius * 0.6f, 8,
		color[0], color[1], color[2], color[3],
		radius * 4, 0.8f, 0.6f, 0, // orange dlight
		cgs.media.shaderRocketExplosion );

	VectorSet( vec, crandom() * expvelocity, crandom() * expvelocity, crandom() * expvelocity );
	VectorScale( dir, expvelocity, le->velocity );
	VectorAdd( le->velocity, vec, le->velocity );
	le->ent.rotation = rand() % 360;

	if( cg_explosionsRing->integer ) {
		// explosion ring sprite
		vec3_t origin;
		VectorMA( pos, radius * 0.20f, dir, origin );
		le = CG_AllocSprite( LE_ALPHA_FADE, origin, radius, 3,
			color[0], color[1], color[2], color[3],
			0, 0, 0, 0, // no dlight
			cgs.media.shaderRocketExplosionRing );

		le->ent.rotation = rand() % 360;
	}

	if( cg_explosionsDust->integer == 1 ) {
		// dust ring parallel to the contact surface
		CG_ExplosionsDust( pos, dir, radius );
	}

	// Explosion particles
	CG_ParticleExplosionEffect( pos, dir, 1, 0.5, 0, 32 );

	S_StartFixedSound( cgs.media.sfxRocketLauncherHit, pos, CHAN_AUTO, cg_volume_effects->value, ATTN_DISTANT );
}

/*
* CG_BladeImpact
*/
void CG_BladeImpact( const vec3_t pos, const vec3_t dir ) {
	LocalEntity *le;
	vec3_t angles;
	vec3_t end;
	vec3_t local_pos, local_dir;
	trace_t trace;

	//find what are we hitting
	VectorCopy( pos, local_pos );
	VectorNormalize2( dir, local_dir );
	VectorMA( pos, -1.0, local_dir, end );
	CG_Trace( &trace, local_pos, vec3_origin, vec3_origin, end, cg.view.POVent, MASK_SHOT );
	if( trace.fraction == 1.0 ) {
		return;
	}

	VecToAngles( local_dir, angles );

	if( trace.surfFlags & SURF_FLESH ||
		( trace.ent > 0 && cg_entities[trace.ent].current.type == ET_PLAYER )
		|| ( trace.ent > 0 && cg_entities[trace.ent].current.type == ET_CORPSE ) ) {
		le = CG_AllocModel( LE_ALPHA_FADE, pos, angles, 3, //3 frames for weak
							1, 1, 1, 1, //full white no inducted alpha
							0, 0, 0, 0, //dlight
							cgs.media.modBladeWallHit, NULL );
		le->ent.rotation = rand() % 360;
		le->ent.scale = 1.0f;

		S_StartFixedSound( cgs.media.sfxBladeFleshHit[(int)( random() * 3 )], pos, CHAN_AUTO,
								cg_volume_effects->value, ATTN_NORM );
	} else if( trace.surfFlags & SURF_DUST ) {
		// throw particles on dust
		CG_ParticleEffect( trace.endpos, trace.plane.normal, 0.30f, 0.30f, 0.25f, 30 );

		//fixme? would need a dust sound
		S_StartFixedSound( cgs.media.sfxBladeWallHit[(int)( random() * 2 )], pos, CHAN_AUTO,
								cg_volume_effects->value, ATTN_NORM );
	} else {
		le = CG_AllocModel( LE_ALPHA_FADE, pos, angles, 3, //3 frames for weak
							1, 1, 1, 1, //full white no inducted alpha
							0, 0, 0, 0, //dlight
							cgs.media.modBladeWallHit, NULL );
		le->ent.rotation = rand() % 360;
		le->ent.scale = 1.0f;

		CG_ParticleEffect( trace.endpos, trace.plane.normal, 0.30f, 0.30f, 0.25f, 15 );

		S_StartFixedSound( cgs.media.sfxBladeWallHit[(int)( random() * 2 )], pos, CHAN_AUTO,
								cg_volume_effects->value, ATTN_NORM );
		if( !( trace.surfFlags & SURF_NOMARKS ) ) {
			CG_SpawnDecal( pos, dir, random() * 45, 8, 1, 1, 1, 1, 10, 1, false, cgs.media.shaderBladeMark );
		}
	}
}

/*
* CG_LasertGunImpact
*/
void CG_LaserGunImpact( const vec3_t pos, float radius, const vec3_t laser_dir, const vec4_t color ) {
	entity_t ent;
	vec3_t ndir;
	vec3_t angles;

	memset( &ent, 0, sizeof( ent ) );
	VectorCopy( pos, ent.origin );
	ent.scale = 1.45f;
	Vector4Set( ent.shaderRGBA, color[0] * 255, color[1] * 255, color[2] * 255, color[3] * 255 );
	ent.model = cgs.media.modLasergunWallExplo;
	VectorNegate( laser_dir, ndir );
	VecToAngles( ndir, angles );
	AnglesToAxis( angles, ent.axis );

	// trap_R_AddEntityToScene( &ent );
}


/*
* CG_ProjectileTrail
*/
void CG_ProjectileTrail( centity_t *cent ) {
	if( !cg_projectileFireTrail->integer )
		return;

	float radius = 8;
	float alpha = Clamp01( cg_projectileFireTrailAlpha->value );

	// didn't move
	vec3_t vec;
	VectorSubtract( cent->ent.origin, cent->trailOrigin, vec );
	float len = VectorNormalize( vec );
	if( len == 0 )
		return;

	const Material * material = cgs.media.shaderRocketFireTrailPuff;

	// density is found by quantity per second
	int trailTime = int( 1000.0f / cg_projectileFireTrail->value );
	if( trailTime < 1 ) {
		trailTime = 1;
	}

	// we don't add more than one sprite each frame. If frame
	// ratio is too slow, people will prefer having less sprites on screen
	if( cent->localEffects[LOCALEFFECT_ROCKETFIRE_LAST_DROP] + trailTime < cg.time ) {
		cent->localEffects[LOCALEFFECT_ROCKETFIRE_LAST_DROP] = cg.time;

		vec4_t color;
		CG_TeamColor( cent->current.team, color );
		LocalEntity * le = CG_AllocSprite( LE_INVERSESCALE_ALPHA_FADE, cent->trailOrigin, radius, 4,
							 color[ 0 ], color[ 1 ], color[ 2 ], alpha,
							 0, 0, 0, 0,
							 material );
		VectorSet( le->velocity, -vec[0] * 10 + crandom() * 5, -vec[1] * 10 + crandom() * 5, -vec[2] * 10 + crandom() * 5 );
		le->ent.rotation = rand() % 360;
	}
}

/*
* CG_NewBloodTrail
*/
void CG_NewBloodTrail( centity_t *cent ) {
	float radius = 2.5f;
	float alpha = Clamp01( cg_bloodTrailAlpha->value );
	const Material * material = cgs.media.shaderBloodTrailPuff;

	if( !cg_showBloodTrail->integer ) {
		return;
	}

	if( !cg_bloodTrail->integer ) {
		return;
	}

	// didn't move
	vec3_t vec;
	VectorSubtract( cent->ent.origin, cent->trailOrigin, vec );
	float len = VectorNormalize( vec );
	if( !len ) {
		return;
	}

	// density is found by quantity per second
	int trailTime = (int)( 1000.0f / cg_bloodTrail->value );
	if( trailTime < 1 ) {
		trailTime = 1;
	}

	// we don't add more than one sprite each frame. If frame
	// ratio is too slow, people will prefer having less sprites on screen
	if( cent->localEffects[LOCALEFFECT_BLOODTRAIL_LAST_DROP] + trailTime < cg.time ) {
		cent->localEffects[LOCALEFFECT_BLOODTRAIL_LAST_DROP] = cg.time;

		int contents = ( CG_PointContents( cent->trailOrigin ) & CG_PointContents( cent->ent.origin ) );
		if( contents & MASK_WATER ) {
			material = cgs.media.shaderBloodTrailLiquidPuff;
			radius = 4 + crandom();
			alpha *= 0.5f;
		}

		LocalEntity * le = CG_AllocSprite( LE_SCALE_ALPHA_FADE, cent->trailOrigin, radius, 8,
							 1.0f, 1.0f, 1.0f, alpha,
							 0, 0, 0, 0,
							 material );
		VectorSet( le->velocity, -vec[0] * 5 + crandom() * 5, -vec[1] * 5 + crandom() * 5, -vec[2] * 5 + crandom() * 5 + 3 );
		le->ent.rotation = rand() % 360;
	}
}

/*
* CG_BloodDamageEffect
*/
void CG_BloodDamageEffect( const vec3_t origin, const vec3_t dir, int damage, int team ) {
	float radius = 5.0f, alpha = cg_bloodTrailAlpha->value;
	int time = 8;
	const Material * material = cgs.media.shaderBloodImpactPuff;
	vec3_t local_dir;

	if( !cg_showBloodTrail->integer ) {
		return;
	}

	if( !cg_bloodTrail->integer ) {
		return;
	}

	int count = Clamp( 1, int( damage * 0.25f ), 10 );

	if( CG_PointContents( origin ) & MASK_WATER ) {
		material = cgs.media.shaderBloodTrailLiquidPuff;
		radius += ( 1 + crandom() );
		alpha = 0.5f * cg_bloodTrailAlpha->value;
	}

	if( !VectorLength( dir ) ) {
		VectorNegate( &cg.view.axis[AXIS_FORWARD], local_dir );
	} else {
		VectorNormalize2( dir, local_dir );
	}

	vec4_t color;
	CG_TeamColor( team, color );

	for( int i = 0; i < count; i++ ) {
		LocalEntity *le = CG_AllocSprite( LE_PUFF_SHRINK, origin, radius + crandom(), time,
			color[ 0 ], color[ 1 ], color[ 2 ], alpha,
			0, 0, 0, 0, material );

		le->ent.rotation = rand() % 360;

		// randomize dir
		VectorSet( le->velocity,
				   -local_dir[0] * 5 + crandom() * 5,
				   -local_dir[1] * 5 + crandom() * 5,
				   -local_dir[2] * 5 + crandom() * 5 + 3 );
		VectorMA( local_dir, min( 6, count ), le->velocity, le->velocity );
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

		vec3_t teleportOrigin;
		vec3_t rgb;
		VectorSet( rgb, 0.5, 0.5, 0.5 );
		if( i == LOCALEFFECT_EV_PLAYER_TELEPORT_OUT ) {
			VectorCopy( cent->teleportedFrom, teleportOrigin );
		}
		else {
			VectorCopy( cent->teleportedTo, teleportOrigin );
			if( ISVIEWERENTITY( cent->current.number ) ) {
				VectorSet( rgb, 0.1, 0.1, 0.1 );
			}
		}

		LocalEntity * le = CG_AllocModel( LE_RGB_FADE, teleportOrigin, vec3_origin, 10,
							rgb[0], rgb[1], rgb[2], 1, 0, 0, 0, 0, cent->ent.model,
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
void CG_GrenadeExplosionMode( const vec3_t pos, const vec3_t dir, float radius, int team ) {
	LocalEntity *le;
	vec3_t angles;
	vec3_t decaldir;
	vec3_t origin, vec;
	float expvelocity = 8.0f;

	VectorCopy( dir, decaldir );
	VecToAngles( dir, angles );

	CG_SpawnDecal( pos, decaldir, random() * 360, radius * 0.5, 1, 1, 1, 1, 10, 1, false, cgs.media.shaderExplosionMark );

	vec4_t color;
	CG_TeamColor( team, color );

	// animmap shader of the explosion
	VectorMA( pos, radius * 0.15f, dir, origin );
	le = CG_AllocSprite( LE_ALPHA_FADE, origin, radius * 0.5f, 8,
		color[0], color[1], color[2], color[3],
		radius * 4, 0.75f, 0.533f, 0, // yellow dlight
		cgs.media.shaderGrenadeExplosion );

	VectorSet( vec, crandom() * expvelocity, crandom() * expvelocity, crandom() * expvelocity );
	VectorScale( dir, expvelocity, le->velocity );
	VectorAdd( le->velocity, vec, le->velocity );
	le->ent.rotation = rand() % 360;

	// explosion ring sprite
	if( cg_explosionsRing->integer ) {
		VectorMA( pos, radius * 0.25f, dir, origin );
		le = CG_AllocSprite( LE_ALPHA_FADE, origin, radius, 3,
			color[0], color[1], color[2], color[3],
			0, 0, 0, 0, // no dlight
			cgs.media.shaderGrenadeExplosionRing );

		le->ent.rotation = rand() % 360;
	}

	if( cg_explosionsDust->integer == 1 ) {
		// dust ring parallel to the contact surface
		CG_ExplosionsDust( pos, dir, radius );
	}

	// Explosion particles
	CG_ParticleExplosionEffect( pos, dir, 1, 0.5, 0, 32 );

	S_StartFixedSound( cgs.media.sfxGrenadeExplosion, pos, CHAN_AUTO, cg_volume_effects->value, ATTN_DISTANT );
}

/*
* CG_GenericExplosion
*/
void CG_GenericExplosion( const vec3_t pos, const vec3_t dir, float radius ) {
	LocalEntity *le;
	vec3_t angles;
	vec3_t decaldir;
	vec3_t origin, vec;
	float expvelocity = 8.0f;

	VectorCopy( dir, decaldir );
	VecToAngles( dir, angles );

	//if( CG_PointContents( pos ) & MASK_WATER )
	//jalfixme: (shouldn't we do the water sound variation?)

	CG_SpawnDecal( pos, decaldir, random() * 360, radius * 0.5, 1, 1, 1, 1, 10, 1, false, cgs.media.shaderExplosionMark );

	// animmap shader of the explosion
	VectorMA( pos, radius * 0.15f, dir, origin );
	le = CG_AllocSprite( LE_ALPHA_FADE, origin, radius * 0.5f, 8,
						 1, 1, 1, 1,
						 radius * 4, 0.75f, 0.533f, 0, // yellow dlight
						 cgs.media.shaderRocketExplosion );

	VectorSet( vec, crandom() * expvelocity, crandom() * expvelocity, crandom() * expvelocity );
	VectorScale( dir, expvelocity, le->velocity );
	VectorAdd( le->velocity, vec, le->velocity );
	le->ent.rotation = rand() % 360;

	// use the rocket explosion sounds
	S_StartFixedSound( cgs.media.sfxRocketLauncherHit, pos, CHAN_AUTO, cg_volume_effects->value, ATTN_DISTANT );
}

void CG_Dash( const entity_state_t *state ) {
	LocalEntity *le;
	vec3_t pos, dvect, angle = { 0, 0, 0 };

	if( !( cg_cartoonEffects->integer & 4 ) ) {
		return;
	}

	// KoFFiE: Calculate angle based on relative position of the previous origin state of the player entity
	VectorSubtract( state->origin, cg_entities[state->number].prev.origin, dvect );

	// ugly inline define -> Ignore when difference between 2 positions was less than this value.
#define IGNORE_DASH 6.0

	if( ( dvect[0] > -IGNORE_DASH ) && ( dvect[0] < IGNORE_DASH ) &&
		( dvect[1] > -IGNORE_DASH ) && ( dvect[1] < IGNORE_DASH ) ) {
		return;
	}

	VecToAngles( dvect, angle );
	VectorCopy( state->origin, pos );
	angle[1] += 270; // Adjust angle
	pos[2] -= 24; // Adjust the position to ground height

	if( CG_PointContents( pos ) & MASK_WATER ) {
		return; // no smoke under water :)
	}

	le = CG_AllocModel( LE_DASH_SCALE, pos, angle, 7,
						1.0, 1.0, 1.0, 0.2f,
						0, 0, 0, 0,
						cgs.media.modDash,
						NULL
						);
	le->ent.scale = 0.01f;
	le->ent.axis[AXIS_UP + 2] *= 2.0f;
}

void CG_Explosion_Puff( const vec3_t pos, float radius, int frame ) {
	const Material * material = cgs.media.shaderSmokePuff1;
	vec3_t local_pos;

	switch( (int)floor( crandom() * 3.0f ) ) {
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

	VectorCopy( pos, local_pos );
	local_pos[0] += crandom() * 4;
	local_pos[1] += crandom() * 4;
	local_pos[2] += crandom() * 4;

	LocalEntity * le = CG_AllocSprite( LE_PUFF_SCALE, local_pos, radius, frame,
						 1.0f, 1.0f, 1.0f, 1.0f,
						 0, 0, 0, 0,
						 material );
	le->ent.rotation = rand() % 360;
}

void CG_Explosion_Puff_2( const vec3_t pos, const vec3_t vel, int radius ) {

	const Material * material = cgs.media.shaderSmokePuff3;
	if( radius == 0 ) {
		radius = (int)floor( 35.0f + crandom() * 5 );
	}

	LocalEntity * le = CG_AllocSprite( LE_PUFF_SHRINK, pos, radius, 7,
						 1.0f, 1.0f, 1.0f, 0.2f,
						 0, 0, 0, 0,
						 material );
	VectorCopy( vel, le->velocity );

	//le->ent.rotation = rand () % 360;
}

void CG_DustCircle( const vec3_t pos, const vec3_t dir, float radius, int count ) {
	vec3_t dir_per1;
	vec3_t dir_per2;
	vec3_t dir_temp = { 0.0f, 0.0f, 0.0f };
	int i;
	float angle;

	if( CG_PointContents( pos ) & MASK_WATER ) {
		return; // no smoke under water :)

	}
	PerpendicularVector( dir_per2, dir );
	CrossProduct( dir, dir_per2, dir_per1 );

	VectorScale( dir_per1, VectorNormalize( dir_per1 ), dir_per1 );
	VectorScale( dir_per2, VectorNormalize( dir_per2 ), dir_per2 );

	for( i = 0; i < count; i++ ) {
		angle = (float)( 6.2831f / count * i );
		VectorSet( dir_temp, 0.0f, 0.0f, 0.0f );
		VectorMA( dir_temp, sin( angle ), dir_per1, dir_temp );
		VectorMA( dir_temp, cos( angle ), dir_per2, dir_temp );

		//VectorScale(dir_temp, VectorNormalize(dir_temp),dir_temp );
		VectorScale( dir_temp, crandom() * 10 + radius, dir_temp );
		CG_Explosion_Puff_2( pos, dir_temp, 10 );
	}
}

void CG_ExplosionsDust( const vec3_t pos, const vec3_t dir, float radius ) {
	const int count = 32; /* Number of sprites used to create the circle */
	LocalEntity *le;
	const Material * material = cgs.media.shaderSmokePuff3;

	vec3_t dir_per1;
	vec3_t dir_per2;
	vec3_t dir_temp = { 0.0f, 0.0f, 0.0f };
	int i;
	float angle;

	if( CG_PointContents( pos ) & MASK_WATER ) {
		return; // no smoke under water :)

	}
	PerpendicularVector( dir_per2, dir );
	CrossProduct( dir, dir_per2, dir_per1 );

	//VectorScale( dir_per1, VectorNormalize( dir_per1 ), dir_per1 );
	//VectorScale( dir_per2, VectorNormalize( dir_per2 ), dir_per2 );

	// make a circle out of the specified number (int count) of sprites
	for( i = 0; i < count; i++ ) {
		angle = (float)( 6.2831f / count * i );
		VectorSet( dir_temp, 0.0f, 0.0f, 0.0f );
		VectorMA( dir_temp, sin( angle ), dir_per1, dir_temp );
		VectorMA( dir_temp, cos( angle ), dir_per2, dir_temp );

		//VectorScale(dir_temp, VectorNormalize(dir_temp),dir_temp );
		VectorScale( dir_temp, crandom() * 8 + radius + 16.0f, dir_temp );

		// make the sprite smaller & alpha'd
		le = CG_AllocSprite( LE_ALPHA_FADE, pos, 10, 10,
							 1.0f, 1.0f, 1.0f, 1.0f,
							 0, 0, 0, 0,
							 material );
		VectorCopy( dir_temp, le->velocity );
	}
}

void CG_SmallPileOfGibs( const vec3_t origin, int damage, const vec3_t initialVelocity, int team ) {
	LocalEntity *le;
	vec3_t angles, velocity;

	int time = 25;
	int count = min( damage * 0.75, 60 );

	for( int i = 0; i < count; i++ ) {
		vec4_t color;

		CG_TeamColor( team, color );

		le = CG_AllocModel( LE_ALPHA_FADE, origin, vec3_origin, time + time * random(),
							color[0], color[1], color[2], color[3],
							0, 0, 0, 0,
							cgs.media.modGib,
							NULL );

		// random rotation and scale variations
		VectorSet( angles, crandom() * 360, crandom() * 360, crandom() * 360 );
		AnglesToAxis( angles, le->ent.axis );
		le->ent.scale = 0.5f - ( random() * 0.25 );

		velocity[0] = crandom() * 0.5;
		velocity[1] = crandom() * 0.5;
		velocity[2] = random() * 0.5; // always have upwards
		VectorNormalize( velocity );
		VectorScale( velocity, min( damage * 4, 320 ), velocity );

		velocity[0] += crandom() * bound( 0, damage, 150 );
		velocity[1] += crandom() * bound( 0, damage, 150 );
		velocity[2] += random() * bound( 0, damage, 250 );

		VectorAdd( initialVelocity, velocity, le->velocity );

		//friction and gravity
		VectorSet( le->accel, -1.0f, -1.0f, -1000 );

		le->bounce = 20;
	}
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
	vec3_t angles;

	time = (float)cg.frameTime * 0.001f;

	hnode = &cg_localents_headnode;
	for( le = hnode->next; le != hnode; le = next ) {
		next = le->next;

		frac = ( cg.time - le->start ) * 0.01f;
		f = Max2( 0, int( floorf( frac ) ) );

		// it's time to DIE
		if( f >= le->frames - 1 ) {
			le->type = LE_FREE;
			CG_FreeLocalEntity( le );
			continue;
		}

		if( le->frames > 1 ) {
			scale = 1.0f - frac / ( le->frames - 1 );
			scale = bound( 0.0f, scale, 1.0f );
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
			CG_AddLightToScene( ent->origin, le->light * scale, le->lightcolor[0], le->lightcolor[1], le->lightcolor[2] );
		}

		if( le->type == LE_DASH_SCALE ) {
			if( f < 1 ) {
				ent->scale = 0.15 * frac;
			} else {
				VecToAngles( &ent->axis[AXIS_RIGHT], angles );
				ent->axis[1 * 3 + 1] += 0.005f * sin( DEG2RAD( angles[YAW] ) ); //length
				ent->axis[1 * 3 + 0] += 0.005f * cos( DEG2RAD( angles[YAW] ) ); //length
				ent->axis[0 * 3 + 1] += 0.008f * cos( DEG2RAD( angles[YAW] ) ); //width
				ent->axis[0 * 3 + 0] -= 0.008f * sin( DEG2RAD( angles[YAW] ) ); //width
				ent->axis[2 * 3 + 2] -= 0.052f;              //height

				if( ent->axis[AXIS_UP + 2] <= 0 ) {
					le->type = LE_FREE;
					CG_FreeLocalEntity( le );
				}
			}
		}
		if( le->type == LE_PUFF_SCALE ) {
			if( le->frames - f < 4 ) {
				ent->scale = 1.0f - 1.0f * ( frac - abs( 4 - le->frames ) ) / 4;
			}
		}
		if( le->type == LE_PUFF_SHRINK ) {
			if( frac < 3 ) {
				ent->scale = 1.0f - 0.2f * frac / 4;
			} else {
				ent->scale = 0.8 - 0.8 * ( frac - 3 ) / 3;
				VectorScale( le->velocity, 0.85f, le->velocity );
			}
		}

		switch( le->type ) {
			case LE_NO_FADE:
				break;
			case LE_RGB_FADE:
				fade = min( fade, fadeIn );
				ent->shaderRGBA[0] = ( uint8_t )( fade * le->color[0] );
				ent->shaderRGBA[1] = ( uint8_t )( fade * le->color[1] );
				ent->shaderRGBA[2] = ( uint8_t )( fade * le->color[2] );
				break;
			case LE_SCALE_ALPHA_FADE:
				fade = min( fade, fadeIn );
				ent->scale = 1.0f + 1.0f / scale;
				ent->scale = min( ent->scale, 5.0f );
				ent->shaderRGBA[3] = ( uint8_t )( fade * le->color[3] );
				break;
			case LE_INVERSESCALE_ALPHA_FADE:
				fade = min( fade, fadeIn );
				ent->scale = Clamp( 0.1f, scale + 0.1f, 1.0f );
				ent->shaderRGBA[3] = ( uint8_t )( fade * le->color[3] );
				break;
			case LE_ALPHA_FADE:
				fade = min( fade, fadeIn );
				ent->shaderRGBA[3] = ( uint8_t )( fade * le->color[3] );
				break;
			default:
				break;
		}

		if( le->avelocity[0] || le->avelocity[1] || le->avelocity[2] ) {
			VectorMA( le->angles, time, le->avelocity, le->angles );
			AnglesToAxis( le->angles, le->ent.axis );
		}

		// apply rotational friction
		if( le->bounce ) { // FIXME?
			int i;
			const float adj = 100 * 6 * time; // magic constants here

			for( i = 0; i < 3; i++ ) {
				if( le->avelocity[i] > 0.0f ) {
					le->avelocity[i] -= adj;
					if( le->avelocity[i] < 0.0f ) {
						le->avelocity[i] = 0.0f;
					}
				} else if( le->avelocity[i] < 0.0f ) {
					le->avelocity[i] += adj;
					if( le->avelocity[i] > 0.0f ) {
						le->avelocity[i] = 0.0f;
					}
				}
			}
		}

		if( le->bounce ) {
			trace_t trace;
			vec3_t next_origin;

			VectorMA( ent->origin, time, le->velocity, next_origin );

			assert( le->ent.model );
			MinMax3 bounds = le->ent.model->bounds;
			bounds.mins *= le->ent.scale;
			bounds.maxs *= le->ent.scale;

			vec3_t qf_mins, qf_maxs;
			VectorCopy( bounds.mins.ptr(), qf_mins );
			VectorCopy( bounds.maxs.ptr(), qf_maxs );

			CG_Trace( &trace, ent->origin, qf_mins, qf_maxs, next_origin, 0, MASK_SOLID );

			// remove the particle when going out of the map
			if( ( trace.contents & CONTENTS_NODROP ) || ( trace.surfFlags & SURF_SKY ) ) {
				le->frames = 0;
			} else if( trace.fraction != 1.0 ) {   // found solid
				float dot;
				float xyzspeed, orig_xyzspeed;
				float bounce;

				orig_xyzspeed = VectorLength( le->velocity );

				// Reflect velocity
				dot = DotProduct( le->velocity, trace.plane.normal );
				VectorMA( le->velocity, -2.0f * dot, trace.plane.normal, le->velocity );

				//put new origin in the impact point, but move it out a bit along the normal
				VectorMA( trace.endpos, 1, trace.plane.normal, ent->origin );

				// make sure we don't gain speed from bouncing off
				bounce = 2.0f * le->bounce * 0.01f;
				if( bounce < 1.5f ) {
					bounce = 1.5f;
				}
				xyzspeed = orig_xyzspeed / bounce;

				VectorNormalize( le->velocity );
				VectorScale( le->velocity, xyzspeed, le->velocity );

				//the entity has not speed enough. Stop checks
				if( xyzspeed * time < 1.0f ) {
					trace_t traceground;
					vec3_t ground_origin;

					//see if we have ground
					VectorCopy( ent->origin, ground_origin );
					ground_origin[2] += bounds.mins.z - 4;
					CG_Trace( &traceground, ent->origin, qf_mins, qf_maxs, ground_origin, 0, MASK_SOLID );
					if( traceground.fraction != 1.0 ) {
						le->bounce = 0;
						VectorClear( le->velocity );
						VectorClear( le->accel );
						VectorClear( le->avelocity );
					}
				}

			} else {
				VectorCopy( ent->origin, ent->origin2 );
				VectorCopy( next_origin, ent->origin );
			}
		} else {
			VectorCopy( ent->origin, ent->origin2 );
			VectorMA( ent->origin, time, le->velocity, ent->origin );
		}

		VectorMA( le->velocity, time, le->accel, le->velocity );

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
