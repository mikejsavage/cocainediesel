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

/*
* CG_Event_WeaponBeam
*/
static void CG_Event_WeaponBeam( vec3_t origin, vec3_t dir, int ownerNum ) {
	vec3_t end;
	VectorNormalize( dir );
	float range = GS_GetWeaponDef( Weapon_Railgun )->range;
	VectorMA( origin, range, dir, end );

	centity_t * owner = &cg_entities[ ownerNum ];

	// retrace to spawn wall impact
	trace_t trace;
	CG_Trace( &trace, origin, vec3_origin, vec3_origin, end, cg.view.POVent, MASK_WALLBANG );
	if( trace.ent != -1 ) {
		CG_EBImpact( trace.endpos, trace.plane.normal, trace.surfFlags, owner->current.team );
	}

	// when it's predicted we have to delay the drawing until the view weapon is calculated
	owner->localEffects[LOCALEFFECT_EV_WEAPONBEAM] = Weapon_Railgun;
	VectorCopy( origin, owner->laserOrigin );
	VectorCopy( trace.endpos, owner->laserPoint );
}

void CG_WeaponBeamEffect( centity_t *cent ) {
	if( !cent->localEffects[LOCALEFFECT_EV_WEAPONBEAM] ) {
		return;
	}

	// now find the projection source for the beam we will draw
	orientation_t projection;
	if( !CG_PModel_GetProjectionSource( cent->current.number, &projection ) ) {
		VectorCopy( cent->laserOrigin, projection.origin );
	}

	CG_EBBeam( FromQF3( projection.origin ), FromQF3( cent->laserPoint ), cent->current.team );

	cent->localEffects[LOCALEFFECT_EV_WEAPONBEAM] = 0;
}

static centity_t *laserOwner = NULL;

static void BulletSparks( Vec3 pos, Vec3 normal, Vec4 color, int num_particles ) {
	float num_yellow_particles = num_particles / 4.0f;

	{
		ParticleEmitter emitter = { };
		emitter.position = pos;

		if( Length( normal ) == 0.0f ) {
			emitter.use_cone_direction = true;
			emitter.direction_cone.normal = normal;
			emitter.direction_cone.theta = 90.0f;
		}

		emitter.start_speed = 128.0f;
		emitter.end_speed = 128.0f;

		emitter.start_color = color;

		emitter.start_size = 2.0f;
		emitter.end_size = 0.0f;

		emitter.lifetime = 0.5f;

		emitter.n = num_particles - num_yellow_particles;

		EmitParticles( &cgs.bullet_sparks, emitter );
	}

	{
		ParticleEmitter emitter = { };
		emitter.position = pos;

		if( Length( normal ) == 0.0f ) {
			emitter.use_cone_direction = true;
			emitter.direction_cone.normal = normal;
			emitter.direction_cone.theta = 90.0f;
		}

		emitter.start_speed = 128.0f;
		emitter.end_speed = 128.0f;

		emitter.start_color = Vec4( 1.0f, 0.9, 0.0f, 0.5f );

		emitter.start_size = 2.0f;
		emitter.end_size = 0.0f;

		emitter.lifetime = 0.5f;

		emitter.n = num_yellow_particles;

		EmitParticles( &cgs.bullet_sparks, emitter );
	}
}

static void BulletImpact( const trace_t * trace, Vec4 color, int num_particles ) {
	CG_BulletExplosion( trace->endpos, NULL, trace );
	BulletSparks( FromQF3( trace->endpos ), FromQF3( trace->plane.normal ), color, num_particles );
}

static void WallbangImpact( const trace_t * trace, int num_particles ) {
	if( ( trace->contents & CONTENTS_WALLBANGABLE ) == 0 )
		return;

	ParticleEmitter emitter = { };
	emitter.position = FromQF3( trace->endpos );

	emitter.use_cone_direction = true;
	emitter.direction_cone.normal = FromQF3( trace->plane.normal );
	emitter.direction_cone.theta = 90.0f;

	emitter.start_speed = 128.0f;
	emitter.end_speed = 128.0f;

	emitter.start_color = Vec4( 1.0f, 0.9, 0.0f, 0.5f );

	emitter.start_size = 2.0f;
	emitter.end_size = 0.0f;

	emitter.lifetime = 0.5f;

	emitter.n = num_particles;

	EmitParticles( &cgs.bullet_sparks, emitter );
}

static void _LaserImpact( const trace_t *trace, const vec3_t dir ) {
	if( !trace || trace->ent < 0 ) {
		return;
	}

	Vec4 color = CG_TeamColorVec4( laserOwner->current.team );

	if( laserOwner ) {
#define TRAILTIME ( (int)( 1000.0f / 20.0f ) ) // density as quantity per second

		if( laserOwner->localEffects[LOCALEFFECT_LASERBEAM_SMOKE_TRAIL] + TRAILTIME < cl.serverTime ) {
			laserOwner->localEffects[LOCALEFFECT_LASERBEAM_SMOKE_TRAIL] = cl.serverTime;

			// CG_HighVelImpactPuffParticles( trace->endpos, trace->plane.normal, 8, 0.5f, color[ 0 ], color[ 1 ], color[ 2 ], color[ 3 ], NULL );

			S_StartFixedSound( cgs.media.sfxLasergunHit, FromQF3( trace->endpos ), CHAN_AUTO, 1.0f );
		}
#undef TRAILTIME
	}

	// it's a brush model
	if( trace->ent == 0 || !( cg_entities[trace->ent].current.effects & EF_TAKEDAMAGE ) ) {
		CG_LaserGunImpact( trace->endpos, 15.0f, dir, RGBA8( color ) );
		// CG_AddLightToScene( trace->endpos, 100, color[ 0 ], color[ 1 ], color[ 2 ] );
		BulletImpact( trace, color, 1 );
		return;
	}

	// it's a player

	// TODO: add player-impact model
}

