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

static void CG_Event_WeaponBeam( Vec3 origin, Vec3 dir, int ownerNum ) {
	float range = GS_GetWeaponDef( Weapon_Railgun )->range;
	Vec3 end = origin + dir * range;

	centity_t * owner = &cg_entities[ ownerNum ];

	// retrace to spawn wall impact
	trace_t trace;
	CG_Trace( &trace, origin, Vec3( 0.0f ), Vec3( 0.0f ), end, cg.view.POVent, MASK_WALLBANG );
	if( trace.ent != -1 ) {
		CG_EBImpact( trace.endpos, trace.plane.normal, trace.surfFlags, CG_TeamColorVec4( owner->current.team ) );
	}

	// when it's predicted we have to delay the drawing until the view weapon is calculated
	owner->localEffects[ LOCALEFFECT_EV_WEAPONBEAM ] = Weapon_Railgun;
	owner->laserOrigin = origin;
	owner->laserPoint = trace.endpos;
}

void CG_WeaponBeamEffect( centity_t * cent ) {
	if( !cent->localEffects[ LOCALEFFECT_EV_WEAPONBEAM ] ) {
		return;
	}

	// now find the projection source for the beam we will draw
	orientation_t projection;
	if( !CG_PModel_GetProjectionSource( cent->current.number, &projection ) ) {
		projection.origin = cent->laserOrigin;
	}

	CG_EBBeam( projection.origin, cent->laserPoint, CG_TeamColorVec4( cent->current.team ) );

	cent->localEffects[ LOCALEFFECT_EV_WEAPONBEAM ] = 0;
}

static void BulletImpact( const trace_t * trace, Vec4 color, int num_particles, float decal_lifetime_scale = 1.0f ) {
	// decal_lifetime_scale is a shitty hack to help reduce decal spam with shotgun
	DoVisualEffect( "vfx/bullet_impact", trace->endpos, trace->plane.normal, num_particles, color, decal_lifetime_scale );
}

static void WallbangImpact( const trace_t * trace, Vec4 color, int num_particles, float decal_lifetime_scale = 1.0f ) {
	// TODO: should draw on entry/exit of all wallbanged surfaces
	if( ( trace->contents & CONTENTS_WALLBANGABLE ) == 0 )
		return;

	DoVisualEffect( "vfx/bullet_impact", trace->endpos, trace->plane.normal, num_particles, color, decal_lifetime_scale );
	DoVisualEffect( "vfx/wallbang_impact", trace->endpos, trace->plane.normal, num_particles, color, decal_lifetime_scale );
}

