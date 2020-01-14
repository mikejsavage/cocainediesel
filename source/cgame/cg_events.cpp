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
	VectorMA( origin, ELECTROBOLT_RANGE, dir, end );

	centity_t * owner = &cg_entities[ ownerNum ];

	// retrace to spawn wall impact
	trace_t trace;
	CG_Trace( &trace, origin, vec3_origin, vec3_origin, end, cg.view.POVent, MASK_SOLID );
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

static void _LaserImpact( trace_t *trace, vec3_t dir ) {
	if( !trace || trace->ent < 0 ) {
		return;
	}

	RGBA8 color = RGBA8( CG_TeamColor( laserOwner->current.team ) );

	if( laserOwner ) {
#define TRAILTIME ( (int)( 1000.0f / 20.0f ) ) // density as quantity per second

		if( laserOwner->localEffects[LOCALEFFECT_LASERBEAM_SMOKE_TRAIL] + TRAILTIME < cl.serverTime ) {
			laserOwner->localEffects[LOCALEFFECT_LASERBEAM_SMOKE_TRAIL] = cl.serverTime;

			// CG_HighVelImpactPuffParticles( trace->endpos, trace->plane.normal, 8, 0.5f, color[ 0 ], color[ 1 ], color[ 2 ], color[ 3 ], NULL );

			S_StartFixedSound( cgs.media.sfxLasergunHit, FromQF3( trace->endpos ), CHAN_AUTO,
									cg_volume_effects->value, ATTN_STATIC );
		}
#undef TRAILTIME
	}

	// it's a brush model
	if( trace->ent == 0 || !( cg_entities[trace->ent].current.effects & EF_TAKEDAMAGE ) ) {
		CG_LaserGunImpact( trace->endpos, 15.0f, dir, color );
		// CG_AddLightToScene( trace->endpos, 100, color[ 0 ], color[ 1 ], color[ 2 ] );
		return;
	}

	// it's a player

	// TODO: add player-impact model
}