void CG_LaserBeamEffect( centity_t *cent ) {
	trace_t trace;
	orientation_t projectsource;
	vec3_t laserOrigin, laserAngles, laserPoint;

	if( cent->localEffects[LOCALEFFECT_LASERBEAM] <= cl.serverTime ) {
		if( cent->localEffects[LOCALEFFECT_LASERBEAM] ) {
			if( ISVIEWERENTITY( cent->current.number ) ) {
				S_StartGlobalSound( cgs.media.sfxLasergunStop, CHAN_AUTO, 1.0f );
			} else {
				S_StartEntitySound( cgs.media.sfxLasergunStop, cent->current.number, CHAN_AUTO, 1.0f );
			}
		}
		cent->localEffects[LOCALEFFECT_LASERBEAM] = 0;
		return;
	}

	laserOwner = cent;
	Vec4 color = CG_TeamColorVec4( laserOwner->current.team );

	// interpolate the positions
	bool firstPerson = ISVIEWERENTITY( cent->current.number ) && !cg.view.thirdperson;

	if( firstPerson ) {
		VectorCopy( cg.predictedPlayerState.pmove.origin, laserOrigin );
		laserOrigin[2] += cg.predictedPlayerState.viewheight;
		VectorCopy( cg.predictedPlayerState.viewangles, laserAngles );

		VectorLerp( cent->laserPointOld, cg.lerpfrac, cent->laserPoint, laserPoint );
	}
	else {
		VectorLerp( cent->laserOriginOld, cg.lerpfrac, cent->laserOrigin, laserOrigin );
		VectorLerp( cent->laserPointOld, cg.lerpfrac, cent->laserPoint, laserPoint );
		vec3_t dir;

		// make up the angles from the start and end points (s->angles is not so precise)
		VectorSubtract( laserPoint, laserOrigin, dir );
		VecToAngles( dir, laserAngles );
	}

	// trace the beam: for tracing we use the real beam origin
	float range = GS_GetWeaponDef( Weapon_Laser )->range;
	GS_TraceLaserBeam( &client_gs, &trace, laserOrigin, laserAngles, range, cent->current.number, 0, _LaserImpact );

	// draw the beam: for drawing we use the weapon projection source (already handles the case of viewer entity)
	if( CG_PModel_GetProjectionSource( cent->current.number, &projectsource ) ) {
		VectorCopy( projectsource.origin, laserOrigin );
	}

	Vec3 start = FromQF3( laserOrigin );
	Vec3 end = FromQF3( trace.endpos );
	DrawBeam( start, end, 16.0f, color, cgs.media.shaderLGBeam );

	cent->sound = S_ImmediateEntitySound( cgs.media.sfxLasergunHum, cent->current.number, 1.0f, cent->sound );

	if( ISVIEWERENTITY( cent->current.number ) ) {
		cent->lg_beam_sound = S_ImmediateEntitySound( cgs.media.sfxLasergunBeam, cent->current.number, 1.0f, cent->lg_beam_sound );
	}
	else {
		cent->lg_beam_sound = S_ImmediateLineSound( cgs.media.sfxLasergunBeam, start, end, 1.0f, cent->lg_beam_sound );
	}

	laserOwner = NULL;
}

static void CG_Event_LaserBeam( const vec3_t origin, const vec3_t dir, int entNum ) {
	// lasergun's smooth refire
	// it appears that 64ms is that maximum allowed time interval between prediction events on localhost
	unsigned int range = Max2( GS_GetWeaponDef( Weapon_Laser )->refire_time + 10, 65u );

	centity_t *cent = &cg_entities[entNum];
	VectorCopy( origin, cent->laserOrigin );
	VectorMA( cent->laserOrigin, GS_GetWeaponDef( Weapon_Laser )->range, dir, cent->laserPoint );

	VectorCopy( cent->laserOrigin, cent->laserOriginOld );
	VectorCopy( cent->laserPoint, cent->laserPointOld );
	cent->localEffects[LOCALEFFECT_LASERBEAM] = cl.serverTime + range;
}