void CG_LaserBeamEffect( centity_t * cent ) {
	trace_t trace;
	orientation_t projectsource;
	Vec3 laserOrigin, laserAngles, laserPoint;

	if( cent->localEffects[ LOCALEFFECT_LASERBEAM ] <= cl.serverTime ) {
		if( cent->localEffects[ LOCALEFFECT_LASERBEAM ] ) {
			if( ISVIEWERENTITY( cent->current.number ) ) {
				S_StartGlobalSound( "weapons/lg/stop", CHAN_AUTO, 1.0f );
			}
			else {
				S_StartEntitySound( "weapons/lg/stop", cent->current.number, CHAN_AUTO, 1.0f );
			}
		}
		cent->localEffects[ LOCALEFFECT_LASERBEAM ] = 0;
		return;
	}

	Vec4 color = CG_TeamColorVec4( cent->current.team );

	// interpolate the positions
	bool firstPerson = ISVIEWERENTITY( cent->current.number ) && !cg.view.thirdperson;

	if( firstPerson ) {
		laserOrigin = cg.predictedPlayerState.pmove.origin;
		laserOrigin.z += cg.predictedPlayerState.viewheight;
		laserAngles = cg.predictedPlayerState.viewangles;

		laserPoint = Lerp( cent->laserPointOld, cg.lerpfrac, cent->laserPoint );
	}
	else {
		laserOrigin = Lerp( cent->laserOriginOld, cg.lerpfrac, cent->laserOrigin );
		laserPoint = Lerp( cent->laserPointOld, cg.lerpfrac, cent->laserPoint );

		// make up the angles from the start and end points (s->angles is not so precise)
		Vec3 dir = laserPoint - laserOrigin;
		laserAngles = VecToAngles( dir );
	}

	// trace the beam: for tracing we use the real beam origin
	float range = GS_GetWeaponDef( Weapon_Laser )->range;
	GS_TraceLaserBeam( &client_gs, &trace, laserOrigin, laserAngles, range, cent->current.number, 0, []( const trace_t * trace, Vec3 dir, void * data ) {
		centity_t * cent = ( centity_t * ) data;

		Vec4 color = CG_TeamColorVec4( cent->current.team );
		DoVisualEffect( "weapons/lg/tip_hit", trace->endpos, trace->plane.normal, 1.0f, color );

		cent->lg_tip_sound = S_ImmediateFixedSound( "weapons/lg/tip_hit", trace->endpos, 1.0f, cent->lg_tip_sound );
	}, cent );

	// draw the beam: for drawing we use the weapon projection source (already handles the case of viewer entity)
	if( CG_PModel_GetProjectionSource( cent->current.number, &projectsource ) ) {
		laserOrigin = projectsource.origin;
	}

	Vec3 start = laserOrigin;
	Vec3 end = trace.endpos;
	DrawBeam( start, end, 16.0f, color, cgs.media.shaderLGBeam );

	cent->lg_hum_sound = S_ImmediateEntitySound( "weapons/lg/hum", cent->current.number, 1.0f, cent->lg_hum_sound );

	if( ISVIEWERENTITY( cent->current.number ) ) {
		cent->lg_beam_sound = S_ImmediateEntitySound( "weapons/lg/beam", cent->current.number, 1.0f, cent->lg_beam_sound );
	}
	else {
		cent->lg_beam_sound = S_ImmediateLineSound( "weapons/lg/beam", start, end, 1.0f, cent->lg_beam_sound );
	}

	if( trace.fraction == 1.0f ) {
		DoVisualEffect( "weapons/lg/tip_miss", end, Vec3( 0.0f, 0.0f, 1.0f ), 1, color );
		cent->lg_tip_sound = S_ImmediateFixedSound( "weapons/lg/tip_miss", end, 1.0f, cent->lg_tip_sound );
	}
}

static void CG_Event_LaserBeam( Vec3 origin, Vec3 dir, int entNum ) {
	// lasergun's smooth refire
	// it appears that 64ms is that maximum allowed time interval between prediction events on localhost
	unsigned int range = Max2( GS_GetWeaponDef( Weapon_Laser )->refire_time + 10, 65u );

	centity_t * cent = &cg_entities[ entNum ];
	cent->laserOrigin = origin;
	cent->laserPoint = cent->laserOrigin + dir * GS_GetWeaponDef( Weapon_Laser )->range;

	cent->laserOriginOld = cent->laserOrigin;
	cent->laserPointOld = cent->laserPoint;
	cent->localEffects[ LOCALEFFECT_LASERBEAM ] = cl.serverTime + range;
}

static void CG_FireWeaponEvent( int entNum, WeaponType weapon ) {
	const WeaponModelMetadata * weaponInfo = GetWeaponModelMetadata( weapon );
	StringHash sfx = weaponInfo->fire_sound;

	if( ISVIEWERENTITY( entNum ) ) {
		S_StartGlobalSound( sfx, CHAN_AUTO, 1.0f );
	}
	else {
		S_StartEntitySound( sfx, entNum, CHAN_AUTO, 1.0f );
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
		case Weapon_BubbleGun:
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
		CG_AddRecoil( weapon );
	}
}

static void CG_Event_FireBullet( Vec3 origin, Vec3 dir, WeaponType weapon, int owner, Vec4 team_color ) {
	int range = GS_GetWeaponDef( weapon )->range;

	Vec3 right, up;
	ViewVectors( dir, &right, &up );

	trace_t trace, wallbang;
	GS_TraceBullet( &client_gs, &trace, &wallbang, origin, dir, right, up, 0, 0, range, owner, 0 );

	if( trace.ent != -1 && !( trace.surfFlags & SURF_NOIMPACT ) ) {
		if( trace.surfFlags & SURF_FLESH || ( trace.ent > 0 && cg_entities[ trace.ent ].current.type == ET_PLAYER ) ) {
			// flesh impact sound
		}
		else {
			BulletImpact( &trace, team_color, 24 );
			S_StartFixedSound( "weapons/bullet_impact", trace.endpos, CHAN_AUTO, 1.0f );

			if( !ISVIEWERENTITY( owner ) ) {
				S_StartLineSound( "weapons/bullet_whizz", origin, trace.endpos, CHAN_AUTO, 1.0f );
			}
		}

		WallbangImpact( &wallbang, team_color, 12 );
	}

	orientation_t projection;
	if( !CG_PModel_GetProjectionSource( owner, &projection ) ) {
		projection.origin = origin;
	}

	if( weapon != Weapon_Pistol ) {
		AddPersistentBeam( projection.origin, trace.endpos, 1.0f, team_color, cgs.media.shaderTracer, 0.2f, 0.1f );
	}
}

