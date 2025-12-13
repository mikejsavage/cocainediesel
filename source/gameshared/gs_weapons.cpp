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

#include "qcommon/base.h"
#include "qcommon/qcommon.h"
#include "qcommon/rng.h"
#include "gameshared/gs_public.h"
#include "gameshared/gs_weapons.h"

void GS_TraceBullet( const gs_state_t * gs, trace_t * trace, trace_t * wallbang_trace, Vec3 start, Vec3 dir, Vec3 right, Vec3 up, Vec2 spread, int range, int ignore, int timeDelta ) {
	Vec3 end = start + ( dir + right * spread.x + up * spread.y ) * range;

	*trace = gs->api.Trace( start, MinMax3( 0.0f ), end, ignore, SolidMask_WallbangShot, timeDelta );

	if( wallbang_trace != NULL ) {
		*wallbang_trace = gs->api.Trace( start, MinMax3( 0.0f ), trace->endpos, ignore, Solid_Wallbangable, timeDelta );
	}
}

Vec2 RandomSpreadPattern( u16 entropy, float spread ) {
	RNG rng = NewRNG( entropy, 0 );
	return spread * UniformSampleInsideCircle( &rng );
}

float ZoomSpreadness( s16 zoom_time, WeaponType w, bool alt ) {
	float frac = 1.0f - float( zoom_time ) / float( ZOOMTIME );
	return frac * GetWeaponDefProperties( w )->unzoom_spread;
}

Vec2 FixedSpreadPattern( int i, float spread ) {
	float x = i * 2.4f;
	return Vec2( cosf( x ), sinf( x ) ) * spread * sqrtf( x );
}

trace_t GS_TraceLaserBeam( const gs_state_t * gs, Vec3 origin, EulerDegrees3 angles, float range, int ignore, int timeDelta ) {
	Vec3 dir;
	AngleVectors( angles, &dir, NULL, NULL );
	Vec3 end = origin + dir * range;
	return gs->api.Trace( origin, MinMax3( 0.5f ), end, ignore, SolidMask_Shot, timeDelta );
}

WeaponSlot * FindWeapon( SyncPlayerState * player, WeaponType weapon ) {
	for( size_t i = 0; i < ARRAY_COUNT( player->weapons ); i++ ) {
		if( player->weapons[ i ].weapon == weapon ) {
			return &player->weapons[ i ];
		}
	}

	return NULL;
}

const WeaponSlot * FindWeapon( const SyncPlayerState * player, WeaponType weapon ) {
	return FindWeapon( const_cast< SyncPlayerState * >( player ), weapon );
}

WeaponSlot * GetSelectedWeapon( SyncPlayerState * player ) {
	return FindWeapon( player, player->weapon );
}

const WeaponSlot * GetSelectedWeapon( const SyncPlayerState * player ) {
	return FindWeapon( player, player->weapon );
}

static bool HasAmmo( WeaponType w, bool altfire, const WeaponSlot * slot ) {
	return GetWeaponDefProperties( w )->clip_size == 0 || slot->ammo >= GetWeaponDefFire( w, altfire )->ammo_use;
}

enum TransitionType {
	TransitionType_Normal,
	TransitionType_NoReset,
	TransitionType_ForceReset,
};

struct ItemStateTransition {
	TransitionType type;
	WeaponState state;

	ItemStateTransition() = default;

	ItemStateTransition( WeaponState s ) {
		type = TransitionType_Normal;
		state = s;
	}
};




template <EventType event>
static void FireWeaponEvent( const gs_state_t * gs, SyncPlayerState * ps, const UserCommand * cmd, WeaponType weapon, bool altfire ) {
	// 8 | 8 | 16 | 32
	u64 parm = weapon | u64( altfire ) << 8 | u64( cmd->entropy ) << 16 | ( u64( ps->zoom_time ) << 32 );
	gs->api.PredictedEvent( ps->POVnum, event, parm );
}



static ItemStateTransition NoReset( WeaponState state ) {
	ItemStateTransition t;
	t.type = TransitionType_NoReset;
	t.state = state;
	return t;
}