/*
* CG_FireWeaponEvent
*/
static void CG_FireWeaponEvent( int entNum, WeaponType weapon ) {
	const WeaponModelMetadata * weaponInfo = cgs.weaponInfos[ weapon ];
	const SoundEffect * sfx = weaponInfo->fire_sound;

	if( sfx ) {
		if( ISVIEWERENTITY( entNum ) ) {
			S_StartGlobalSound( sfx, CHAN_AUTO, 1.0f );
		} else {
			// fixed position is better for location, but the channels are used from worldspawn
			// and openal runs out of channels quick on cheap cards. Relative sound uses per-entity channels.
			S_StartEntitySound( sfx, entNum, CHAN_AUTO, 1.0f );
		}
	}

	// add animation to the player model
	switch( weapon ) {
		case Weapon_Knife:
			CG_PModel_AddAnimation( entNum, 0, TORSO_SHOOT_BLADE, 0, EVENT_CHANNEL );
			break;

		case Weapon_Pistol:
		case Weapon_Laser:
		case Weapon_Deagle:
			CG_PModel_AddAnimation( entNum, 0, TORSO_SHOOT_PISTOL, 0, EVENT_CHANNEL );
			break;

		case Weapon_MachineGun:
		case Weapon_Shotgun:
		case Weapon_Plasma:
			CG_PModel_AddAnimation( entNum, 0, TORSO_SHOOT_LIGHTWEAPON, 0, EVENT_CHANNEL );
			break;

		case Weapon_AssaultRifle:
		case Weapon_RocketLauncher:
		case Weapon_GrenadeLauncher:
			CG_PModel_AddAnimation( entNum, 0, TORSO_SHOOT_HEAVYWEAPON, 0, EVENT_CHANNEL );
			break;

		case Weapon_Railgun:
		case Weapon_Sniper:
		case Weapon_Rifle:
			CG_PModel_AddAnimation( entNum, 0, TORSO_SHOOT_AIMWEAPON, 0, EVENT_CHANNEL );
			break;
	}

	// add animation to the view weapon model
	if( ISVIEWERENTITY( entNum ) && !cg.view.thirdperson ) {
		CG_ViewWeapon_StartAnimationEvent( WEAPANIM_ATTACK );
	}

	// recoil
	if( ISVIEWERENTITY( entNum ) && cg.view.playerPrediction ) {
		if( !cg.recoiling ) {
			cg.recoil_initial_pitch[ 0 ] = cl.viewangles[ PITCH ];
			cg.recoil_initial_pitch[ 1 ] = cl.viewangles[ YAW ];
			cg.recoiling = true;
		}

		const WeaponDef * weap = GS_GetWeaponDef( weapon );
		float rand = random_float11( &cls.rng ) * weap->recoil_rand;
		rand += ( rand > 0 ? 1.f - weap->recoil_rand : weap->recoil_rand - 1.f );
		cg.recoil[ 0 ] += weap->v_recoil * ( rand >= 0 ? rand : -rand );
		cg.recoil[ 1 ] += weap->h_recoil * rand;

		cg.recoil_sign[ 0 ] = cg.recoil[ 0 ] >= 0 ? 1 : -1;
		cg.recoil_sign[ 1 ] = cg.recoil[ 1 ] >= 0 ? 1 : -1;
	}
}

/*
* CG_LeadWaterSplash
*/
static void CG_LeadWaterSplash( trace_t *tr ) {
	int contents;
	float *dir, *pos;

	contents = tr->contents;
	pos = tr->endpos;
	dir = tr->plane.normal;

	if( contents & CONTENTS_WATER ) {
		// CG_ParticleEffect( pos, dir, 0.47f, 0.48f, 0.8f, 8 );
	} else if( contents & CONTENTS_SLIME ) {
		// CG_ParticleEffect( pos, dir, 0.0f, 1.0f, 0.0f, 8 );
	} else if( contents & CONTENTS_LAVA ) {
		// CG_ParticleEffect( pos, dir, 1.0f, 0.67f, 0.0f, 8 );
	}
}

/*
* CG_LeadBubbleTrail
*/
static void CG_LeadBubbleTrail( trace_t *tr, vec3_t water_start ) {
	// if went through water, determine where the end and make a bubble trail
	vec3_t dir, pos;

	VectorSubtract( tr->endpos, water_start, dir );
	VectorNormalize( dir );
	VectorMA( tr->endpos, -2, dir, pos );

	if( CG_PointContents( pos ) & MASK_WATER ) {
		VectorCopy( pos, tr->endpos );
	} else {
		CG_Trace( tr, pos, vec3_origin, vec3_origin, water_start, tr->ent, MASK_WATER );
	}

	VectorAdd( water_start, tr->endpos, pos );
	VectorScale( pos, 0.5, pos );

	CG_BubbleTrail( water_start, tr->endpos, 32 );
}

static void CG_Event_FireBullet( const vec3_t origin, const vec3_t dir, WeaponType weapon, int owner, int team ) {
	Vec4 color = CG_TeamColorVec4( team );

	int range = GS_GetWeaponDef( weapon )->range;

	vec3_t right, up;
	ViewVectors( dir, right, up );

	trace_t trace, wallbang;
	GS_TraceBullet( &client_gs, &trace, &wallbang, origin, dir, right, up, 0, 0, range, owner, 0 );

	if( trace.ent != -1 && !( trace.surfFlags & SURF_NOIMPACT ) ) {
		if( trace.surfFlags & SURF_FLESH || ( trace.ent > 0 && cg_entities[trace.ent].current.type == ET_PLAYER ) ) {
			// flesh impact sound
		}
		else {
			BulletImpact( &trace, color, 24 );
			S_StartFixedSound( cgs.media.sfxBulletImpact, FromQF3( trace.endpos ), CHAN_AUTO, 1.0f );

			if( !ISVIEWERENTITY( owner ) ) {
				S_StartLineSound( cgs.media.sfxBulletWhizz, FromQF3( origin ), FromQF3( trace.endpos ), CHAN_AUTO, 1.0f );
			}
		}

		WallbangImpact( &wallbang, 12 );
	}

	orientation_t projection;
	if( !CG_PModel_GetProjectionSource( owner, &projection ) ) {
		VectorCopy( origin, projection.origin );
	}

	if( weapon != Weapon_Pistol ) {
		AddPersistentBeam( FromQF3( projection.origin ), FromQF3( trace.endpos ), 1.0f, color, cgs.media.shaderTracer, 0.2f, 0.1f );
	}
}

