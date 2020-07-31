#include "cgame/cg_local.h"
#include "client/renderer/renderer.h"

static float RandomRadians() {
	return random_uniform_float( &cls.rng, 0.0f, Radians( 360.0f ) );
}

void CG_EBBeam( Vec3 start, Vec3 end, Vec4 team_color ) {
	AddPersistentBeam( start, end, 16.0f, team_color, cgs.media.shaderEBBeam, 0.25f, 0.1f );
	RailTrailParticles( start, end, team_color );
}

void CG_BubbleTrail( Vec3 start, Vec3 end, int dist ) {
	/*
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
	*/
}

void CG_PlasmaExplosion( Vec3 pos, Vec3 dir, Vec4 team_color ) {
	PlasmaImpactParticles( pos, dir, team_color.xyz() );

	AddPersistentDecal( pos, dir, 8.0f, RandomRadians(), "weapons/pg/impact_decal", team_color, 30000 );

	S_StartFixedSound( cgs.media.sfxPlasmaHit, pos, CHAN_AUTO, 1.0f );
}

void CG_BubbleExplosion( Vec3 pos, Vec4 team_color ) {
	BubbleImpactParticles( pos, team_color.xyz() );
	S_StartFixedSound( cgs.media.sfxBubbleHit, pos, CHAN_AUTO, 1.0f );
}

void CG_EBImpact( Vec3 pos, Vec3 dir, int surfFlags, Vec4 team_color ) {
	if( surfFlags & ( SURF_SKY | SURF_NOMARKS | SURF_NOIMPACT ) ) {
		return;
	}

	/*
	Vec3 angles = VecToAngles( dir );

	LocalEntity * le = CG_AllocModel( LE_INVERSESCALE_ALPHA_FADE, pos, angles, 6, // 6 is time
						vec4_white,
						250, 0.75, 0.75, 0.75, //white dlight
						cgs.media.modElectroBoltWallHit, NULL );

	le->ent.rotation = random_float01( &cls.rng ) * 360;
	le->ent.scale = 1.5f;
	*/

	AddPersistentDecal( pos, dir, 4.0f, RandomRadians(), "weapons/eb/impact_decal", team_color, 30000 );

	S_StartFixedSound( cgs.media.sfxElectroboltHit, pos, CHAN_AUTO, 1.0f );
}

static void ScorchDecal( Vec3 pos, Vec3 normal ) {
	constexpr StringHash decals[] = {
		"weapons/explosion_scorch1",
		"weapons/explosion_scorch2",
		"weapons/explosion_scorch3",
	};

	AddPersistentDecal( pos, normal, 32.0f, RandomRadians(), random_select( &cls.rng, decals ), vec4_white, 30000 );
}

void CG_RocketExplosion( Vec3 pos, Vec3 dir, Vec4 team_color ) {
	ExplosionParticles( pos, dir, team_color.xyz() );
	ScorchDecal( pos, dir );
	S_StartFixedSound( cgs.media.sfxRocketLauncherHit, pos, CHAN_AUTO, 1.0f );
}

void CG_GrenadeExplosion( Vec3 pos, Vec3 dir, Vec4 team_color ) {
	ExplosionParticles( pos, dir, team_color.xyz() );
	ScorchDecal( pos, dir );
	S_StartFixedSound( cgs.media.sfxGrenadeExplosion, pos, CHAN_AUTO, 1.0f );
}

void CG_BladeImpact( Vec3 pos, Vec3 dir ) {
	/*
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
	*/
}