static ItemStateTransition ForceReset( WeaponState state ) {
	ItemStateTransition t;
	t.type = TransitionType_ForceReset;
	t.state = state;
	return t;
}

using ItemStateThinkCallback = ItemStateTransition ( * )( const gs_state_t * gs, WeaponState state, SyncPlayerState * ps, const UserCommand * cmd );

struct ItemState {
	WeaponState state;
	ItemStateThinkCallback think;

	constexpr ItemState( WeaponState s, ItemStateThinkCallback t ) : state( s ), think( t ) { }
};

static ItemStateTransition GenericDelay( WeaponState state, SyncPlayerState * ps, s64 delay, ItemStateTransition next ) {
	return ps->weapon_state_time >= delay ? next : state;
}

static void HandleZoom( const gs_state_t * gs, SyncPlayerState * ps, const UserCommand * cmd ) {
	const WeaponDef::Properties * def = GetWeaponDefProperties( ps->weapon );
	const WeaponSlot * slot = GetSelectedWeapon( ps );

	s16 last_zoom_time = ps->zoom_time;
	bool can_zoom = ( ps->weapon_state == WeaponState_Idle ||
					( ( ps->weapon_state == WeaponState_Firing || ps->weapon_state == WeaponState_FiringEntireClip ) &&
						HasAmmo( ps->weapon, false, slot ) ) )
				&& ( ps->pmove.features & PMFEAT_SCOPE );

	if( can_zoom && def->zoom_fov != 0 && HasAllBits( cmd->buttons, Button_Attack2 ) ) {
		ps->zoom_time = Min2( ps->zoom_time + cmd->msec, ZOOMTIME );
		if( last_zoom_time == 0 ) {
			gs->api.PredictedEvent( ps->POVnum, EV_ZOOM_IN, ps->weapon );
		}
	}
	else {
		ps->zoom_time = Max2( 0, ps->zoom_time - cmd->msec );
		if( ps->zoom_time == 0 && last_zoom_time != 0 ) {
			gs->api.PredictedEvent( ps->POVnum, EV_ZOOM_OUT, ps->weapon );
		}
	}
}

static void FireWeapon( const gs_state_t * gs, SyncPlayerState * ps, const UserCommand * cmd, bool smooth, bool altfire ) {
	const WeaponDef::Properties * def = GetWeaponDefProperties( ps->weapon );
	WeaponSlot * slot = GetSelectedWeapon( ps );

	if( smooth ) {
		FireWeaponEvent<EV_SMOOTHREFIREWEAPON>( gs, ps, cmd, ps->weapon, altfire );
	}
	else {
		FireWeaponEvent<EV_FIREWEAPON>( gs, ps, cmd, ps->weapon, altfire );
	}

	if( def->clip_size > 0 ) {
		slot->ammo -= GetWeaponDefFire( ps->weapon, altfire )->ammo_use;
	}
}

static ItemStateTransition Dispatch( const gs_state_t * gs, WeaponState state, SyncPlayerState * ps, const UserCommand * cmd ) {
	if( ps->pending_weapon == Weapon_None && !ps->pending_gadget ) {
		return state;
	}

	if( ps->pending_gadget && ps->gadget_ammo > 0 ) {
		ps->using_gadget = true;
	}
	else {
		ps->weapon = ps->pending_weapon;
	}

	ps->pending_weapon = Weapon_None;
	ps->pending_gadget = false;

	if( state != WeaponState_DispatchQuiet ) {
		u64 parm = ps->using_gadget ? 1 : 0;
		parm |= ps->using_gadget ? ( ps->gadget << 1 ) : ( ps->weapon << 1 );
		gs->api.PredictedEvent( ps->POVnum, EV_WEAPONACTIVATE, parm );
	}

	return WeaponState_SwitchingIn;
}

static constexpr ItemState dispatch_states[] = {
	ItemState( WeaponState_Dispatch, Dispatch ),
	ItemState( WeaponState_DispatchQuiet, Dispatch ),
};