/*
* CG_Fire_SunflowerPattern
*/
static void CG_Fire_SunflowerPattern( vec3_t start, vec3_t dir, int owner, int team, int count, int spread, int range ) {
	Vec4 color = CG_TeamColorVec4( team );
	vec3_t right, up;
	ViewVectors( dir, right, up );

	orientation_t projection;
	if( !CG_PModel_GetProjectionSource( owner, &projection ) ) {
		VectorCopy( start, projection.origin );
	}

	for( int i = 0; i < count; i++ ) {
		float fi = i * 2.4f; //magic value creating Fibonacci numbers
		float r = cosf( fi ) * spread * sqrtf( fi );
		float u = sinf( fi ) * spread * sqrtf( fi );

		trace_t trace, wallbang;
		GS_TraceBullet( &client_gs, &trace, &wallbang, start, dir, right, up, r, u, range, owner, 0 );

		if( trace.ent != -1 && !( trace.surfFlags & SURF_NOIMPACT ) ) {
			BulletImpact( &trace, color, 4 );
		}

		WallbangImpact( &wallbang, 2 );

		AddPersistentBeam( FromQF3( projection.origin ), FromQF3( trace.endpos ), 1.0f, color, cgs.media.shaderTracer, 0.2f, 0.1f );
	}
}

/*
* CG_Event_FireRiotgun
*/
static void CG_Event_FireRiotgun( vec3_t origin, vec3_t dir, int owner, int team ) {
	const WeaponDef * def = GS_GetWeaponDef( Weapon_Shotgun );

	CG_Fire_SunflowerPattern( origin, dir, owner, team, def->projectile_count, def->spread, def->range );

	// spawn a single sound at the impact
	vec3_t end;
	VectorMA( origin, def->range, dir, end );

	trace_t trace;
	CG_Trace( &trace, origin, vec3_origin, vec3_origin, end, owner, MASK_SHOT );

	if( trace.ent != -1 && !( trace.surfFlags & SURF_NOIMPACT ) ) {
		S_StartFixedSound( cgs.media.sfxRiotgunHit, FromQF3( trace.endpos ), CHAN_AUTO, 1.0f );
	}
}


//==================================================================

//=========================================================
#define CG_MAX_ANNOUNCER_EVENTS 32
#define CG_MAX_ANNOUNCER_EVENTS_MASK ( CG_MAX_ANNOUNCER_EVENTS - 1 )
#define CG_ANNOUNCER_EVENTS_FRAMETIME 1500 // the announcer will speak each 1.5 seconds
typedef struct cg_announcerevent_s
{
	const SoundEffect *sound;
} cg_announcerevent_t;
cg_announcerevent_t cg_announcerEvents[CG_MAX_ANNOUNCER_EVENTS];
static int cg_announcerEventsCurrent = 0;
static int cg_announcerEventsHead = 0;
static int cg_announcerEventsDelay = 0;

/*
* CG_ClearAnnouncerEvents
*/
void CG_ClearAnnouncerEvents( void ) {
	cg_announcerEventsCurrent = cg_announcerEventsHead = 0;
}

/*
* CG_AddAnnouncerEvent
*/
void CG_AddAnnouncerEvent( const SoundEffect *sound, bool queued ) {
	if( !sound ) {
		return;
	}

	if( !queued ) {
		S_StartLocalSound( sound, CHAN_AUTO, cg_volume_announcer->value );
		cg_announcerEventsDelay = CG_ANNOUNCER_EVENTS_FRAMETIME; // wait
		return;
	}

	if( cg_announcerEventsCurrent + CG_MAX_ANNOUNCER_EVENTS >= cg_announcerEventsHead ) {
		// full buffer (we do nothing, just let it overwrite the oldest
	}

	// add it
	cg_announcerEvents[cg_announcerEventsHead & CG_MAX_ANNOUNCER_EVENTS_MASK].sound = sound;
	cg_announcerEventsHead++;
}

/*
* CG_ReleaseAnnouncerEvents
*/
void CG_ReleaseAnnouncerEvents( void ) {
	// see if enough time has passed
	cg_announcerEventsDelay -= cls.realFrameTime;
	if( cg_announcerEventsDelay > 0 ) {
		return;
	}

	if( cg_announcerEventsCurrent < cg_announcerEventsHead ) {
		const SoundEffect * sound = cg_announcerEvents[cg_announcerEventsCurrent & CG_MAX_ANNOUNCER_EVENTS_MASK].sound;
		S_StartLocalSound( sound, CHAN_AUTO, cg_volume_announcer->value );
		cg_announcerEventsDelay = CG_ANNOUNCER_EVENTS_FRAMETIME; // wait
		cg_announcerEventsCurrent++;
	} else {
		cg_announcerEventsDelay = 0; // no wait
	}
}

/*
* CG_StartVoiceTokenEffect
*/
static void CG_StartVoiceTokenEffect( int entNum, int vsay ) {
	if( !cg_voiceChats->integer ) {
		return;
	}
	if( vsay < 0 || vsay >= Vsay_Total ) {
		return;
	}

	centity_t *cent = &cg_entities[entNum];

	// ignore repeated/flooded events
	// TODO: this should really look at how long the vsay is...
	if( cent->localEffects[LOCALEFFECT_VSAY_TIMEOUT] > cl.serverTime ) {
		return;
	}

	cent->localEffects[LOCALEFFECT_VSAY_TIMEOUT] = cl.serverTime + VSAY_TIMEOUT;

	// play the sound
	const SoundEffect * sound = cgs.media.sfxVSaySounds[vsay];
	if( !sound ) {
		return;
	}

	// played as it was made by the 1st person player
	if( GS_MatchState( &client_gs ) >= MATCH_STATE_POSTMATCH )
		S_StartGlobalSound( sound, CHAN_AUTO, 1.0f );
	else
		S_StartEntitySound( sound, entNum, CHAN_AUTO, 1.0f );
}

