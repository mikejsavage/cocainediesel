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

void RailgunImpact( Vec3 pos, Vec3 dir, int surfFlags, Vec4 color ) {
	DoVisualEffect( "weapons/eb/hit", pos, dir, 1.0f, color );
	PlaySFX( "weapons/eb/hit", PlaySFXConfigPosition( pos ) );
}

static void BulletImpact( const trace_t * trace, Vec4 color, int num_particles, float decal_lifetime_scale = 1.0f ) {
	// decal_lifetime_scale is a shitty hack to help reduce decal spam with shotgun
	DoVisualEffect( "vfx/bullet_impact", trace->endpos, trace->plane.normal, num_particles, color, decal_lifetime_scale );
}

static void WallbangImpact( const trace_t * trace, Vec4 color, int num_particles, float decal_lifetime_scale = 1.0f ) {
	// TODO: should draw on entry/exit of all wallbanged surfaces
	if( ( trace->contents & CONTENTS_WALLBANGABLE ) == 0 )
		return;

	DoVisualEffect( "vfx/wallbang_impact", trace->endpos, trace->plane.normal, num_particles, color, decal_lifetime_scale );
}

static Mat4 GetMuzzleTransform( int ent ) {
	if( ent < 1 || ent > client_gs.maxclients )
		return Mat4::Identity();

	if( ISVIEWERENTITY( ent ) && !cg.view.thirdperson ) {
		return cg.weapon.muzzle_transform;
	}

	return cg_entPModels[ ent ].muzzle_transform;
}

static void FireRailgun( Vec3 origin, Vec3 dir, int ownerNum ) {
	const WeaponDef * def = GS_GetWeaponDef( Weapon_Railgun );

	float range = def->range;
	Vec3 end = origin + dir * range;

	centity_t * owner = &cg_entities[ ownerNum ];

	Vec4 color = CG_TeamColorVec4( owner->current.team );

	trace_t trace;
	CG_Trace( &trace, origin, Vec3( 0.0f ), Vec3( 0.0f ), end, cg.view.POVent, MASK_WALLBANG );
	if( trace.ent != -1 ) {
		RailgunImpact( trace.endpos, trace.plane.normal, trace.surfFlags, color );
	}

	Mat4 muzzle_transform = GetMuzzleTransform( ownerNum );

	AddPersistentBeam( muzzle_transform.col3.xyz(), trace.endpos, 16.0f, color, "weapons/eb/beam", 0.25f, 0.1f );
	RailTrailParticles( muzzle_transform.col3.xyz(), trace.endpos, color );
}