static ItemStateTransition AllowWeaponSwitch( const gs_state_t * gs, SyncPlayerState * ps, ItemStateTransition otherwise ) {
	bool switching = false;
	switching = switching || ( ps->pending_weapon != Weapon_None && ps->pending_weapon != ps->weapon );
	switching = switching || ( ps->pending_gadget && ps->gadget_ammo > 0 && !ps->using_gadget );

	if( !switching ) {
		ps->pending_weapon = Weapon_None;
		ps->pending_gadget = false;
		return otherwise;
	}

	gs->api.PredictedEvent( ps->POVnum, EV_WEAPONDROP, 0 );

	return WeaponState_SwitchingOut;
}

static constexpr ItemState generic_gun_switching_in_state =
	ItemState( WeaponState_SwitchingIn, []( const gs_state_t * gs, WeaponState state, SyncPlayerState * ps, const UserCommand * cmd ) -> ItemStateTransition {
		return AllowWeaponSwitch( gs, ps, GenericDelay( state, ps, GetWeaponDefProperties( ps->weapon )->switch_in_time, WeaponState_Idle ) );
	} );

static constexpr ItemState generic_gun_switching_out_state =
	ItemState( WeaponState_SwitchingOut, []( const gs_state_t * gs, WeaponState state, SyncPlayerState * ps, const UserCommand * cmd ) -> ItemStateTransition {
		const WeaponDef::Properties * def = GetWeaponDefProperties( ps->weapon );
		if( ps->weapon_state_time >= def->switch_out_time ) {
			ps->last_weapon = ps->weapon;
			ps->weapon = Weapon_None;
			return WeaponState_Dispatch;
		}

		return state;
	} );

static constexpr ItemState generic_gun_refire_state =
	ItemState( WeaponState_Firing, []( const gs_state_t * gs, WeaponState state, SyncPlayerState * ps, const UserCommand * cmd ) -> ItemStateTransition {
		u16 refire_time = GetWeaponDefFire( ps->weapon, ps->weapon_alt_fire )->refire_time;
		return GenericDelay( state, ps, refire_time, AllowWeaponSwitch( gs, ps, WeaponState_Idle ) );
	} );

static UserCommandButton WeaponAttackBits( const WeaponDef * def ) {
	return def->has_altfire ? ( Button_Attack1 | Button_Attack2 ) : Button_Attack1;
}