//==================================================================

//==================================================================

/*
* CG_Event_Fall
*/
void CG_Event_Fall( const SyncEntityState * state, u64 parm ) {
	if( ISVIEWERENTITY( state->number ) ) {
		CG_StartFallKickEffect( ( parm + 5 ) * 10 );
	}

	vec3_t mins, maxs;
	CG_BBoxForEntityState( state, mins, maxs );

	vec3_t ground_position;
	VectorCopy( state->origin, ground_position );
	ground_position[ 2 ] += mins[ 2 ];

	if( parm < 40 )
		return;

	float volume = ( parm - 40 ) / 300.0f;
	if( ISVIEWERENTITY( state->number ) ) {
		S_StartLocalSound( cgs.media.sfxFall, CHAN_AUTO, volume );
	}
	else {
		S_StartEntitySound( cgs.media.sfxFall, state->number, CHAN_AUTO, volume );
	}
}

/*
* CG_Event_Pain
*/
static void CG_Event_Pain( SyncEntityState *state, u64 parm ) {
	constexpr PlayerSound sounds[] = { PlayerSound_Pain25, PlayerSound_Pain50, PlayerSound_Pain75, PlayerSound_Pain100 };
	if( parm >= ARRAY_COUNT( sounds ) )
		return;

	CG_PlayerSound( state->number, CHAN_AUTO, sounds[ parm ] );
	constexpr int animations[] = { TORSO_PAIN1, TORSO_PAIN2, TORSO_PAIN3 };
	CG_PModel_AddAnimation( state->number, 0, random_select( &cls.rng, animations ), 0, EVENT_CHANNEL );
}

/*
* CG_Event_Die
*/
static void CG_Event_Die( int entNum, u64 parm ) {
	constexpr struct { int dead, dying; } animations[] = {
		{ BOTH_DEAD1, BOTH_DEATH1 },
		{ BOTH_DEAD2, BOTH_DEATH2 },
		{ BOTH_DEAD3, BOTH_DEATH3 },
	};
	parm %= ARRAY_COUNT( animations );

	CG_PlayerSound( entNum, CHAN_AUTO, PlayerSound_Death );
	CG_PModel_AddAnimation( entNum, animations[ parm ].dead, animations[ parm ].dead, ANIM_NONE, BASE_CHANNEL );
	CG_PModel_AddAnimation( entNum, animations[ parm ].dying, animations[ parm ].dying, ANIM_NONE, EVENT_CHANNEL );
}

/*
* CG_Event_Dash
*/
void CG_Event_Dash( SyncEntityState *state, u64 parm ) {
	constexpr int animations[] = { LEGS_DASH, LEGS_DASH_LEFT, LEGS_DASH_RIGHT, LEGS_DASH_BACK };
	if( parm >= ARRAY_COUNT( animations ) )
		return;

	CG_PModel_AddAnimation( state->number, animations[ parm ], 0, 0, EVENT_CHANNEL );
	CG_PlayerSound( state->number, CHAN_BODY, PlayerSound_Dash );
	CG_Dash( state ); // Dash smoke effect

	// since most dash animations jump with right leg, reset the jump to start with left leg after a dash
	cg_entities[state->number].jumpedLeft = true;
}

/*
* CG_Event_WallJump
*/
void CG_Event_WallJump( SyncEntityState *state, u64 parm, int ev ) {
	vec3_t normal, forward, right;

	ByteToDir( parm, normal );

	AngleVectors( tv( state->angles[0], state->angles[1], 0 ), forward, right, NULL );

	if( DotProduct( normal, right ) > 0.3 ) {
		CG_PModel_AddAnimation( state->number, LEGS_WALLJUMP_RIGHT, 0, 0, EVENT_CHANNEL );
	} else if( -DotProduct( normal, right ) > 0.3 ) {
		CG_PModel_AddAnimation( state->number, LEGS_WALLJUMP_LEFT, 0, 0, EVENT_CHANNEL );
	} else if( -DotProduct( normal, forward ) > 0.3 ) {
		CG_PModel_AddAnimation( state->number, LEGS_WALLJUMP_BACK, 0, 0, EVENT_CHANNEL );
	} else {
		CG_PModel_AddAnimation( state->number, LEGS_WALLJUMP, 0, 0, EVENT_CHANNEL );
	}

	CG_PlayerSound( state->number, CHAN_BODY, PlayerSound_WallJump );

	
	vec3_t pos;
	VectorCopy( state->origin, pos );
	pos[2] += 15;
	CG_DustCircle( pos, normal, 65, 12 );
}

static void CG_PlayJumpSound( const SyncEntityState * state ) {
	CG_PlayerSound( state->number, CHAN_BODY, PlayerSound_Jump );
}

/*
* CG_Event_DoubleJump
*/
static void CG_Event_DoubleJump( SyncEntityState * state ) {
	CG_PlayJumpSound( state );
}