void CG_LaserBeamEffect( centity_t * cent ) {
	trace_t trace;
	Vec3 laserOrigin, laserAngles, laserPoint;

	if( cent->localEffects[ LOCALEFFECT_LASERBEAM ] <= cl.serverTime ) {
		if( cent->localEffects[ LOCALEFFECT_LASERBEAM ] ) {
			if( ISVIEWERENTITY( cent->current.number ) ) {
				PlaySFX( "weapons/lg/stop" );
			}
			else {
				PlaySFX( "weapons/lg/stop", PlaySFXConfigEntity( cent->current.number ) );
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
		DrawDynamicLight( trace->endpos, color, 10000.0f );
		DoVisualEffect( "weapons/lg/tip_hit", trace->endpos, trace->plane.normal, 1.0f, color );

		cent->lg_tip_sound = PlayImmediateSFX( "weapons/lg/tip_hit", cent->lg_tip_sound, PlaySFXConfigPosition( trace->endpos ) );
	}, cent );

	Mat4 muzzle_transform = GetMuzzleTransform( cent->current.number );

	Vec3 start = muzzle_transform.col3.xyz();
	Vec3 end = trace.endpos;
	DrawBeam( start, end, 8.0f, color, "weapons/lg/beam" );

	cent->lg_hum_sound = PlayImmediateSFX( "weapons/lg/hum", cent->lg_hum_sound, PlaySFXConfigEntity( cent->current.number ) );

	if( ISVIEWERENTITY( cent->current.number ) ) {
		cent->lg_beam_sound = PlayImmediateSFX( "weapons/lg/beam", cent->lg_beam_sound, PlaySFXConfigEntity( cent->current.number ) );
	}
	else {
		cent->lg_beam_sound = PlayImmediateSFX( "weapons/lg/beam", cent->lg_beam_sound, PlaySFXConfigLineSegment( start, end ) );
	}

	if( trace.fraction == 1.0f ) {
		DoVisualEffect( "weapons/lg/tip_miss", end, Vec3( 0.0f, 0.0f, 1.0f ), 1, color );
		cent->lg_tip_sound = PlayImmediateSFX( "weapons/lg/tip_miss", cent->lg_tip_sound, PlaySFXConfigPosition( end ) );
	}
}

static void CG_Event_LaserBeam( Vec3 origin, Vec3 dir, int entNum ) {
	// lasergun's smooth refire
	// it appears that 64ms is that maximum allowed time interval between prediction events on localhost
	unsigned int range = Max2( GS_GetWeaponDef( Weapon_Laser )->refire_time + 10, 65 );

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
		PlaySFX( sfx );
	}
	else {
		PlaySFX( sfx, PlaySFXConfigEntity( entNum ) );
	}

	if( weapon != Weapon_Laser && ISVIEWERENTITY( entNum ) ) {
		CG_ScreenCrosshairShootUpdate( GS_GetWeaponDef( weapon )->refire_time );
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
		case Weapon_AssaultRifle:
		case Weapon_BubbleGun:
		case Weapon_StakeGun:
		case Weapon_MasterBlaster:
		case Weapon_RoadGun:
			CG_PModel_AddAnimation( entNum, 0, TORSO_SHOOT_LIGHTWEAPON, 0, EVENT_CHANNEL );
			break;

		case Weapon_BurstRifle:
		case Weapon_RocketLauncher:
		case Weapon_GrenadeLauncher:
		// case Weapon_Minigun:
			CG_PModel_AddAnimation( entNum, 0, TORSO_SHOOT_HEAVYWEAPON, 0, EVENT_CHANNEL );
			break;

		case Weapon_Railgun:
		case Weapon_Sniper:
		case Weapon_Rifle:
			CG_PModel_AddAnimation( entNum, 0, TORSO_SHOOT_AIMWEAPON, 0, EVENT_CHANNEL );
			break;
	}

	// recoil
	if( ISVIEWERENTITY( entNum ) && cg.view.playerPrediction ) {
		CG_AddRecoil( weapon );
	}
}

static void CG_Event_FireBullet( Vec3 origin, Vec3 dir, u16 entropy, s16 zoom_time, WeaponType weapon, int owner, Vec4 team_color ) {
	const WeaponDef * def = GS_GetWeaponDef( weapon );

	Vec3 right, up;
	ViewVectors( dir, &right, &up );

	Vec2 spread = RandomSpreadPattern( entropy, def->spread + ZoomSpreadness( zoom_time, def ) );
	int range = def->range;

	trace_t trace, wallbang;
	GS_TraceBullet( &client_gs, &trace, &wallbang, origin, dir, right, up, spread, range, owner, 0 );

	if( trace.ent != -1 ) {
		if( trace.ent > 0 && cg_entities[ trace.ent ].current.type == ET_PLAYER ) {
			// flesh impact sound
		}
		else {
			BulletImpact( &trace, team_color, 24 );
			PlaySFX( "weapons/bullet_impact", PlaySFXConfigPosition( trace.endpos ) );

			if( !ISVIEWERENTITY( owner ) ) {
				PlaySFX( "weapons/bullet_whizz", PlaySFXConfigLineSegment( origin, trace.endpos ) );
			}
		}

		WallbangImpact( &wallbang, team_color, 12 );
	}

	Mat4 muzzle_transform = GetMuzzleTransform( owner );

	AddPersistentBeam( muzzle_transform.col3.xyz(), trace.endpos, 1.0f, team_color, "weapons/tracer", 0.2f, 0.1f );
}