static constexpr ItemState generic_gun_states[] = {
	generic_gun_switching_in_state,
	generic_gun_switching_out_state,
	generic_gun_refire_state,

	ItemState( WeaponState_Idle, []( const gs_state_t * gs, WeaponState state, SyncPlayerState * ps, const UserCommand * cmd ) -> ItemStateTransition {
		const WeaponDef * def = GS_GetWeaponDef( ps->weapon );
		const WeaponDef::Properties * prop = GetWeaponDefProperties( ps->weapon );
		WeaponSlot * slot = GetSelectedWeapon( ps );

		if( HasAnyBit( cmd->buttons, WeaponAttackBits( def ) ) ) {
			bool altfire = def->has_altfire && HasAllBits( cmd->buttons, Button_Attack2 );
			if( HasAmmo( ps->weapon, altfire, slot ) ) {
				FireWeapon( gs, ps, cmd, false, altfire );

				ps->weapon_alt_fire = altfire;
				switch( GetWeaponDefFire( ps->weapon, altfire )->firing_mode ) {
					case FiringMode_Auto: return WeaponState_Firing;
					case FiringMode_SemiAuto: return WeaponState_FiringSemiAuto;
					case FiringMode_Smooth: return WeaponState_FiringSmooth;
					case FiringMode_Clip: return WeaponState_FiringEntireClip;
				}
			} else {
				gs->api.PredictedEvent( ps->POVnum, EV_NOAMMOCLICK, ps->weapon_state_time );
			}
		}

		bool has_ammo = HasAmmo( ps->weapon, false, slot );
		if ( def->has_altfire ) {
			has_ammo = has_ammo || HasAmmo( ps->weapon, true, slot );
		}

		bool wants_reload = HasAllBits( cmd->buttons, Button_Reload ) && prop->clip_size != 0 && slot->ammo < prop->clip_size;
		if( wants_reload || !has_ammo ) {
			return WeaponState_Reloading;
		}

		return AllowWeaponSwitch( gs, ps, WeaponState_Idle );
	} ),

	ItemState( WeaponState_FiringSemiAuto, []( const gs_state_t * gs, WeaponState state, SyncPlayerState * ps, const UserCommand * cmd ) -> ItemStateTransition {
		if( HasAllBits( cmd->buttons, Button_Attack1 ) ) {
			u16 refire_time = GetWeaponDefFire( ps->weapon, ps->weapon_alt_fire )->refire_time;
			ps->weapon_state_time = Min2( refire_time, ps->weapon_state_time );
			ps->weapon_alt_fire = false;

			if( ps->weapon_state_time >= refire_time ) {
				return AllowWeaponSwitch( gs, ps, state );
			}

			return state;
		}

		return NoReset( WeaponState_Firing );
	} ),

	ItemState( WeaponState_FiringSmooth, []( const gs_state_t * gs, WeaponState state, SyncPlayerState * ps, const UserCommand * cmd ) -> ItemStateTransition {
		u16 refire_time = GetWeaponDefFire( ps->weapon, ps->weapon_alt_fire )->refire_time;
		if( ps->weapon_state_time < refire_time ) {
			return state;
		}

		WeaponSlot * slot = GetSelectedWeapon( ps );

		if( !HasAllBits( cmd->buttons, ps->weapon_alt_fire ? Button_Attack2 : Button_Attack1 ) ||
			!HasAmmo( ps->weapon, ps->weapon_alt_fire, slot ) )
		{
			return WeaponState_Idle;
		}

		FireWeapon( gs, ps, cmd, true, ps->weapon_alt_fire );

		return AllowWeaponSwitch( gs, ps, ForceReset( state ) );
	} ),

	ItemState( WeaponState_FiringEntireClip, []( const gs_state_t * gs, WeaponState state, SyncPlayerState * ps, const UserCommand * cmd ) -> ItemStateTransition {
		u16 refire_time = GetWeaponDefFire( ps->weapon, ps->weapon_alt_fire )->refire_time;
		if( ps->weapon_state_time < refire_time ) {
			return state;
		}

		WeaponSlot * slot = GetSelectedWeapon( ps );
		if( slot->ammo == 0 ) {
			return WeaponState_Idle;
		}

		FireWeapon( gs, ps, cmd, false, ps->weapon_alt_fire );

		return ForceReset( state );
	} ),

	ItemState( WeaponState_Reloading, []( const gs_state_t * gs, WeaponState state, SyncPlayerState * ps, const UserCommand * cmd ) -> ItemStateTransition {
		if( ps->weapon_state_time == 0 ) {
			gs->api.PredictedEvent( ps->POVnum, EV_RELOAD, ps->weapon );
		}

		const WeaponDef::Properties * def = GetWeaponDefProperties( ps->weapon );
		WeaponSlot * slot = GetSelectedWeapon( ps );

		if( HasAnyBit( cmd->buttons, WeaponAttackBits( GS_GetWeaponDef( ps->weapon ) ) ) ) {
			gs->api.PredictedEvent( ps->POVnum, EV_NOAMMOCLICK, ps->weapon_state_time );
		}

		if( ps->weapon_state_time < def->reload_time ) {
			if( !def->staged_reload ) {
				slot->ammo = 0;
			}
			return AllowWeaponSwitch( gs, ps, state );
		}

		if( def->staged_reload ) {
			ps->weapon_state_time = def->reload_time;
			return NoReset( WeaponState_StagedReloading );
		}

		slot->ammo = def->clip_size;

		return WeaponState_Idle;
	} ),

	ItemState( WeaponState_StagedReloading, []( const gs_state_t * gs, WeaponState state, SyncPlayerState * ps, const UserCommand * cmd ) -> ItemStateTransition {
		if( ps->weapon_state_time == 0 ) {
			gs->api.PredictedEvent( ps->POVnum, EV_RELOAD, ps->weapon );
		}

		const WeaponDef::Properties * def = GetWeaponDefProperties( ps->weapon );
		WeaponSlot * slot = GetSelectedWeapon( ps );

		if( ps->weapon_state_time < def->reload_time ) {
			return AllowWeaponSwitch( gs, ps, state );
		}

		slot->ammo++;

		if( ( HasAnyBit( cmd->buttons, Button_Attack1 ) && HasAmmo( ps->weapon, false, slot ) ) || 
			( GS_GetWeaponDef( ps->weapon )->has_altfire && HasAnyBit( cmd->buttons, Button_Attack2 ) && HasAmmo( ps->weapon, true, slot ) ) )
		{
			return WeaponState_Idle;
		}

		return slot->ammo == def->clip_size ? WeaponState_Idle : ForceReset( WeaponState_StagedReloading );
	} ),
};