/*
* CG_Event_Jump
*/
static void CG_Event_Jump( SyncEntityState * state ) {
	CG_PlayJumpSound( state );

	centity_t *cent = &cg_entities[state->number];
	float xyspeedcheck = VectorLength( tv( cent->animVelocity[0], cent->animVelocity[1], 0 ) );
	if( xyspeedcheck < 100 ) { // the player is jumping on the same place, not running
		CG_PModel_AddAnimation( state->number, LEGS_JUMP_NEUTRAL, 0, 0, EVENT_CHANNEL );
	} else {
		vec3_t movedir;
		mat3_t viewaxis;

		movedir[0] = cent->animVelocity[0];
		movedir[1] = cent->animVelocity[1];
		movedir[2] = 0;
		VectorNormalize( movedir );

		Matrix3_FromAngles( tv( 0, cent->current.angles[YAW], 0 ), viewaxis );

		// see what's his relative movement direction
		constexpr float MOVEDIREPSILON = 0.25f;
		if( DotProduct( movedir, &viewaxis[AXIS_FORWARD] ) > MOVEDIREPSILON ) {
			cent->jumpedLeft = !cent->jumpedLeft;
			if( !cent->jumpedLeft ) {
				CG_PModel_AddAnimation( state->number, LEGS_JUMP_LEG2, 0, 0, EVENT_CHANNEL );
			} else {
				CG_PModel_AddAnimation( state->number, LEGS_JUMP_LEG1, 0, 0, EVENT_CHANNEL );
			}
		} else {
			CG_PModel_AddAnimation( state->number, LEGS_JUMP_NEUTRAL, 0, 0, EVENT_CHANNEL );
		}
	}
}