static void CG_Event_FireShotgun( Vec3 origin, Vec3 dir, int owner, Vec4 team_color ) {
	const WeaponDef * def = GS_GetWeaponDef( Weapon_Shotgun );

	Vec3 right, up;
	ViewVectors( dir, &right, &up );

	orientation_t projection;
	if( !CG_PModel_GetProjectionSource( owner, &projection ) ) {
		projection.origin = origin;
	}

	// Sunflower pattern
	for( int i = 0; i < def->projectile_count; i++ ) {
		float fi = i * 2.4f; // magic value creating Fibonacci numbers
		float r = cosf( fi ) * def->spread * sqrtf( fi );
		float u = sinf( fi ) * def->spread * sqrtf( fi );

		trace_t trace, wallbang;
		GS_TraceBullet( &client_gs, &trace, &wallbang, origin, dir, right, up, r, u, def->range, owner, 0 );

		// don't create so many decals if they would all end up overlapping anyway
		float distance = Length( trace.endpos - origin );
		float decal_p = Lerp( 0.25f, Unlerp( 0.0f, distance, 256.0f ), 0.5f );
		if( random_p( &cls.rng, decal_p ) ) {
			if( trace.ent != -1 && !( trace.surfFlags & SURF_NOIMPACT ) ) {
				BulletImpact( &trace, team_color, 4, 0.5f );
			}

			WallbangImpact( &wallbang, team_color, 2, 0.5f );
		}

		AddPersistentBeam( projection.origin, trace.endpos, 1.0f, team_color, cgs.media.shaderTracer, 0.2f, 0.1f );
	}

	// spawn a single sound at the impact
	Vec3 end = origin + dir * def->range;

	trace_t trace;
	CG_Trace( &trace, origin, Vec3( 0.0f ), Vec3( 0.0f ), end, owner, MASK_SHOT );

	if( trace.ent != -1 && !( trace.surfFlags & SURF_NOIMPACT ) ) {
		S_StartFixedSound( "weapons/rg/hit", trace.endpos, CHAN_AUTO, 1.0f );
	}
}

//=========================================================
#define CG_MAX_ANNOUNCER_EVENTS 32
#define CG_MAX_ANNOUNCER_EVENTS_MASK ( CG_MAX_ANNOUNCER_EVENTS - 1 )
#define CG_ANNOUNCER_EVENTS_FRAMETIME 1500 // the announcer will speak each 1.5 seconds
struct cg_announcerevent_t {
	StringHash sound;
};
cg_announcerevent_t cg_announcerEvents[ CG_MAX_ANNOUNCER_EVENTS ];
static int cg_announcerEventsCurrent = 0;
static int cg_announcerEventsHead = 0;
static int cg_announcerEventsDelay = 0;

static Vec3 speaker_origins[ 1024 ];
static size_t num_speakers;

void ResetAnnouncerSpeakers() {
	num_speakers = 0;
}

void AddAnnouncerSpeaker( const centity_t * cent ) {
	if( num_speakers == ARRAY_COUNT( speaker_origins ) )
		return;

	speaker_origins[ num_speakers ] = cent->current.origin;
	num_speakers++;
}

static void PlayAnnouncerSound( StringHash sound ) {
	if( num_speakers == 0 ) {
		S_StartLocalSound( sound, CHAN_AUTO, 1.0f );
	}
	else {
		for( size_t i = 0; i < num_speakers; i++ ) {
			S_StartFixedSound( sound, speaker_origins[ i ], CHAN_AUTO, 1.0f );
		}
	}
}

void CG_ClearAnnouncerEvents() {
	cg_announcerEventsCurrent = cg_announcerEventsHead = 0;
}