void CG_LaserBeamEffect( centity_t *cent ) {
	const SoundEffect * sfx = NULL;
	float range;
	bool firstPerson;
	trace_t trace;
	orientation_t projectsource;
	vec3_t laserOrigin, laserAngles, laserPoint;

	if( cent->localEffects[LOCALEFFECT_LASERBEAM] <= cl.serverTime ) {
		if( cent->localEffects[LOCALEFFECT_LASERBEAM] ) {
			sfx = cgs.media.sfxLasergunStop;

			if( ISVIEWERENTITY( cent->current.number ) ) {
				S_StartGlobalSound( sfx, CHAN_AUTO, cg_volume_effects->value );
			} else {
				S_StartEntitySound( sfx, cent->current.number, CHAN_AUTO, cg_volume_effects->value, ATTN_NORM );
			}
		}
		cent->localEffects[LOCALEFFECT_LASERBEAM] = 0;
		return;
	}

	laserOwner = cent;
	Vec4 color = CG_TeamColorVec4( laserOwner->current.team );

	// interpolate the positions
	firstPerson = ISVIEWERENTITY( cent->current.number ) && !cg.view.thirdperson;

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

	range = GS_GetWeaponDef( Weapon_Laser )->range;

	sfx = cgs.media.sfxLasergunHum;

	// trace the beam: for tracing we use the real beam origin
	GS_TraceLaserBeam( &client_gs, &trace, laserOrigin, laserAngles, range, cent->current.number, 0, _LaserImpact );

	// draw the beam: for drawing we use the weapon projection source (already handles the case of viewer entity)
	if( CG_PModel_GetProjectionSource( cent->current.number, &projectsource ) ) {
		VectorCopy( projectsource.origin, laserOrigin );
	}

	Vec3 start = FromQF3( laserOrigin );
	Vec3 end = FromQF3( trace.endpos );
	DrawBeam( start, end, 16.0f, color, cgs.media.shaderLGBeam );

	// enable continuous flash on the weapon owner
	if( cg_weaponFlashes->integer ) {
		cg_entPModels[cent->current.number].flash_time = cl.serverTime + cgs.weaponInfos[ Weapon_Laser ]->flashTime;
	}

	if( ISVIEWERENTITY( cent->current.number ) ) {
		S_ImmediateEntitySound( sfx, cent->current.number, cg_volume_effects->value, ATTN_NONE );
	} else {
		S_ImmediateLineSound( sfx, cent->current.number, start, end, cg_volume_effects->value, ATTN_STATIC );
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
static void CG_FireWeaponEvent( int entNum, int weapon ) {
	// hack idle attenuation on the plasmagun to reduce sound flood on the scene
	float attenuation;
	if( weapon == Weapon_Plasma ) {
		attenuation = ATTN_IDLE;
	} else {
		attenuation = ATTN_NORM;
	}

	const weaponinfo_t * weaponInfo = cgs.weaponInfos[ weapon ];
	const SoundEffect * sfx = weaponInfo->sound_fire;

	if( sfx ) {
		if( ISVIEWERENTITY( entNum ) ) {
			S_StartGlobalSound( sfx, CHAN_AUTO, cg_volume_effects->value );
		} else {
			// fixed position is better for location, but the channels are used from worldspawn
			// and openal runs out of channels quick on cheap cards. Relative sound uses per-entity channels.
			S_StartEntitySound( sfx, entNum, CHAN_AUTO, cg_volume_effects->value, attenuation );
		}
	}

	// flash and barrel effects
	if( weapon == Weapon_Knife && weaponInfo->barrelTime ) {
		// start barrel rotation or offsetting
		cg_entPModels[entNum].barrel_time = cl.serverTime + weaponInfo->barrelTime;
	} else {
		// light flash
		if( cg_weaponFlashes->integer && weaponInfo->flashTime ) {
			cg_entPModels[entNum].flash_time = cl.serverTime + weaponInfo->flashTime;
		}

		// start barrel rotation or offsetting
		if( weaponInfo->barrelTime ) {
			cg_entPModels[entNum].barrel_time = cl.serverTime + weaponInfo->barrelTime;
		}
	}

	// add animation to the player model
	switch( weapon ) {
		case Weapon_Knife:
			CG_PModel_AddAnimation( entNum, 0, TORSO_SHOOT_BLADE, 0, EVENT_CHANNEL );
			break;

		case Weapon_Laser:
			CG_PModel_AddAnimation( entNum, 0, TORSO_SHOOT_PISTOL, 0, EVENT_CHANNEL );
			break;

		case Weapon_MachineGun:
		case Weapon_Shotgun:
		case Weapon_Plasma:
			CG_PModel_AddAnimation( entNum, 0, TORSO_SHOOT_LIGHTWEAPON, 0, EVENT_CHANNEL );
			break;

		case Weapon_RocketLauncher:
		case Weapon_GrenadeLauncher:
			CG_PModel_AddAnimation( entNum, 0, TORSO_SHOOT_HEAVYWEAPON, 0, EVENT_CHANNEL );
			break;

		case Weapon_Railgun:
			CG_PModel_AddAnimation( entNum, 0, TORSO_SHOOT_AIMWEAPON, 0, EVENT_CHANNEL );
			break;
	}

	// add animation to the view weapon model
	if( ISVIEWERENTITY( entNum ) && !cg.view.thirdperson ) {
		CG_ViewWeapon_StartAnimationEvent( weapon == Weapon_Knife ? WEAPANIM_ATTACK_WEAK : WEAPANIM_ATTACK_STRONG );
	}

	// recoil
	if( cg.view.playerPrediction ) {
		if( !cg.recoiling ) {
			cg.recoil_initial_pitch = cl.viewangles[ PITCH ];
			cg.recoiling = true;
		}
		cg.recoil += GS_GetWeaponDef( weapon )->recoil;
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

/*
* CG_BulletImpact
*/
static void CG_BulletImpact( trace_t *tr ) {
	// bullet impact
	CG_BulletExplosion( tr->endpos, NULL, tr );

	// throw particles on dust
	if( cg_particles->integer && ( tr->surfFlags & SURF_DUST ) ) {
		// CG_ParticleEffect( tr->endpos, tr->plane.normal, 0.30f, 0.30f, 0.25f, 1 );
	}
}

static void CG_Event_FireMachinegun( vec3_t origin, vec3_t dir, int owner, int team ) {
	Vec4 color = CG_TeamColorVec4( team );

	int range = GS_GetWeaponDef( Weapon_MachineGun )->range;

	vec3_t right, up;
	ViewVectors( dir, right, up );

	trace_t trace;
	trace_t * water_trace = GS_TraceBullet( &client_gs, &trace, origin, dir, right, up, 0, 0, range, owner, 0 );
	if( water_trace ) {
		if( !VectorCompare( water_trace->endpos, origin ) ) {
			CG_LeadWaterSplash( water_trace );
		}
	}

	if( trace.ent != -1 && !( trace.surfFlags & SURF_NOIMPACT ) ) {
		if( !water_trace ) {
			if( trace.surfFlags & SURF_FLESH || ( trace.ent > 0 && cg_entities[trace.ent].current.type == ET_PLAYER ) ) {
				// flesh impact sound
			}
			else {
				S_StartFixedSound( cgs.media.sfxBulletImpact, FromQF3( trace.endpos ), CHAN_AUTO, cg_volume_effects->value, ATTN_STATIC );

				ParticleEmitter emitter = { };
				emitter.position = FromQF3( trace.endpos );

				emitter.use_cone_direction = true;
				emitter.direction_cone.normal = FromQF3( trace.plane.normal );
				emitter.direction_cone.theta = 90.0f;

				emitter.start_speed = 128.0f;
				emitter.end_speed = 128.0f;

				emitter.start_color = color;
				emitter.end_color = Vec3( 0.0f );

				emitter.start_size = 2.0f;
				emitter.end_size = 0.0f;

				emitter.lifetime = 0.5f;

				emitter.n = 24;

				EmitParticles( &cgs.SMGsparks, emitter );
			}
		}
	}

	if( water_trace ) {
		CG_LeadBubbleTrail( &trace, water_trace->endpos );
	}

	orientation_t projection;
	if( !CG_PModel_GetProjectionSource( owner, &projection ) ) {
		VectorCopy( origin, projection.origin );
	}

	AddPersistentBeam( FromQF3( projection.origin ), FromQF3( trace.endpos ), 1.0f, color, cgs.media.shaderSMGtrail, 0.2f, 0.1f );
}

/*
* CG_Fire_SunflowerPattern
*/
static void CG_Fire_SunflowerPattern( vec3_t start, vec3_t dir, int ignore, int count,
									  int spread, int range, void ( *impact )( trace_t *tr ) ) {
	vec3_t right, up;
	ViewVectors( dir, right, up );

	for( int i = 0; i < count; i++ ) {
		float fi = i * 2.4f; //magic value creating Fibonacci numbers
		float r = cosf( fi ) * spread * sqrtf( fi );
		float u = sinf( fi ) * spread * sqrtf( fi );

		trace_t trace;
		trace_t * water_trace = GS_TraceBullet( &client_gs, &trace, start, dir, right, up, r, u, range, ignore, 0 );
		if( water_trace ) {
			trace_t *tr = water_trace;
			if( !VectorCompare( tr->endpos, start ) ) {
				CG_LeadWaterSplash( tr );
			}
		}

		if( trace.ent != -1 && !( trace.surfFlags & SURF_NOIMPACT ) ) {
			impact( &trace );
		}

		if( water_trace ) {
			CG_LeadBubbleTrail( &trace, water_trace->endpos );
		}
	}
}

/*
* CG_Event_FireRiotgun
*/
static void CG_Event_FireRiotgun( vec3_t origin, vec3_t dir, int owner ) {
	const WeaponDef * def = GS_GetWeaponDef( Weapon_Shotgun );

	CG_Fire_SunflowerPattern( origin, dir, owner, def->projectile_count, def->spread, def->range, CG_BulletImpact );

	// spawn a single sound at the impact
	vec3_t end;
	VectorMA( origin, def->range, dir, end );

	trace_t trace;
	CG_Trace( &trace, origin, vec3_origin, vec3_origin, end, owner, MASK_SHOT );

	if( trace.ent != -1 && !( trace.surfFlags & SURF_NOIMPACT ) ) {
		S_StartFixedSound( cgs.media.sfxRiotgunHit, FromQF3( trace.endpos ), CHAN_AUTO,
			cg_volume_effects->value, ATTN_IDLE );
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
	if( !cg_voiceChats->integer || cg_volume_voicechats->value <= 0.0f ) {
		return;
	}
	if( vsay < 0 || vsay >= VSAY_TOTAL ) {
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
		S_StartGlobalSound( sound, CHAN_AUTO, cg_volume_voicechats->value );
	else
		S_StartEntitySound( sound, entNum, CHAN_AUTO, cg_volume_voicechats->value, ATTN_DISTANT );
}

//==================================================================

//==================================================================

/*
* CG_Event_Fall
*/
void CG_Event_Fall( const SyncEntityState * state, int parm ) {
	if( ISVIEWERENTITY( state->number ) ) {
		CG_StartFallKickEffect( ( parm + 5 ) * 10 );
	}

	vec3_t mins, maxs;
	CG_BBoxForEntityState( state, mins, maxs );

	vec3_t ground_position;
	VectorCopy( state->origin, ground_position );
	ground_position[ 2 ] += mins[ 2 ];

	float volume = Max2(( parm - 40 ) * ( 1.0f / 300.0f ), 0.f);
	if( ISVIEWERENTITY( state->number ) ) {
		S_StartLocalSound( cgs.media.sfxFall, CHAN_AUTO, volume );
	}
	else {
		S_StartEntitySound( cgs.media.sfxFall, state->number, CHAN_AUTO, volume, state->attenuation );
	}
}

/*
* CG_Event_Pain
*/
static void CG_Event_Pain( SyncEntityState *state, int parm ) {
	constexpr PlayerSound sounds[] = { PlayerSound_Pain25, PlayerSound_Pain50, PlayerSound_Pain75, PlayerSound_Pain100 };
	CG_PlayerSound( state->number, CHAN_AUTO, sounds[ parm ], cg_volume_players->value, state->attenuation );
	constexpr int animations[] = { TORSO_PAIN1, TORSO_PAIN2, TORSO_PAIN3 };
	CG_PModel_AddAnimation( state->number, 0, random_select( &cls.rng, animations ), 0, EVENT_CHANNEL );
}

/*
* CG_Event_Die
*/
static void CG_Event_Die( int entNum, int parm ) {
	constexpr struct { int dead, dying; } animations[] = {
		{ BOTH_DEAD1, BOTH_DEATH1 },
		{ BOTH_DEAD2, BOTH_DEATH2 },
		{ BOTH_DEAD3, BOTH_DEATH3 },
	};
	parm %= ARRAY_COUNT( animations );

	CG_PlayerSound( entNum, CHAN_AUTO, PlayerSound_Death, cg_volume_players->value, ATTN_NORM );
	CG_PModel_AddAnimation( entNum, animations[ parm ].dead, animations[ parm ].dead, ANIM_NONE, BASE_CHANNEL );
	CG_PModel_AddAnimation( entNum, animations[ parm ].dying, animations[ parm ].dying, ANIM_NONE, EVENT_CHANNEL );
}

/*
* CG_Event_Dash
*/
void CG_Event_Dash( SyncEntityState *state, int parm ) {
	switch( parm ) {
		case 0: // dash front
			CG_PModel_AddAnimation( state->number, LEGS_DASH, 0, 0, EVENT_CHANNEL );
			break;
		case 1: // dash left
			CG_PModel_AddAnimation( state->number, LEGS_DASH_LEFT, 0, 0, EVENT_CHANNEL );
			break;
		case 2: // dash right
			CG_PModel_AddAnimation( state->number, LEGS_DASH_RIGHT, 0, 0, EVENT_CHANNEL );
			break;
		case 3: // dash back
			CG_PModel_AddAnimation( state->number, LEGS_DASH_BACK, 0, 0, EVENT_CHANNEL );
			break;
	}

	CG_PlayerSound( state->number, CHAN_BODY, PlayerSound_Dash, cg_volume_players->value, state->attenuation );

	CG_Dash( state ); // Dash smoke effect

	// since most dash animations jump with right leg, reset the jump to start with left leg after a dash
	cg_entities[state->number].jumpedLeft = true;
}

/*
* CG_Event_WallJump
*/
void CG_Event_WallJump( SyncEntityState *state, int parm, int ev ) {
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

	CG_PlayerSound( state->number, CHAN_BODY, PlayerSound_WallJump, cg_volume_players->value, state->attenuation );

	// smoke effect
	if( cg_cartoonEffects->integer & 1 ) {
		vec3_t pos;
		VectorCopy( state->origin, pos );
		pos[2] += 15;
		CG_DustCircle( pos, normal, 65, 12 );
	}
}

static void CG_PlayJumpSound( const SyncEntityState * state ) {
	CG_PlayerSound( state->number, CHAN_BODY, PlayerSound_Jump, cg_volume_players->value, state->attenuation );
}

/*
* CG_Event_DoubleJump
*/
void CG_Event_DoubleJump( SyncEntityState *state, int parm ) {
	CG_PlayJumpSound( state );
}

/*
* CG_Event_Jump
*/
void CG_Event_Jump( SyncEntityState *state, int parm ) {
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
void CG_EntityEvent( SyncEntityState *ent, int ev, int parm, bool predicted ) {
	vec3_t dir;
	bool viewer = ISVIEWERENTITY( ent->number );
	int count = 0;

	if( viewer && ( ev < PREDICTABLE_EVENTS_MAX ) && ( predicted != cg.view.playerPrediction ) ) {
		return;
	}

	switch( ev ) {
		case EV_NONE:
		default:
			break;

		//  PREDICTABLE EVENTS

		case EV_WEAPONACTIVATE: {
			int weapon = parm >> 1;
			bool silent = ( parm & 1 ) != 0;
			if( predicted ) {
				cg_entities[ent->number].current.weapon = weapon;
				CG_ViewWeapon_RefreshAnimation( &cg.weapon );
			}

			if( viewer ) {
				cg.predictedWeaponSwitch = Weapon_Count;
			}

			// reset weapon animation timers
			cg_entPModels[ent->number].flash_time = 0;
			cg_entPModels[ent->number].barrel_time = 0;

			if( !silent ) {
				CG_PModel_AddAnimation( ent->number, 0, TORSO_WEAPON_SWITCHIN, 0, EVENT_CHANNEL );

				if( viewer ) {
					S_StartGlobalSound( cgs.media.sfxWeaponUp, CHAN_AUTO, cg_volume_effects->value );
				} else {
					S_StartFixedSound( cgs.media.sfxWeaponUp, FromQF3( ent->origin ), CHAN_AUTO, cg_volume_effects->value, ATTN_NORM );
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
			if( parm < 0 || parm >= Weapon_Count )
				return;

			if( predicted ) {
				cg_entities[ent->number].current.weapon = parm;
			}

			CG_FireWeaponEvent( ent->number, parm );

			if( predicted ) {
				vec3_t origin;
				VectorCopy( cg.predictedPlayerState.pmove.origin, origin );
				origin[2] += cg.predictedPlayerState.viewheight;
				AngleVectors( cg.predictedPlayerState.viewangles, dir, NULL, NULL );

				if( parm == Weapon_Railgun ) {
					CG_Event_WeaponBeam( origin, dir, ent->number );
				}
				else if( parm == Weapon_Shotgun ) {
					CG_Event_FireRiotgun( origin, dir, ent->number );
				}
				else if( parm == Weapon_Laser ) {
					CG_Event_LaserBeam( origin, dir, ent->number );
				}
				else if( parm == Weapon_MachineGun ) {
					CG_Event_FireMachinegun( origin, dir, ent->number, ent->team );
				}
			}
		} break;

		case EV_ELECTROTRAIL:
			// check the owner for predicted case
			if( ISVIEWERENTITY( parm ) && ev < PREDICTABLE_EVENTS_MAX && predicted != cg.view.playerPrediction ) {
				return;
			}
			CG_Event_WeaponBeam( ent->origin, ent->origin2, parm );
			break;

		case EV_FIRE_RIOTGUN:
			// check the owner for predicted case
			if( ISVIEWERENTITY( ent->ownerNum ) && ev < PREDICTABLE_EVENTS_MAX && predicted != cg.view.playerPrediction ) {
				return;
			}
			CG_Event_FireRiotgun( ent->origin, ent->origin2, ent->ownerNum );
			break;

		case EV_NOAMMOCLICK:
			if( viewer ) {
				S_StartGlobalSound( cgs.media.sfxWeaponUpNoAmmo, CHAN_AUTO, cg_volume_effects->value );
			} else {
				S_StartFixedSound( cgs.media.sfxWeaponUpNoAmmo, FromQF3( ent->origin ), CHAN_AUTO, cg_volume_effects->value, ATTN_IDLE );
			}
			break;

		case EV_DASH:
			CG_Event_Dash( ent, parm );
			break;

		case EV_WALLJUMP:
			CG_Event_WallJump( ent, parm, ev );
			break;

		case EV_DOUBLEJUMP:
			CG_Event_DoubleJump( ent, parm );
			break;

		case EV_JUMP:
			CG_Event_Jump( ent, parm );
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

		case EV_ITEM_RESPAWN:
			cg_entities[ent->number].respawnTime = cl.serverTime;
			S_StartEntitySound( cgs.media.sfxItemRespawn, ent->number, CHAN_AUTO,
									   cg_volume_effects->value, ATTN_IDLE );
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
			S_StartFixedSound( cgs.media.sfxTeleportIn, FromQF3( ent->origin ), CHAN_AUTO, cg_volume_effects->value, ATTN_NORM );

			if( ent->ownerNum && ent->ownerNum < client_gs.maxclients + 1 ) {
				cg_entities[ent->ownerNum].localEffects[LOCALEFFECT_EV_PLAYER_TELEPORT_IN] = cl.serverTime;
				VectorCopy( ent->origin, cg_entities[ent->ownerNum].teleportedTo );
			}
			break;

		case EV_PLAYER_TELEPORT_OUT:
			S_StartFixedSound( cgs.media.sfxTeleportOut, FromQF3( ent->origin ), CHAN_AUTO, cg_volume_effects->value, ATTN_NORM );

			if( ent->ownerNum && ent->ownerNum < client_gs.maxclients + 1 ) {
				cg_entities[ent->ownerNum].localEffects[LOCALEFFECT_EV_PLAYER_TELEPORT_OUT] = cl.serverTime;
				VectorCopy( ent->origin, cg_entities[ent->ownerNum].teleportedFrom );
			}
			break;

		case EV_PLASMA_EXPLOSION:
			ByteToDir( parm, dir );
			CG_PlasmaExplosion( ent->origin, dir, ent->team, (float)ent->weapon * 8.0f );
			S_StartFixedSound( cgs.media.sfxPlasmaHit, FromQF3( ent->origin ), CHAN_AUTO, cg_volume_effects->value, ATTN_IDLE );
			CG_StartKickAnglesEffect( ent->origin, 50, ent->weapon * 8, 100 );
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

			CG_StartKickAnglesEffect( ent->origin, 135, ent->weapon * 8, 325 );
			break;

		case EV_ROCKET_EXPLOSION:
			ByteToDir( parm, dir );
			CG_RocketExplosionMode( ent->origin, dir, (float)ent->weapon * 8.0f, ent->team );

			CG_StartKickAnglesEffect( ent->origin, 135, ent->weapon * 8, 300 );
			break;

		case EV_GRENADE_BOUNCE:
			S_StartEntitySound( cgs.media.sfxGrenadeBounce, ent->number, CHAN_AUTO, cg_volume_effects->value, ATTN_IDLE );
			break;

		case EV_BLADE_IMPACT:
			CG_BladeImpact( ent->origin, ent->origin2 );
			break;

		case EV_BLOOD:
			if( cg_showBloodTrail->integer == 2 && ISVIEWERENTITY( ent->ownerNum ) ) {
				break;
			}
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
			S_StartFixedSound( cgs.soundPrecache[parm], FromQF3( so ), CHAN_AUTO, cg_volume_effects->value, ATTN_STATIC );
		}
		break;

		case EV_VSAY:
			CG_StartVoiceTokenEffect( ent->ownerNum, parm );
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
			if( early == ISEARLYEVENT( state->events[j] ) ) {
				CG_EntityEvent( state, state->events[j], state->eventParms[j], false );
			}
		}
	}
}

/*
* CG_FirePlayerStateEvents
* This events are only received by this client, and only affect it.
*/
static void CG_FirePlayerStateEvents( void ) {
	unsigned int event, parm, count;
	vec3_t dir;

	if( cg.view.POVent != (int)cg.frame.playerState.POVnum ) {
		return;
	}

	for( count = 0; count < 2; count++ ) {
		// first byte is event number, second is parm
		event = cg.frame.playerState.event[count] & 127;
		parm = cg.frame.playerState.eventParm[count] & 0xFF;

		switch( event ) {
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
				if( cgs.soundPrecache[parm] ) {
					S_StartGlobalSound( cgs.soundPrecache[parm], CHAN_AUTO, cg_volume_effects->value );
				}
				break;

			case PSEV_ANNOUNCER:
				CG_AddAnnouncerEvent( cgs.soundPrecache[parm], false );
				break;

			case PSEV_ANNOUNCER_QUEUED:
				CG_AddAnnouncerEvent( cgs.soundPrecache[parm], true );
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