/*
* CG_EntityEvent
*/
void CG_EntityEvent( SyncEntityState *ent, int ev, u64 parm, bool predicted ) {
	vec3_t dir;
	bool viewer = ISVIEWERENTITY( ent->number );
	int count = 0;

	if( viewer && ev < PREDICTABLE_EVENTS_MAX && predicted != cg.view.playerPrediction ) {
		return;
	}

	switch( ev ) {
		default:
			break;

		//  PREDICTABLE EVENTS

		case EV_WEAPONACTIVATE: {
			WeaponType weapon = parm >> 1;
			bool silent = ( parm & 1 ) != 0;
			if( predicted ) {
				cg_entities[ ent->number ].current.weapon = weapon;
				CG_ViewWeapon_RefreshAnimation( &cg.weapon );
			}

			if( !silent ) {
				CG_PModel_AddAnimation( ent->number, 0, TORSO_WEAPON_SWITCHIN, 0, EVENT_CHANNEL );

				const SoundEffect * sfx = cgs.weaponInfos[ weapon ]->up_sound;
				if( viewer ) {
					S_StartGlobalSound( sfx, CHAN_AUTO, 1.0f );
				}
				else {
					S_StartFixedSound( sfx, FromQF3( ent->origin ), CHAN_AUTO, 1.0f );
				}
			}
		} break;

		case EV_SMOOTHREFIREWEAPON: // the server never sends this event
			if( predicted ) {
				cg_entities[ent->number].current.weapon = parm;

				CG_ViewWeapon_RefreshAnimation( &cg.weapon );

				if( parm == Weapon_Laser ) {
					vec3_t origin;
					VectorCopy( cg.predictedPlayerState.pmove.origin, origin );
					origin[2] += cg.predictedPlayerState.viewheight;
					AngleVectors( cg.predictedPlayerState.viewangles, dir, NULL, NULL );

					CG_Event_LaserBeam( origin, dir, ent->number );
				}
			}
			break;

		case EV_FIREWEAPON: {
			if( parm <= Weapon_None || parm >= Weapon_Count )
				return;

			// check the owner for predicted case
			if( ISVIEWERENTITY( ent->ownerNum ) && ev < PREDICTABLE_EVENTS_MAX && predicted != cg.view.playerPrediction ) {
				return;
			}

			if( predicted ) {
				cg_entities[ent->number].current.weapon = parm;
			}

			int num;
			vec3_t origin, angles;
			if( predicted ) {
				num = ent->number;
				VectorCopy( cg.predictedPlayerState.pmove.origin, origin );
				origin[ 2 ] += cg.predictedPlayerState.viewheight;
				VectorCopy( cg.predictedPlayerState.viewangles, angles );
			}
			else {
				num = ent->ownerNum;
				VectorCopy( ent->origin, origin );
				VectorCopy( ent->origin2, angles );
			}

			CG_FireWeaponEvent( num, parm );

			AngleVectors( angles, dir, NULL, NULL );

			if( parm == Weapon_Railgun ) {
				CG_Event_WeaponBeam( origin, dir, num );
			}
			else if( parm == Weapon_Shotgun ) {
				CG_Event_FireRiotgun( origin, dir, num, ent->team );
			}
			else if( parm == Weapon_Laser ) {
				CG_Event_LaserBeam( origin, dir, num );
			}
			else if( parm == Weapon_Pistol || parm == Weapon_MachineGun || parm == Weapon_Deagle || parm == Weapon_AssaultRifle ) {
				CG_Event_FireBullet( origin, dir, parm, num, ent->team );
			}
		} break;

		case EV_NOAMMOCLICK:
			if( viewer ) {
				S_StartGlobalSound( cgs.media.sfxWeaponNoAmmo, CHAN_AUTO, 1.0f );
			}
			else {
				S_StartFixedSound( cgs.media.sfxWeaponNoAmmo, FromQF3( ent->origin ), CHAN_AUTO, 1.0f );
			}
			break;

		case EV_ZOOM_IN:
		case EV_ZOOM_OUT: {
			if( parm <= Weapon_None || parm >= Weapon_Count )
				return;

			const WeaponModelMetadata * weapon = cgs.weaponInfos[ parm ];
			const SoundEffect * sfx = ev == EV_ZOOM_IN ? weapon->zoom_in_sound : weapon->zoom_out_sound;

			if( viewer ) {
				S_StartGlobalSound( sfx, CHAN_AUTO, 1.0f );
			}
			else {
				S_StartFixedSound( sfx, FromQF3( ent->origin ), CHAN_AUTO, 1.0f );
			}
		} break;

		case EV_DASH:
			CG_Event_Dash( ent, parm );
			break;

		case EV_WALLJUMP:
			CG_Event_WallJump( ent, parm, ev );
			break;

		case EV_DOUBLEJUMP:
			CG_Event_DoubleJump( ent );
			break;

		case EV_JUMP:
			CG_Event_Jump( ent );
			break;

		case EV_JUMP_PAD:
			CG_PlayJumpSound( ent );
			CG_PModel_AddAnimation( ent->number, LEGS_JUMP_NEUTRAL, 0, 0, EVENT_CHANNEL );
			break;

		case EV_FALL:
			CG_Event_Fall( ent, parm );
			break;

		//  NON PREDICTABLE EVENTS

		case EV_WEAPONDROP: // deactivate is not predictable
			CG_PModel_AddAnimation( ent->number, 0, TORSO_WEAPON_SWITCHOUT, 0, EVENT_CHANNEL );
			break;

		case EV_PAIN:
			CG_Event_Pain( ent, parm );
			break;

		case EV_DIE:
			CG_Event_Die( ent->ownerNum, parm );
			break;

		case EV_EXPLOSION1:
			CG_GenericExplosion( ent->origin, vec3_origin, parm * 8 );
			break;

		case EV_EXPLOSION2:
			CG_GenericExplosion( ent->origin, vec3_origin, parm * 16 );
			break;

		case EV_SPARKS:
			ByteToDir( parm, dir );
			if( ent->damage > 0 ) {
				count = Clamp( 1, int( ent->damage * 0.25f ), 10 );
			} else {
				count = 6;
			}

			// CG_ParticleEffect( ent->origin, dir, 1.0f, 0.67f, 0.0f, count );
			break;

		case EV_LASER_SPARKS:
			ByteToDir( parm, dir );
			// CG_ParticleEffect2( ent->origin, dir,
			// 					COLOR_R( ent->colorRGBA ) * ( 1.0 / 255.0 ),
			// 					COLOR_G( ent->colorRGBA ) * ( 1.0 / 255.0 ),
			// 					COLOR_B( ent->colorRGBA ) * ( 1.0 / 255.0 ),
			// 					ent->counterNum );
			break;

		case EV_SPOG:
			SpawnGibs( FromQF3( ent->origin ), FromQF3( ent->origin2 ), parm, ent->team );
			break;

		case EV_PLAYER_RESPAWN:
			if( (unsigned)ent->ownerNum == cgs.playerNum + 1 ) {
				CG_ResetKickAngles();
			}

			if( ent->ownerNum && ent->ownerNum < client_gs.maxclients + 1 ) {
				cg_entities[ent->ownerNum].localEffects[LOCALEFFECT_EV_PLAYER_TELEPORT_IN] = cl.serverTime;
				VectorCopy( ent->origin, cg_entities[ent->ownerNum].teleportedTo );
			}
			break;

		case EV_PLAYER_TELEPORT_IN:
			S_StartFixedSound( cgs.media.sfxTeleportIn, FromQF3( ent->origin ), CHAN_AUTO, 1.0f );

			if( ent->ownerNum && ent->ownerNum < client_gs.maxclients + 1 ) {
				cg_entities[ent->ownerNum].localEffects[LOCALEFFECT_EV_PLAYER_TELEPORT_IN] = cl.serverTime;
				VectorCopy( ent->origin, cg_entities[ent->ownerNum].teleportedTo );
			}
			break;

		case EV_PLAYER_TELEPORT_OUT:
			S_StartFixedSound( cgs.media.sfxTeleportOut, FromQF3( ent->origin ), CHAN_AUTO, 1.0f );

			if( ent->ownerNum && ent->ownerNum < client_gs.maxclients + 1 ) {
				cg_entities[ent->ownerNum].localEffects[LOCALEFFECT_EV_PLAYER_TELEPORT_OUT] = cl.serverTime;
				VectorCopy( ent->origin, cg_entities[ent->ownerNum].teleportedFrom );
			}
			break;

		case EV_PLASMA_EXPLOSION:
			ByteToDir( parm, dir );
			CG_PlasmaExplosion( ent->origin, dir, ent->team, (float)ent->weapon * 8.0f );
			S_StartFixedSound( cgs.media.sfxPlasmaHit, FromQF3( ent->origin ), CHAN_AUTO, 1.0f );
			break;

		case EV_BOLT_EXPLOSION:
			ByteToDir( parm, dir );
			CG_EBImpact( ent->origin, dir, 0, ent->team );
			break;

		case EV_GRENADE_EXPLOSION:
			if( parm ) {
				// we have a direction
				ByteToDir( parm, dir );
				CG_GrenadeExplosionMode( ent->origin, dir, (float)ent->weapon * 8.0f, ent->team );
			} else {
				// no direction
				CG_GrenadeExplosionMode( ent->origin, vec3_origin, (float)ent->weapon * 8.0f, ent->team );
			}

			break;

		case EV_ROCKET_EXPLOSION:
			ByteToDir( parm, dir );
			CG_RocketExplosionMode( ent->origin, dir, (float)ent->weapon * 8.0f, ent->team );

			break;

		case EV_GRENADE_BOUNCE:
			S_StartEntitySound( cgs.media.sfxGrenadeBounce, ent->number, CHAN_AUTO, 1.0f );
			break;

		case EV_BLADE_IMPACT:
			CG_BladeImpact( ent->origin, ent->origin2 );
			break;

		case EV_RIFLEBULLET_IMPACT:
			ByteToDir( parm, dir );
			BulletSparks( FromQF3( ent->origin ), FromQF3( dir ), CG_TeamColorVec4( ent->team ), 24 );
			S_StartFixedSound( cgs.media.sfxBulletImpact, FromQF3( ent->origin ), CHAN_AUTO, 1.0f );
			break;

		case EV_BLOOD:
			ByteToDir( parm, dir );
			CG_BloodDamageEffect( ent->origin, dir, ent->damage, ent->team );
			break;

		// func movers
		case EV_PLAT_HIT_TOP:
		case EV_PLAT_HIT_BOTTOM:
		case EV_PLAT_START_MOVING:
		case EV_DOOR_HIT_TOP:
		case EV_DOOR_HIT_BOTTOM:
		case EV_DOOR_START_MOVING:
		case EV_BUTTON_FIRE:
		case EV_TRAIN_STOP:
		case EV_TRAIN_START:
		{
			vec3_t so;
			CG_GetEntitySpatilization( ent->number, so, NULL );
			S_StartFixedSound( FindSoundEffect( StringHash( parm ) ), FromQF3( so ), CHAN_AUTO, 1.0f );
		}
		break;

		case EV_VSAY:
			CG_StartVoiceTokenEffect( ent->ownerNum, parm );
			break;

		case EV_TBAG:
			S_StartFixedSound( cgs.media.sfxTbag, FromQF3( ent->origin ), CHAN_AUTO, parm / 255.0f );
			break;

		case EV_DAMAGE:
			CG_AddDamageNumber( ent );
			break;
	}
}