void CG_AddAnnouncerEvent( StringHash sound, bool queued ) {
	if( !queued ) {
		PlayAnnouncerSound( sound );
		cg_announcerEventsDelay = CG_ANNOUNCER_EVENTS_FRAMETIME; // wait
		return;
	}

	if( cg_announcerEventsCurrent + CG_MAX_ANNOUNCER_EVENTS >= cg_announcerEventsHead ) {
		// full buffer (we do nothing, just let it overwrite the oldest
	}

	// add it
	cg_announcerEvents[ cg_announcerEventsHead & CG_MAX_ANNOUNCER_EVENTS_MASK ].sound = sound;
	cg_announcerEventsHead++;
}

/*
 * CG_ReleaseAnnouncerEvents
 */
void CG_ReleaseAnnouncerEvents() {
	// see if enough time has passed
	cg_announcerEventsDelay -= cls.realFrameTime;
	if( cg_announcerEventsDelay > 0 ) {
		return;
	}

	if( cg_announcerEventsCurrent < cg_announcerEventsHead ) {
		StringHash sound = cg_announcerEvents[ cg_announcerEventsCurrent & CG_MAX_ANNOUNCER_EVENTS_MASK ].sound;
		PlayAnnouncerSound( sound );
		cg_announcerEventsDelay = CG_ANNOUNCER_EVENTS_FRAMETIME; // wait
		cg_announcerEventsCurrent++;
	}
	else {
		cg_announcerEventsDelay = 0; // no wait
	}
}

/*
 * CG_StartVoiceTokenEffect
 */
static void CG_StartVoiceTokenEffect( int entNum, u64 parm ) {
	if( !cg_voiceChats->integer ) {
		return;
	}

	u32 vsay = parm & 0xffff;
	u32 entropy = parm >> 16;

	if( vsay >= Vsay_Total ) {
		return;
	}

	centity_t * cent = &cg_entities[ entNum ];

	// ignore repeated/flooded events
	// TODO: this should really look at how long the vsay is...
	if( cent->localEffects[ LOCALEFFECT_VSAY_TIMEOUT ] > cl.serverTime ) {
		return;
	}

	cent->localEffects[ LOCALEFFECT_VSAY_TIMEOUT ] = cl.serverTime + VSAY_TIMEOUT;

	StringHash sound = cgs.media.sfxVSaySounds[ vsay ];

	if( GS_MatchState( &client_gs ) >= MATCH_STATE_POSTMATCH ) {
		S_StartGlobalSound( sound, CHAN_AUTO, 1.0f, entropy );
	}
	else {
		S_StartEntitySound( sound, entNum, CHAN_AUTO, 1.0f, entropy );
	}
}

//==================================================================

/*
 * CG_Event_Fall
 */
void CG_Event_Fall( const SyncEntityState * state, u64 parm ) {
	if( ISVIEWERENTITY( state->number ) ) {
		CG_StartFallKickEffect( ( parm + 5 ) * 10 );
	}

	Vec3 ground_position = state->origin;
	ground_position.z += state->bounds.mins.z;

	if( parm < 40 )
		return;

	float volume = ( parm - 40 ) / 300.0f;
	if( ISVIEWERENTITY( state->number ) ) {
		S_StartLocalSound( "players/fall", CHAN_AUTO, volume );
	}
	else {
		S_StartEntitySound( "players/fall", state->number, CHAN_AUTO, volume );
	}
}

/*
 * CG_Event_Pain
 */