static constexpr ItemState bat_states[] = {
	generic_gun_switching_in_state,
	generic_gun_switching_out_state,
	generic_gun_refire_state,

	ItemState( WeaponState_Idle, []( const gs_state_t * gs, WeaponState state, SyncPlayerState * ps, const UserCommand * cmd ) -> ItemStateTransition {
		if( HasAllBits( cmd->buttons, Button_Attack1 ) ) {
			return WeaponState_Cooking;
		}

		return AllowWeaponSwitch( gs, ps, WeaponState_Idle );
	} ),

	ItemState( WeaponState_Cooking, []( const gs_state_t * gs, WeaponState state, SyncPlayerState * ps, const UserCommand * cmd ) -> ItemStateTransition {
		const WeaponDef::Properties * def = GetWeaponDefProperties( ps->weapon );

		if( !HasAllBits( cmd->buttons, Button_Attack1 ) ) {
			FireWeaponEvent<EV_FIREWEAPON>( gs, ps, cmd, ps->weapon, false );
			ps->weapon_alt_fire = false;
			return WeaponState_Firing;
		}

		ps->weapon_state_time = Min2( def->reload_time, ps->weapon_state_time );

		return state;
	} ),
};

static constexpr ItemState generic_throwable_states[] = {
	ItemState( WeaponState_SwitchingIn, []( const gs_state_t * gs, WeaponState state, SyncPlayerState * ps, const UserCommand * cmd ) -> ItemStateTransition {
		const GadgetDef * def = GetGadgetDef( ps->gadget );
		return AllowWeaponSwitch( gs, ps, GenericDelay( state, ps, def->switch_in_time, WeaponState_Cooking ) );
	} ),

	ItemState( WeaponState_Cooking, []( const gs_state_t * gs, WeaponState state, SyncPlayerState * ps, const UserCommand * cmd ) -> ItemStateTransition {
		const GadgetDef * def = GetGadgetDef( ps->gadget );

		if( !HasAllBits( cmd->buttons, Button_Gadget ) || def->cook_time == 0 ) {
			gs->api.PredictedUseGadget( ps->POVnum, ps->gadget, ps->weapon_state_time, false );
			ps->gadget_ammo--;
			return WeaponState_Throwing;
		}

		ps->weapon_state_time = Min2( def->cook_time, ps->weapon_state_time );

		return AllowWeaponSwitch( gs, ps, state );
	} ),

	ItemState( WeaponState_Throwing, []( const gs_state_t * gs, WeaponState state, SyncPlayerState * ps, const UserCommand * cmd ) -> ItemStateTransition {
		const GadgetDef * def = GetGadgetDef( ps->gadget );
		if( ps->weapon_state_time >= def->using_time ) {
			ps->using_gadget = false;
			ps->pending_weapon = ps->last_weapon;
			return WeaponState_Dispatch;
		}

		return state;
	} ),

	ItemState( WeaponState_SwitchingOut, []( const gs_state_t * gs, WeaponState state, SyncPlayerState * ps, const UserCommand * cmd ) -> ItemStateTransition {
		const GadgetDef * def = GetGadgetDef( ps->gadget );
		if( ps->weapon_state_time >= def->switch_out_time ) {
			ps->using_gadget = false;
			return WeaponState_Dispatch;
		}

		return state;
	} ),
};