#define ISEARLYEVENT( ev ) ( ev == EV_WEAPONDROP )

/*
* CG_FireEvents
*/
static void CG_FireEntityEvents( bool early ) {
	int pnum, j;
	SyncEntityState *state;

	for( pnum = 0; pnum < cg.frame.numEntities; pnum++ ) {
		state = &cg.frame.parsedEntities[pnum & ( MAX_PARSE_ENTITIES - 1 )];

		if( state->type == ET_SOUNDEVENT ) {
			if( early ) {
				CG_SoundEntityNewState( &cg_entities[state->number] );
			}
			continue;
		}

		for( j = 0; j < 2; j++ ) {
			if( early == ISEARLYEVENT( state->events[j].type ) ) {
				CG_EntityEvent( state, state->events[j].type, state->events[j].parm, false );
			}
		}
	}
}

/*
* CG_FirePlayerStateEvents
* This events are only received by this client, and only affect it.
*/
static void CG_FirePlayerStateEvents( void ) {
	vec3_t dir;

	if( cg.view.POVent != (int)cg.frame.playerState.POVnum ) {
		return;
	}

	for( int count = 0; count < 2; count++ ) {
		u64 parm = cg.frame.playerState.events[ count ].parm;

		switch( cg.frame.playerState.events[ count ].type ) {
			case PSEV_HIT:
				if( parm > 6 ) {
					break;
				}
				if( parm < 4 ) { // hit of some caliber
					S_StartLocalSound( cgs.media.sfxWeaponHit[parm], CHAN_AUTO, cg_volume_hitsound->value );
					CG_ScreenCrosshairDamageUpdate();
				} else if( parm == 4 ) { // killed an enemy
					S_StartLocalSound( cgs.media.sfxWeaponKill, CHAN_AUTO, cg_volume_hitsound->value );
					CG_ScreenCrosshairDamageUpdate();
				} else { // hit a teammate
					S_StartLocalSound( cgs.media.sfxWeaponHitTeam, CHAN_AUTO, cg_volume_hitsound->value );
				}
				break;

			case PSEV_DAMAGE_10:
				ByteToDir( parm, dir );
				// CG_DamageIndicatorAdd( 10, dir );
				break;

			case PSEV_DAMAGE_20:
				ByteToDir( parm, dir );
				// CG_DamageIndicatorAdd( 20, dir );
				break;

			case PSEV_DAMAGE_30:
				ByteToDir( parm, dir );
				// CG_DamageIndicatorAdd( 30, dir );
				break;

			case PSEV_DAMAGE_40:
				ByteToDir( parm, dir );
				// CG_DamageIndicatorAdd( 40, dir );
				break;

			case PSEV_INDEXEDSOUND:
				S_StartGlobalSound( FindSoundEffect( StringHash( parm ) ), CHAN_AUTO, 1.0f );
				break;

			case PSEV_ANNOUNCER:
				CG_AddAnnouncerEvent( FindSoundEffect( StringHash( parm ) ), false );
				break;

			case PSEV_ANNOUNCER_QUEUED:
				CG_AddAnnouncerEvent( FindSoundEffect( StringHash( parm ) ), true );
				break;

			default:
				break;
		}
	}
}

/*
* CG_FireEvents
*/
void CG_FireEvents( bool early ) {
	if( !cg.fireEvents ) {
		return;
	}

	CG_FireEntityEvents( early );

	if( early ) {
		return;
	}

	CG_FirePlayerStateEvents();
	cg.fireEvents = false;
}