static void CG_Event_FireShotgun( Vec3 origin, Vec3 dir, int owner, Vec4 team_color ) {
	const WeaponDef * def = GS_GetWeaponDef( Weapon_Shotgun );

	Vec3 right, up;
	ViewVectors( dir, &right, &up );

	Mat4 muzzle_transform = GetMuzzleTransform( owner );
	Vec3 muzzle = muzzle_transform.col3.xyz();

	for( int i = 0; i < def->projectile_count; i++ ) {
		Vec2 spread = FixedSpreadPattern( i, def->spread );

		trace_t trace, wallbang;
		GS_TraceBullet( &client_gs, &trace, &wallbang, origin, dir, right, up, spread, def->range, owner, 0 );

		// don't create so many decals if they would all end up overlapping anyway
		float distance = Length( trace.endpos - origin );
		float decal_p = Lerp( 0.25f, Unlerp( 0.0f, distance, 256.0f ), 0.5f );
		if( Probability( &cls.rng, decal_p ) ) {
			if( trace.ent != -1 ) {
				BulletImpact( &trace, team_color, 4, 0.5f );
			}

			WallbangImpact( &wallbang, team_color, 2, 0.5f );
		}

		AddPersistentBeam( muzzle, trace.endpos, 1.0f, team_color, "weapons/tracer", 0.2f, 0.1f );
	}

	// spawn a single sound at the impact
	Vec3 end = origin + dir * def->range;

	trace_t trace;
	CG_Trace( &trace, origin, Vec3( 0.0f ), Vec3( 0.0f ), end, owner, MASK_SHOT );

	if( trace.ent != -1 ) {
		PlaySFX( "weapons/rg/hit", PlaySFXConfigPosition( trace.endpos ) );
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
		PlaySFX( sound );
	}
	else {
		for( size_t i = 0; i < num_speakers; i++ ) {
			PlaySFX( sound, PlaySFXConfigPosition( speaker_origins[ i ] ) );
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

static void CG_StartVsay( int entNum, u64 parm ) {
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

	PlaySFXConfig config = PlaySFXConfigGlobal();
	config.has_entropy = true;
	config.entropy = entropy;
	if( client_gs.gameState.match_state >= MatchState_PostMatch ) {
		PlaySFX( sound, config );
	}
	else {
		config.spatialisation = SpatialisationMethod_Entity;
		config.ent_num = entNum;
		config.pitch = CG_PlayerPitch( entNum );
		cent->playing_vsay = PlaySFX( sound, config );
	}
}

//==================================================================

static void CG_Event_Fall( const SyncEntityState * state, u64 parm, bool viewer ) {
	if( viewer ) {
		CG_StartFallKickEffect( ( parm + 5 ) * 10 );
	}

	Vec3 ground_position = state->origin;
	ground_position.z += state->bounds.mins.z;

	if( parm < 40 )
		return;

	float volume = ( parm - 40 ) / 300.0f;
	float pitch = 1.0f - volume * 0.125f;

	PlaySFXConfig config = PlaySFXConfigGlobal( volume );
	config.pitch = pitch;

	if( !viewer ) {
		config.spatialisation = SpatialisationMethod_Entity;
		config.ent_num = state->number;
	}

	PlaySFX( "players/fall", config );
}

static void CG_Event_Pain( SyncEntityState * state, u64 parm ) {
	static constexpr PlayerSound sounds[] = { PlayerSound_Pain25, PlayerSound_Pain50, PlayerSound_Pain75, PlayerSound_Pain100 };
	if( parm >= ARRAY_COUNT( sounds ) )
		return;

	CG_PlayerSound( state->number, sounds[ parm ], false );
	constexpr int animations[] = { TORSO_PAIN1, TORSO_PAIN2, TORSO_PAIN3 };
	CG_PModel_AddAnimation( state->number, 0, RandomElement( &cls.rng, animations ), 0, EVENT_CHANNEL );
}

static void CG_Event_Die( int corpse_ent, u64 parm ) {
	constexpr struct {
		int dead, dying;
	} animations[] = {
		{ BOTH_DEAD1, BOTH_DEATH1 },
		{ BOTH_DEAD2, BOTH_DEATH2 },
		{ BOTH_DEAD3, BOTH_DEATH3 },
	};

	int player_ent = cg_entities[ corpse_ent ].prev.ownerNum;

	bool void_death = ( parm & 1 ) != 0;
	u64 animation = ( parm >> 1 ) % ARRAY_COUNT( animations );

	CG_PlayerSound( corpse_ent, void_death ? PlayerSound_Void : PlayerSound_Death, false );
	CG_PModel_AddAnimation( corpse_ent, animations[ animation ].dead, animations[ animation ].dead, ANIM_NONE, BASE_CHANNEL );
	CG_PModel_AddAnimation( corpse_ent, animations[ animation ].dying, animations[ animation ].dying, ANIM_NONE, EVENT_CHANNEL );

	StopSFX( cg_entities[ player_ent ].playing_vsay );
}

static void CG_Event_Dash( SyncEntityState * state, u64 parm ) {
	constexpr int animations[] = { LEGS_DASH, LEGS_DASH_LEFT, LEGS_DASH_RIGHT, LEGS_DASH_BACK };
	if( parm >= ARRAY_COUNT( animations ) )
		return;

	CG_PModel_AddAnimation( state->number, animations[ parm ], 0, 0, EVENT_CHANNEL );
	CG_PlayerSound( state->number, PlayerSound_Dash, true );

	// since most dash animations jump with right leg, reset the jump to start with left leg after a dash
	cg_entities[ state->number ].jumpedLeft = true;
}

static void CG_Event_WallJump( SyncEntityState * state, u64 parm, int ev ) {
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

	CG_PlayerSound( state->number, PlayerSound_WallJump, true );
}

static void CG_Event_Jetpack( const SyncEntityState * ent, u64 parm ) {
	centity_t * cent = &cg_entities[ ent->number ];

	cent->jetpack_boost = (parm == 1);
	cent->localEffects[ LOCALEFFECT_JETPACK ] = cl.serverTime + 50;
}

void CG_JetpackEffect( centity_t * cent ) {
	if( cent->localEffects[ LOCALEFFECT_JETPACK ] == 0 ) {
		return;
	}

	float volume = cent->jetpack_boost ? 1.5f : 1.0f;

	if( cent->localEffects[ LOCALEFFECT_JETPACK ] <= cl.serverTime ) {
		if( cent->localEffects[ LOCALEFFECT_JETPACK ] ) {
			if( ISVIEWERENTITY( cent->current.number ) ) {
				PlaySFX( "perks/jetpack/stop", PlaySFXConfigGlobal( volume ) );
			}
			else {
				PlaySFX( "perks/jetpack/stop", PlaySFXConfigEntity( cent->current.number, volume ) );
			}
		}
		cent->localEffects[ LOCALEFFECT_JETPACK ] = 0;
		cent->jetpack_sound = PlayImmediateSFX( "perks/jetpack/idle", cent->jetpack_sound, PlaySFXConfigEntity( cent->current.number ) );
		return;
	}

	Vec4 team_color = CG_TeamColorVec4( cent->current.team );
	Vec3 pos = cent->current.origin;
	pos.z -= 10;
	DoVisualEffect( "vfx/movement/jetpack", pos, cent->current.origin2, 1.0f, team_color );
	cent->jetpack_sound = PlayImmediateSFX( "perks/jetpack/hum", cent->jetpack_sound, PlaySFXConfigEntity( cent->current.number, volume ) );
	if( cent->jetpack_boost ) {
		DoVisualEffect( "vfx/movement/jetpack_boost", pos, cent->current.origin2, 1.0f, team_color );
	}
}

static void CG_PlayJumpSound( const SyncEntityState * state, JumpType j ) {
	switch( j ) {
	case JumpType_Normal:
		return CG_PlayerSound( state->number, PlayerSound_Jump, true );
	case JumpType_MidgetCharge:
		return CG_PlayerSound( state->number, PlayerSound_WallJump, true );
	}
}

static void CG_Event_Jump( SyncEntityState * state, u64 parm ) {
	CG_PlayJumpSound( state, JumpType( parm ) );

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
			CG_PModel_AddAnimation( ent->number, 0, TORSO_WEAPON_SWITCHIN, 0, EVENT_CHANNEL );
			CG_ViewWeapon_AddAnimation( ent->number, "activate" );

			StringHash sfx = EMPTY_HASH;
			bool is_gadget = ( parm & 1 ) != 0;
			if( !is_gadget ) {
				sfx = GetWeaponModelMetadata( WeaponType( parm >> 1 ) )->switch_in_sound;
			}
			else {
				sfx = GetGadgetModelMetadata( GadgetType( parm >> 1 ) )->switch_in_sound;
			}

			if( viewer ) {
				PlaySFX( sfx );
			}
			else {
				PlaySFX( sfx, PlaySFXConfigEntity( ent->number ) );
			}
		} break;

		case EV_SMOOTHREFIREWEAPON: // the server never sends this event
			if( predicted ) {
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
			WeaponType weapon = WeaponType( parm & 0xFF );
			if( weapon <= Weapon_None || weapon >= Weapon_Count )
				return;

			// check the owner for predicted case
			if( ISVIEWERENTITY( ent->ownerNum ) && ev < PREDICTABLE_EVENTS_MAX && predicted != cg.view.playerPrediction ) {
				return;
			}

			int owner;
			Vec3 origin, angles;
			if( predicted ) {
				owner = ent->number;
				origin = cg.predictedPlayerState.pmove.origin;
				origin.z += cg.predictedPlayerState.viewheight;
				angles = cg.predictedPlayerState.viewangles;
			}
			else {
				owner = ent->ownerNum;
				origin = ent->origin;
				angles = ent->origin2;
			}

			CG_FireWeaponEvent( owner, weapon );
			CG_ViewWeapon_AddAnimation( owner, "fire" );

			Vec3 dir;
			AngleVectors( angles, &dir, NULL, NULL );

			if( weapon == Weapon_Railgun ) {
				FireRailgun( origin, dir, owner );
			}
			else if( weapon == Weapon_Shotgun ) {
				CG_Event_FireShotgun( origin, dir, owner, team_color );
			}
			else if( weapon == Weapon_Laser ) {
				CG_Event_LaserBeam( origin, dir, owner );
			}
			else if( weapon == Weapon_Pistol || weapon == Weapon_MachineGun || weapon == Weapon_Deagle || weapon == Weapon_BurstRifle || weapon == Weapon_Sniper || weapon == Weapon_AutoSniper /* || weapon == Weapon_Minigun */ ) {
				u16 entropy = parm >> 8;
				s16 zoom_time = parm >> 24;
				CG_Event_FireBullet( origin, dir, entropy, zoom_time, weapon, owner, team_color );
			}

			// if( predicted && weapon == Weapon_Minigun ) {
			// 	cg.predictedPlayerState.pmove.velocity -= dir * GS_GetWeaponDef( Weapon_Minigun )->knockback;
			// }
		} break;

		case EV_USEGADGET: {
			GadgetType gadget = GadgetType( parm & 0xFF );
			StringHash sfx = GetGadgetModelMetadata( gadget )->use_sound;

			int owner = predicted ? ent->number : ent->ownerNum;
			if( viewer ) {
				PlaySFX( sfx );
			}
			else {
				PlaySFX( sfx, PlaySFXConfigEntity( owner ) );
			}
		} break;

		case EV_NOAMMOCLICK:
			if( viewer ) {
				PlaySFX( "weapons/noammo" );
			}
			else {
				PlaySFX( "weapons/noammo", PlaySFXConfigPosition( ent->origin ) );
			}
			break;

		case EV_RELOAD: {
			if( parm <= Weapon_None || parm >= Weapon_Count )
				return;

			StringHash sfx = GetWeaponModelMetadata( WeaponType( parm ) )->reload_sound;

			if( viewer ) {
				PlaySFX( sfx );
				CG_ViewWeapon_AddAnimation( ent->number, "reload" );
			}
			else {
				PlaySFX( sfx, PlaySFXConfigPosition( ent->origin ) );
			}
		} break;

		case EV_ZOOM_IN:
		case EV_ZOOM_OUT: {
			if( parm <= Weapon_None || parm >= Weapon_Count )
				return;

			const WeaponModelMetadata * weapon = GetWeaponModelMetadata( WeaponType( parm ) );
			StringHash sfx = ev == EV_ZOOM_IN ? weapon->zoom_in_sound : weapon->zoom_out_sound;

			if( viewer ) {
				PlaySFX( sfx );
			}
			else {
				PlaySFX( sfx, PlaySFXConfigPosition( ent->origin ) );
			}
		} break;

		case EV_DASH:
			CG_Event_Dash( ent, parm );
			break;

		case EV_WALLJUMP:
			CG_Event_WallJump( ent, parm, ev );
			break;

		case EV_JETPACK:
			CG_Event_Jetpack( ent, parm );
			break;

		case EV_JUMP:
			CG_Event_Jump( ent, parm );
			break;

		case EV_JUMP_PAD:
			CG_PlayJumpSound( ent, JumpType_Normal );
			CG_PModel_AddAnimation( ent->number, LEGS_JUMP_NEUTRAL, 0, 0, EVENT_CHANNEL );
			break;

		case EV_FALL:
			CG_Event_Fall( ent, parm, viewer );
			break;

			//  NON PREDICTABLE EVENTS

		case EV_WEAPONDROP: // deactivate is not predictable
			CG_PModel_AddAnimation( ent->number, 0, TORSO_WEAPON_SWITCHOUT, 0, EVENT_CHANNEL );
			CG_ViewWeapon_AddAnimation( ent->number, "drop" );
			break;

		case EV_PAIN:
			CG_Event_Pain( ent, parm );
			break;

		case EV_DIE:
			CG_Event_Die( ent->ownerNum, parm );
			break;

		case EV_BOMB_EXPLOSION:
			DoVisualEffect( "models/bomb/explosion", ent->origin, Vec3( 0.0f, 0.0f, 1.0f ), 1.0f, vec4_white );
			PlaySFX( "models/bomb/explode", PlaySFXConfigPosition( ent->origin ) );
			break;

		case EV_GIB:
			SpawnGibs( ent->origin, ent->origin2, parm, team_color );
			break;

		case EV_PLAYER_RESPAWN:
			if( ( unsigned ) ent->ownerNum == cgs.playerNum + 1 ) {
				cg.recoiling = false;
				cg.damage_effect = 0.0f;
			}
			break;

		case EV_PLAYER_TELEPORT_IN:
			PlaySFX( "sounds/world/tele_in", PlaySFXConfigPosition( ent->origin ) );
			break;

		case EV_PLAYER_TELEPORT_OUT:
			PlaySFX( "sounds/world/tele_in", PlaySFXConfigPosition( ent->origin ) );
			break;

		case EV_ARBULLET_EXPLOSION: {
			Vec3 normal = U64ToDir( parm );
			DoVisualEffect( "weapons/ar/explosion", ent->origin, normal, 1.0f, team_color );
			PlaySFX( "weapons/ar/explode", PlaySFXConfigPosition( ent->origin ) );
		} break;

		case EV_BUBBLE_EXPLOSION:
			DoVisualEffect( "weapons/bg/explosion", ent->origin, Vec3( 0.0f, 0.0f, 1.0f ), 1.0f, team_color );
			PlaySFX( "weapons/bg/explode", PlaySFXConfigPosition( ent->origin ) );
			break;

		case EV_BOLT_EXPLOSION: {
			Vec3 normal = U64ToDir( parm );
			RailgunImpact( ent->origin, normal, 0, team_color );
		} break;

		case EV_GRENADE_EXPLOSION: {
			Vec3 normal = U64ToDir( parm );
			DoVisualEffect( "vfx/explosion", ent->origin, normal, 1.0f, team_color );
			PlaySFX( "weapons/gl/explode", PlaySFXConfigPosition( ent->origin ) );
		} break;

		case EV_STICKY_EXPLOSION: {
			Vec3 normal = U64ToDir( parm );
			DoVisualEffect( "weapons/sticky/explosion", ent->origin, normal, 1.0f, team_color );
			if( parm == 0 ) {
				PlaySFX( "weapons/sticky/explode", PlaySFXConfigPosition( ent->origin ) );
			}
		} break;

		case EV_STICKY_IMPACT: {
			Vec3 normal = U64ToDir( parm );
			DoVisualEffect( "weapons/sticky/impact", ent->origin, normal, 24, team_color );
			PlaySFX( "weapons/sticky/impact", PlaySFXConfigPosition( ent->origin ) );
		} break;

		case EV_ROCKET_EXPLOSION: {
			Vec3 normal = U64ToDir( parm );
			DoVisualEffect( "vfx/explosion", ent->origin, normal, 1.0f, team_color );
			PlaySFX( "weapons/rl/explode", PlaySFXConfigPosition( ent->origin ) );
		} break;

		case EV_GRENADE_BOUNCE: {
			float volume = Min2( 1.0f, parm / float( U16_MAX ) );
			PlaySFX( "weapons/gl/bounce", PlaySFXConfigEntity( ent->number, volume ) );
		} break;

		case EV_RIFLEBULLET_IMPACT: {
			Vec3 normal = U64ToDir( parm );
			DoVisualEffect( "vfx/bulletsparks", ent->origin, normal, 24, team_color );
			PlaySFX( "weapons/bullet_impact", PlaySFXConfigPosition( ent->origin ) );
		} break;

		case EV_STAKE_IMPACT: {
			Vec3 normal = U64ToDir( parm );
			DoVisualEffect( "weapons/stake/hit", ent->origin, normal, 1.0f, team_color );
			PlaySFX( "weapons/stake/hit", PlaySFXConfigPosition( ent->origin ) );
		} break;

		case EV_STAKE_IMPALE: {
			Vec3 normal = U64ToDir( parm );
			DoVisualEffect( "weapons/stake/impale", ent->origin, normal, 1.0f, team_color );
			PlaySFX( "weapons/stake/impale", PlaySFXConfigPosition( ent->origin ) );
		} break;

		case EV_BLAST_IMPACT: {
			Vec3 normal = U64ToDir( parm );
			DoVisualEffect( "weapons/mb/hit", ent->origin, normal, 1.0f, team_color );
			PlaySFX( "weapons/mb/hit", PlaySFXConfigPosition( ent->origin ) );
		} break;

		case EV_BLAST_BOUNCE: {
			Vec3 normal = U64ToDir( parm );
			DoVisualEffect( "weapons/mb/bounce", ent->origin, normal, 1.0f, team_color );
			PlaySFX( "weapons/mb/bounce", PlaySFXConfigPosition( ent->origin ) );
		} break;

		case EV_BLOOD: {
			Vec3 dir = ent->origin2;
			Vec3 tangent, bitangent;
			OrthonormalBasis( dir, &tangent, &bitangent );

			int damage = parm;
			float p = damage / 20.0f;

			if( !ISVIEWERENTITY( ent->ownerNum ) ) {
				DoVisualEffect( "vfx/blood_spray", ent->origin, dir, damage, team_color );
			}

			while( true ) {
				if( !Probability( &cls.rng, p ) )
					break;

				Vec3 random_dir = Normalize( dir + tangent * RandomFloat11( &cls.rng ) * 0.1f + bitangent * RandomFloat11( &cls.rng ) * 0.1f );
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

					float angle = RandomUniformFloat( &cls.rng, 0.0f, Radians( 360.0f ) );
					float min_size = Lerp( 20.0f, Unlerp01( 5, damage, 50 ), 64.0f );
					float size = min_size * RandomUniformFloat( &cls.rng, 0.75f, 1.5f );

					AddPersistentDecal( trace.endpos, trace.plane.normal, size, angle, RandomElement( &cls.rng, decals ), team_color, 30000, 10.0f );
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
			Vec3 origin = ent->origin + ( ent->bounds.mins + ent->bounds.maxs ) * 0.5f;
			PlaySFX( StringHash( parm ), PlaySFXConfigPosition( origin ) );
		} break;

		case EV_VSAY:
			CG_StartVsay( ent->ownerNum, parm );
			break;

		case EV_TBAG:
			PlaySFX( "sounds/tbag/tbag", PlaySFXConfigPosition( ent->origin, parm / 255.0f ) );
			break;

		case EV_SPRAY:
			AddSpray( ent->origin, ent->origin2, ent->angles, ent->scale.z, parm );
			PlaySFX( "sounds/spray/spray", PlaySFXConfigPosition( ent->origin ) );
			break;

		case EV_DAMAGE:
			CG_AddDamageNumber( ent, parm );
			break;

		case EV_HEADSHOT:
			PlaySFX( "sounds/headshot/headshot", PlaySFXConfigPosition( ent->origin ) );
			break;

		case EV_VFX:
			DoVisualEffect( StringHash( parm ), ent->origin, Vec3( 0.0f, 0.0f, 1.0f ), 1, vec4_white );
			break;

		case EV_SUICIDE_BOMB_ANNOUNCEMENT:
			PlaySFX( "sounds/vsay/helena", PlaySFXConfigEntity( ent->number ) );
			break;

		case EV_SUICIDE_BOMB_BEEP:
			PlaySFX( "sounds/beep", PlaySFXConfigEntity( ent->number ) );
			break;

		case EV_SUICIDE_BOMB_EXPLODE: {
			Vec3 normal = U64ToDir( parm );
			DoVisualEffect( "vfx/explosion", ent->origin, normal, 1.0f, team_color );
			PlaySFX( "weapons/rl/explode", PlaySFXConfigPosition( ent->origin ) );
		} break;

		case EV_STUN_GRENADE_EXPLOSION:
			DoVisualEffect( "gadgets/flash/explode", ent->origin, Vec3(), 1.0f, vec4_white );
			PlaySFX( "gadgets/flash/explode", PlaySFXConfigPosition( ent->origin ) );
			break;
	}
}

#define ISEARLYEVENT( ev ) ( ev == EV_WEAPONDROP )

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
					PlaySFX( cgs.media.sfxWeaponHit[ parm ] );
					CG_ScreenCrosshairDamageUpdate();
				}
				else { // killed an enemy
					PlaySFX( "sounds/misc/kill" );
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