void CG_ProjectileTrail( const centity_t * cent ) {
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

void CG_RifleBulletTrail( const centity_t * cent ) {
	Vec3 vec = cent->ent.origin - cent->trailOrigin;
	float len = Length( vec );
	if( len == 0 )
		return;

	vec = Normalize( vec );

	ParticleEmitter emitter = { };

	emitter.position = cent->ent.origin;
	emitter.position_distribution.type = RandomDistribution3DType_Line;
	emitter.position_distribution.line.end = cent->trailOrigin;

	emitter.start_speed = 0.0f;
	emitter.end_speed = 0.0f;

	emitter.start_color = Vec4( CG_TeamColorVec4( cent->current.team ).xyz(), 1.0f );
	// emitter.end_color = Lerp( emitter.start_color.xyz(), 0.2f, Vec3( 1.0f ) );
    emitter.end_color = Vec3( 0.0f, 0.0f, 0.0f );
	emitter.start_size = 16.0f;
	emitter.end_size = 0.0f;

	emitter.lifetime = 0.25f;

	emitter.n = 64;

	EmitParticles( &cgs.rifle_bullet, emitter );
}

void CG_NewBloodTrail( centity_t *cent ) {
	/*
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
	*/
}

void CG_BloodDamageEffect( Vec3 origin, Vec3 dir, int damage, Vec4 team_color ) {
	/*
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
	*/
}

void CG_PModel_SpawnTeleportEffect( centity_t * cent, MatrixPalettes temp_pose ) {
	/*
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
	*/
}

void CG_GenericExplosion( Vec3 pos, Vec3 dir, float radius ) {
	/*
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
	*/
}

void CG_Dash( const SyncEntityState *state ) {
	/*
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
	*/
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
	float gib_radius = cgs.media.modGib->bounds.maxs.x;
	constexpr float epsilon = 0.1f;
	float radius = player_radius - gib_radius - epsilon;

	for( int i = 0; i < count; i++ ) {
		if( num_gibs == ARRAY_COUNT( gibs ) )
			break;

		Gib * gib = &gibs[ num_gibs ];
		num_gibs++;

		Vec3 dir = Vec3( UniformSampleDisk( &cls.rng ), 0.0f );
		gib->origin = origin + dir * radius;

		dir.z = random_float01( &cls.rng );
		gib->velocity = velocity * 0.5f + dir * Length( velocity ) * 0.5f;

		gib->scale = random_uniform_float( &cls.rng, 0.5f, 1.0f );
		gib->lifetime = 10.0f;
		gib->color = color;
	}
}

static void GibImpact( Vec3 pos, Vec3 normal, Vec4 color, float scale ) {
	{
		ParticleEmitter emitter = { };
		emitter.position = pos;

		emitter.use_cone_direction = true;
		emitter.direction_cone.normal = normal;
		emitter.direction_cone.theta = 180.0f;

		emitter.start_speed = 128.0f;
		emitter.end_speed = 128.0f;

		emitter.start_color = color;
		emitter.end_color = color.xyz();

		emitter.start_size = 100.0f;
		emitter.end_size = 100.0f;

		emitter.lifetime = 0.4f;

		emitter.n = 3;

		EmitParticles( &cgs.gibimpact, emitter );
	}

	{
		constexpr StringHash decals[] = {
			"textures/blood_decals/blood1",
			"textures/blood_decals/blood2",
			"textures/blood_decals/blood3",
			"textures/blood_decals/blood4",
			"textures/blood_decals/blood5",
			"textures/blood_decals/blood6",
			"textures/blood_decals/blood7",
			"textures/blood_decals/blood8",
			"textures/blood_decals/blood9",
			"textures/blood_decals/blood10",
			"textures/blood_decals/blood11",
		};

		if( random_p( &cls.rng, 0.25f ) ) {
			AddPersistentDecal( pos, normal, scale * 64.0f, RandomRadians(), random_select( &cls.rng, decals ), color, 30000 );
		}
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

		float size = 0.5f * gib->scale;
		MinMax3 bounds = model->bounds * size;

		trace_t trace;
		CG_Trace( &trace, gib->origin, bounds.mins, bounds.maxs, next_origin, 0, MASK_SOLID );

		if( trace.startsolid || ( trace.contents & CONTENTS_NODROP ) || ( trace.surfFlags & SURF_SKY ) ) {
			gib->lifetime = 0;
		}
		else if( trace.fraction != 1.0f ) {
			gib->lifetime = 0;

			GibImpact( trace.endpos, trace.plane.normal, gib->color, gib->scale );
		}

		gib->lifetime -= dt;
		if( gib->lifetime <= 0 ) {
			num_gibs--;
			i--;
			Swap2( gib, &gibs[ num_gibs ] );
			continue;
		}

		Mat4 transform = Mat4Translation( gib->origin ) * Mat4Scale( size );
		DrawModel( model, transform, gib->color );

		gib->origin = next_origin;
	}
}