static void CG_Event_Pain( SyncEntityState * state, u64 parm ) {
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
	constexpr struct {
		int dead, dying;
	} animations[] = {
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
void CG_Event_Dash( SyncEntityState * state, u64 parm ) {
	constexpr int animations[] = { LEGS_DASH, LEGS_DASH_LEFT, LEGS_DASH_RIGHT, LEGS_DASH_BACK };
	if( parm >= ARRAY_COUNT( animations ) )
		return;

	CG_PModel_AddAnimation( state->number, animations[ parm ], 0, 0, EVENT_CHANNEL );
	CG_PlayerSound( state->number, CHAN_BODY, PlayerSound_Dash );
	CG_Dash( state ); // Dash smoke effect

	// since most dash animations jump with right leg, reset the jump to start with left leg after a dash
	cg_entities[ state->number ].jumpedLeft = true;
}

/*
 * CG_Event_WallJump
 */
void CG_Event_WallJump( SyncEntityState * state, u64 parm, int ev ) {
	Vec3 normal = U64ToDir( parm );

	Vec3 forward, right;
	AngleVectors( Vec3( state->angles.x, state->angles.y, 0 ), &forward, &right, NULL );

	if( Dot( normal, right ) > 0.3f ) {
		CG_PModel_AddAnimation( state->number, LEGS_WALLJUMP_RIGHT, 0, 0, EVENT_CHANNEL );
	}
	else if( -Dot( normal, right ) > 0.3f ) {
		CG_PModel_AddAnimation( state->number, LEGS_WALLJUMP_LEFT, 0, 0, EVENT_CHANNEL );
	}
	else if( -Dot( normal, forward ) > 0.3f ) {
		CG_PModel_AddAnimation( state->number, LEGS_WALLJUMP_BACK, 0, 0, EVENT_CHANNEL );
	}
	else {
		CG_PModel_AddAnimation( state->number, LEGS_WALLJUMP, 0, 0, EVENT_CHANNEL );
	}

	CG_PlayerSound( state->number, CHAN_BODY, PlayerSound_WallJump );

	Vec3 pos = state->origin;
	pos.z += 15;
	CG_DustCircle( pos, normal, 65, 12 );
}

static void CG_PlayJumpSound( const SyncEntityState * state ) {
	CG_PlayerSound( state->number, CHAN_BODY, PlayerSound_Jump );
}

/*
 * CG_Event_Jump
 */
static void CG_Event_Jump( SyncEntityState * state ) {
	CG_PlayJumpSound( state );

	centity_t * cent = &cg_entities[ state->number ];
	float xyspeedcheck = Length( Vec3( cent->animVelocity.x, cent->animVelocity.y, 0 ) );
	if( xyspeedcheck < 100 ) { // the player is jumping on the same place, not running
		CG_PModel_AddAnimation( state->number, LEGS_JUMP_NEUTRAL, 0, 0, EVENT_CHANNEL );
	}
	else {
		Vec3 movedir;
		mat3_t viewaxis;

		movedir = cent->animVelocity;
		movedir.z = 0.0f;
		movedir = Normalize( movedir );

		Matrix3_FromAngles( Vec3( 0, cent->current.angles.y, 0 ), viewaxis );

		// see what's his relative movement direction
		constexpr float MOVEDIREPSILON = 0.25f;
		if( Dot( movedir, FromQFAxis( viewaxis, AXIS_FORWARD ) ) > MOVEDIREPSILON ) {
			cent->jumpedLeft = !cent->jumpedLeft;
			if( !cent->jumpedLeft ) {
				CG_PModel_AddAnimation( state->number, LEGS_JUMP_LEG2, 0, 0, EVENT_CHANNEL );
			}
			else {
				CG_PModel_AddAnimation( state->number, LEGS_JUMP_LEG1, 0, 0, EVENT_CHANNEL );
			}
		}
		else {
			CG_PModel_AddAnimation( state->number, LEGS_JUMP_NEUTRAL, 0, 0, EVENT_CHANNEL );
		}
	}
}

/*
 * CG_EntityEvent
 */
void CG_EntityEvent( SyncEntityState * ent, int ev, u64 parm, bool predicted ) {
	bool viewer = ISVIEWERENTITY( ent->number );

	if( viewer && ev < PREDICTABLE_EVENTS_MAX && predicted != cg.view.playerPrediction ) {
		return;
	}

	Vec4 team_color = CG_TeamColorVec4( ent->team );

	switch( ev ) {
		default:
			break;

			//  PREDICTABLE EVENTS

		case EV_WEAPONACTIVATE: {
			WeaponType weapon = parm >> 1;
			bool silent = ( parm & 1 ) != 0;
			if( predicted ) {
				cg_entities[ ent->number ].current.weapon = weapon;
			}

			if( !silent ) {
				CG_PModel_AddAnimation( ent->number, 0, TORSO_WEAPON_SWITCHIN, 0, EVENT_CHANNEL );

				StringHash sfx = GetWeaponModelMetadata( weapon )->up_sound;
				if( viewer ) {
					S_StartGlobalSound( sfx, CHAN_AUTO, 1.0f );
				}
				else {
					S_StartFixedSound( sfx, ent->origin, CHAN_AUTO, 1.0f );
				}
			}
		} break;

		case EV_SMOOTHREFIREWEAPON: // the server never sends this event
			if( predicted ) {
				cg_entities[ ent->number ].current.weapon = parm;

				if( parm == Weapon_Laser ) {
					Vec3 origin = cg.predictedPlayerState.pmove.origin;
					origin.z += cg.predictedPlayerState.viewheight;

					Vec3 dir;
					AngleVectors( cg.predictedPlayerState.viewangles, &dir, NULL, NULL );

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
				cg_entities[ ent->number ].current.weapon = parm;
			}

			int num;
			Vec3 origin, angles;
			if( predicted ) {
				num = ent->number;
				origin = cg.predictedPlayerState.pmove.origin;
				origin.z += cg.predictedPlayerState.viewheight;
				angles = cg.predictedPlayerState.viewangles;
			}
			else {
				num = ent->ownerNum;
				origin = ent->origin;
				angles = ent->origin2;
			}

			CG_FireWeaponEvent( num, parm );

			Vec3 dir;
			AngleVectors( angles, &dir, NULL, NULL );

			if( parm == Weapon_Railgun ) {
				CG_Event_WeaponBeam( origin, dir, num );
			}
			else if( parm == Weapon_Shotgun ) {
				CG_Event_FireShotgun( origin, dir, num, team_color );
			}
			else if( parm == Weapon_Laser ) {
				CG_Event_LaserBeam( origin, dir, num );
			}
			else if( parm == Weapon_Pistol || parm == Weapon_MachineGun || parm == Weapon_Deagle || parm == Weapon_AssaultRifle ) {
				CG_Event_FireBullet( origin, dir, parm, num, team_color );
			}
		} break;

		case EV_NOAMMOCLICK:
			if( viewer ) {
				S_StartGlobalSound( "weapons/noammo", CHAN_AUTO, 1.0f );
			}
			else {
				S_StartFixedSound( "weapons/noammo", ent->origin, CHAN_AUTO, 1.0f );
			}
			break;

		case EV_ZOOM_IN:
		case EV_ZOOM_OUT: {
			if( parm <= Weapon_None || parm >= Weapon_Count )
				return;

			const WeaponModelMetadata * weapon = GetWeaponModelMetadata( parm );
			StringHash sfx = ev == EV_ZOOM_IN ? weapon->zoom_in_sound : weapon->zoom_out_sound;

			if( viewer ) {
				S_StartGlobalSound( sfx, CHAN_AUTO, 1.0f );
			}
			else {
				S_StartFixedSound( sfx, ent->origin, CHAN_AUTO, 1.0f );
			}
		} break;

		case EV_DASH:
			CG_Event_Dash( ent, parm );
			break;

		case EV_WALLJUMP:
			CG_Event_WallJump( ent, parm, ev );
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
			CG_GenericExplosion( ent->origin, Vec3( 0.0f ), parm * 8 );
			break;

		case EV_EXPLOSION2:
			CG_GenericExplosion( ent->origin, Vec3( 0.0f ), parm * 16 );
			break;

		case EV_SPARKS: {
			// Vec3 dir = U64ToDir( parm );
			// if( ent->damage > 0 ) {
			// 	count = Clamp( 1, int( ent->damage * 0.25f ), 10 );
			// } else {
			// 	count = 6;
			// }

			// CG_ParticleEffect( ent->origin, dir, 1.0f, 0.67f, 0.0f, count );
		} break;

		case EV_LASER_SPARKS: {
			// Vec3 dir = U64ToDir( parm );
			// CG_ParticleEffect2( ent->origin, dir,
			// 					COLOR_R( ent->colorRGBA ) * ( 1.0 / 255.0 ),
			// 					COLOR_G( ent->colorRGBA ) * ( 1.0 / 255.0 ),
			// 					COLOR_B( ent->colorRGBA ) * ( 1.0 / 255.0 ),
			// 					ent->counterNum );
		} break;

		case EV_GIB:
			SpawnGibs( ent->origin, ent->origin2, parm, team_color );
			break;

		case EV_PLAYER_RESPAWN:
			if( ( unsigned ) ent->ownerNum == cgs.playerNum + 1 ) {
				CG_ResetKickAngles();
				cg.recoiling = false;
				cg.damage_effect = 0.0f;
			}
			break;

		case EV_PLAYER_TELEPORT_IN:
			S_StartFixedSound( "sounds/world/tele_in", ent->origin, CHAN_AUTO, 1.0f );
			break;

		case EV_PLAYER_TELEPORT_OUT:
			S_StartFixedSound( "sounds/world/tele_in", ent->origin, CHAN_AUTO, 1.0f );
			break;

		case EV_PLASMA_EXPLOSION: {
			Vec3 dir = U64ToDir( parm );
			CG_PlasmaExplosion( ent->origin, dir, team_color );
		} break;

		case EV_BUBBLE_EXPLOSION:
			CG_BubbleExplosion( ent->origin, team_color );
			break;

		case EV_BOLT_EXPLOSION: {
			Vec3 dir = U64ToDir( parm );
			CG_EBImpact( ent->origin, dir, 0, team_color );
		} break;

		case EV_GRENADE_EXPLOSION: {
			Vec3 dir;
			if( parm ) {
				// we have a direction
				dir = U64ToDir( parm );
				CG_GrenadeExplosion( ent->origin, dir, team_color );
			}
			else {
				// no direction
				CG_GrenadeExplosion( ent->origin, Vec3( 0.0f ), team_color );
			}

		} break;

		case EV_ROCKET_EXPLOSION: {
			Vec3 dir = U64ToDir( parm );
			CG_RocketExplosion( ent->origin, dir, team_color );

		} break;

		case EV_GRENADE_BOUNCE:
			S_StartEntitySound( "weapons/gl/bounce", ent->number, CHAN_AUTO, 1.0f );
			break;

		case EV_BLADE_IMPACT:
			CG_BladeImpact( ent->origin, ent->origin2 );
			break;

		case EV_RIFLEBULLET_IMPACT: {
			Vec3 dir = U64ToDir( parm );
			DoVisualEffect( "vfx/bulletsparks", ent->origin, dir, 24, team_color );
			S_StartFixedSound( "weapons/bullet_impact", ent->origin, CHAN_AUTO, 1.0f );
		} break;

		case EV_STAKE_IMPACT: {
			Vec3 dir = U64ToDir( parm );
			CG_StakeImpact( ent->origin, dir, team_color );
		} break;

		case EV_STAKE_IMPALE: {
			Vec3 dir = U64ToDir( parm );
			CG_StakeImpale( ent->origin, dir, team_color );
		} break;

		case EV_BLAST_IMPACT: {
			Vec3 dir = U64ToDir( parm );
			CG_BlastImpact( ent->origin, dir, team_color );
		} break;

		case EV_BLAST_BOUNCE: {
			Vec3 dir = U64ToDir( parm );
			CG_BlastBounce( ent->origin, dir, team_color );
		} break;

		case EV_BLOOD: {
			Vec3 dir = U64ToDir( parm );
			Vec3 tangent, bitangent;
			OrthonormalBasis( dir, &tangent, &bitangent );

			int damage = ent->radius;
			float p = damage / 20.0f;

			if( !ISVIEWERENTITY( ent->ownerNum ) ) {
				DoVisualEffect( "vfx/blood_spray", ent->origin, dir, damage, team_color );
			}

			while( true ) {
				if( !random_p( &cls.rng, p ) )
					break;

				Vec3 random_dir = Normalize( dir + tangent * random_float11( &cls.rng ) * 0.1f + bitangent * random_float11( &cls.rng ) * 0.1f );
				Vec3 end = ent->origin + random_dir * 256.0f;

				trace_t trace;
				CG_Trace( &trace, ent->origin, Vec3( -4.0f ), Vec3( 4.0f ), end, 0, MASK_SOLID );

				if( trace.fraction < 1.0f ) {
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

					float angle = random_uniform_float( &cls.rng, 0.0f, Radians( 360.0f ) );
					float min_size = Lerp( 20.0f, Unlerp01( 5, damage, 50 ), 64.0f );
					float size = min_size * random_uniform_float( &cls.rng, 0.75f, 1.5f );

					AddPersistentDecal( trace.endpos, trace.plane.normal, size, angle, random_select( &cls.rng, decals ), team_color, 30000 );
				}

				p -= 1.0f;
			}
		} break;

		// func movers
		case EV_PLAT_HIT_TOP:
		case EV_PLAT_HIT_BOTTOM:
		case EV_PLAT_START_MOVING:
		case EV_DOOR_HIT_TOP:
		case EV_DOOR_HIT_BOTTOM:
		case EV_DOOR_START_MOVING:
		case EV_BUTTON_FIRE:
		case EV_TRAIN_STOP:
		case EV_TRAIN_START: {
			Vec3 so;
			CG_GetEntitySpatialization( ent->number, &so, NULL );
			S_StartFixedSound( StringHash( parm ), so, CHAN_AUTO, 1.0f );
		} break;

		case EV_VSAY:
			CG_StartVoiceTokenEffect( ent->ownerNum, parm );
			break;

		case EV_TBAG:
			S_StartFixedSound( "sounds/tbag/tbag", ent->origin, CHAN_AUTO, parm / 255.0f );
			break;

		case EV_SPRAY:
			AddSpray( ent->origin, ent->origin2, ent->angles, parm );
			S_StartFixedSound( "sounds/spray/spray", ent->origin, CHAN_AUTO, 1.0f );
			break;

		case EV_DAMAGE:
			CG_AddDamageNumber( ent, parm );
			break;

		case EV_HEADSHOT:
			S_StartFixedSound( "sounds/headshot/headshot", ent->origin, CHAN_AUTO, 1.0f );
			break;

		case EV_VFX:
			DoVisualEffect( StringHash( parm ), ent->origin, Vec3( 0.0f, 0.0f, 1.0f ), 1, vec4_white );
			break;
	}
}

#define ISEARLYEVENT( ev ) ( ev == EV_WEAPONDROP )

/*
 * CG_FireEvents
 */
static void CG_FireEntityEvents( bool early ) {
	for( int pnum = 0; pnum < cg.frame.numEntities; pnum++ ) {
		SyncEntityState * state = &cg.frame.parsedEntities[ pnum & ( MAX_PARSE_ENTITIES - 1 ) ];

		if( cgs.demoPlaying ) {
			if( ( state->svflags & SVF_ONLYTEAM ) && cg.predictedPlayerState.team != state->team )
				continue;
			if( ( ( state->svflags & SVF_ONLYOWNER ) || ( state->svflags & SVF_OWNERANDCHASERS ) ) && cg.predictedPlayerState.POVnum != state->ownerNum )
				continue;
		}

		if( state->type == ET_SOUNDEVENT ) {
			if( early ) {
				CG_SoundEntityNewState( &cg_entities[ state->number ] );
			}
			continue;
		}

		for( int j = 0; j < 2; j++ ) {
			if( early == ISEARLYEVENT( state->events[ j ].type ) ) {
				CG_EntityEvent( state, state->events[ j ].type, state->events[ j ].parm, false );
			}
		}
	}
}

/*
 * CG_FirePlayerStateEvents
 * This events are only received by this client, and only affect it.
 */
static void CG_FirePlayerStateEvents() {
	if( cg.view.POVent != ( int ) cg.frame.playerState.POVnum ) {
		return;
	}

	for( int count = 0; count < 2; count++ ) {
		u64 parm = cg.frame.playerState.events[ count ].parm;

		switch( cg.frame.playerState.events[ count ].type ) {
			case PSEV_HIT:
				if( parm < 4 ) { // hit of some caliber
					S_StartLocalSound( cgs.media.sfxWeaponHit[ parm ], CHAN_AUTO, 1.0f );
					CG_ScreenCrosshairDamageUpdate();
				}
				else { // killed an enemy
					S_StartLocalSound( "sounds/misc/kill", CHAN_AUTO, 1.0f );
					CG_ScreenCrosshairDamageUpdate();
				}
				break;

			case PSEV_DAMAGE_10:
				AddDamageEffect( 0.1f );
				break;

			case PSEV_DAMAGE_20:
				AddDamageEffect( 0.2f );
				break;

			case PSEV_DAMAGE_30:
				AddDamageEffect( 0.3f );
				break;

			case PSEV_DAMAGE_40:
				AddDamageEffect( 0.4f );
				break;

			case PSEV_ANNOUNCER:
				CG_AddAnnouncerEvent( StringHash( parm ), false );
				break;

			case PSEV_ANNOUNCER_QUEUED:
				CG_AddAnnouncerEvent( StringHash( parm ), true );
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