static bool MartyrStage( SyncPlayerState * ps, int stage, u64 delay ) {
	if( ps->gadget_ammo >= stage )
		return false;

	if( ps->weapon_state_time < delay )
		return false;

	ps->gadget_ammo = stage;
	return true;
}

static constexpr ItemState martyr_states[] = {
	ItemState( WeaponState_Firing, []( const gs_state_t * gs, WeaponState state, SyncPlayerState * ps, const UserCommand * cmd ) -> ItemStateTransition {
		constexpr StringHash bomb_announcement = "sounds/vsay/helena";
		constexpr StringHash bomb_beep = "sounds/beep";

		if( MartyrStage( ps, 2, 0 ) ) {
			// TODO: randomise
			gs->api.PredictedEvent( ps->POVnum, EV_SOUND_ENT, bomb_announcement.hash );
		}

		u64 beep_interval = 768;
		u64 beep_delay = beep_interval;
		constexpr int num_beeps = 20;

		for( int beep = 3; beep < 3 + num_beeps; beep++ ) {
			if( MartyrStage( ps, beep, beep_delay ) ) {
				gs->api.PredictedEvent( ps->POVnum, EV_SOUND_ENT, bomb_beep.hash );
			}

			beep_interval = Max2( 64.0f, beep_interval / 1.5f );
			beep_delay += beep_interval;
		}

		if( MartyrStage( ps, 100, 2500 ) ) {
			gs->api.PredictedEvent( ps->POVnum, EV_MARTYR_EXPLODE, 0 );
		}

		return state;
	} ),
};

static constexpr Span< const ItemState > dispatch_state_machine = StaticSpan( dispatch_states );
static constexpr Span< const ItemState > generic_gun_state_machine = StaticSpan( generic_gun_states );
static constexpr Span< const ItemState > bat_state_machine = StaticSpan( bat_states );
static constexpr Span< const ItemState > generic_throwable_state_machine = StaticSpan( generic_throwable_states );
static constexpr Span< const ItemState > martyr_state_machine = StaticSpan( martyr_states );

static Span< const ItemState > FindItemStateMachine( SyncPlayerState * ps ) {
	if( ps->weapon == Weapon_None && !ps->using_gadget ) {
		return dispatch_state_machine;
	}

	if( ps->using_gadget ) {
		switch( ps->gadget ) {
			case Gadget_Axe:
			case Gadget_Flash:
			case Gadget_Rocket:
			case Gadget_Shuriken:
				return generic_throwable_state_machine;

			case Gadget_Martyr:
				return martyr_state_machine;
		}
	}

	switch( ps->weapon ) {
		case Weapon_Bat:
			return bat_state_machine;
		default:
			return generic_gun_state_machine;
	}
}

static const ItemState * FindState( Span< const ItemState > sm, WeaponState state ) {
	for( const ItemState & s : sm ) {
		if( s.state == state ) {
			return &s;
		}
	}

	return NULL;
}

static u16 SaturatingAdd( u16 a, u16 b ) {
	return Min2( u32( a ) + u32( b ), u32( U16_MAX ) );
}

void ClearInventory( SyncPlayerState * ps ) {
	memset( ps->weapons, 0, sizeof( ps->weapons ) );

	ps->weapon = Weapon_None;
	ps->pending_weapon = Weapon_None;
	ps->weapon_state = WeaponState_Dispatch;
	ps->weapon_state_time = 0;
	ps->zoom_time = 0;

	ps->gadget = Gadget_None;
	ps->gadget_ammo = 0;
	ps->using_gadget = false;
	ps->pending_gadget = false;
}

void UpdateWeapons( const gs_state_t * gs, SyncPlayerState * ps, UserCommand cmd, int timeDelta ) {
	if( gs->gameState.paused ) {
		return;
	}

	if( ps->pmove.pm_type != PM_NORMAL ) {
		ClearInventory( ps );
		return;
	}

	ps->weapon_state_time = SaturatingAdd( ps->weapon_state_time, cmd.msec );

	if( cmd.weaponSwitch != Weapon_None && GS_CanEquip( ps, cmd.weaponSwitch ) ) {
		ps->pending_weapon = cmd.weaponSwitch;
	}

	if( ps->pmove.no_shooting_time > 0 ) {
		cmd.buttons = cmd.buttons & ~Button_Attack1;
		cmd.buttons = cmd.buttons & ~Button_Attack2;
		cmd.buttons = cmd.buttons & ~Button_Gadget;
	}

	if( HasAllBits( cmd.buttons, Button_Gadget ) && HasAllBits( cmd.down_edges, Button_Gadget ) ) {
		ps->pending_gadget = true;
	}

	HandleZoom( gs, ps, &cmd );

	while( true ) {
		Span< const ItemState > sm = FindItemStateMachine( ps );

		WeaponState old_state = ps->weapon_state;
		const ItemState * s = FindState( sm, old_state );

		ItemStateTransition transition = s == NULL ? ItemStateTransition( sm[ 0 ].state ) : s->think( gs, old_state, ps, &cmd );

		switch( transition.type ) {
			case TransitionType_Normal:
				if( old_state == transition.state )
					return;
				ps->weapon_state = transition.state;
				ps->weapon_state_time = 0;
				break;

			case TransitionType_NoReset:
				if( old_state == transition.state )
					return;
				ps->weapon_state = transition.state;
				break;

			case TransitionType_ForceReset:
				ps->weapon_state = transition.state;
				ps->weapon_state_time = 0;
				break;
		}
	}
}

bool GS_CanEquip( const SyncPlayerState * ps, WeaponType weapon ) {
	return FindWeapon( ps, weapon ) != NULL;
}

Optional< Loadout > ParseLoadout( Span< const char > loadout_string ) {
	Loadout loadout = { };

	for( size_t i = 0; i < WeaponCategory_Count; i++ ) {
		Span< const char > token = ParseToken( &loadout_string, Parse_DontStopOnNewLine );
		Optional< s64 > weapon = SpanToSigned( token, Weapon_None + 1, Weapon_Count - 1 );
		if( !weapon.exists )
			return NONE;

		WeaponCategory category = GetWeaponDefProperties( WeaponType( weapon.value ) )->category;
		if( category != i )
			return NONE;

		loadout.weapons[ category ] = WeaponType( weapon.value );
	}

	{
		Span< const char > token = ParseToken( &loadout_string, Parse_DontStopOnNewLine );
		Optional< s64 > perk = SpanToSigned( token, Perk_None + 1, Perk_Count - 1 );
		if( !perk.exists )
			return NONE;
		if( GetPerkDef( PerkType( perk.value ) )->disabled )
			return NONE;
		loadout.perk = PerkType( perk.value );
	}

	{
		Span< const char > token = ParseToken( &loadout_string, Parse_DontStopOnNewLine );
		Optional< s64 > gadget = SpanToSigned( token, Gadget_None + 1, Gadget_Count - 1 );
		if( !gadget.exists )
			return NONE;
		loadout.gadget = GadgetType( gadget.value );
	}

	return loadout_string.n == 0 ? MakeOptional( loadout ) : NONE;
}

void format( FormatBuffer * fb, const Loadout & loadout, const FormatOpts & opts ) {
	for( u32 i = 0; i < WeaponCategory_Count; i++ ) {
		format( fb, loadout.weapons[ i ], FormatOpts() );
		format( fb, " " );
	}
	format( fb, loadout.perk, FormatOpts() );
	format( fb, " " );
	format( fb, loadout.gadget, FormatOpts() );
}
