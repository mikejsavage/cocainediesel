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
	Vec3 end = start + dir * range + right * spread.x + up * spread.y;

	gs->api.Trace( trace, start, Vec3( 0.0f ), Vec3( 0.0f ), end, ignore, MASK_WALLBANG, timeDelta );

	if( wallbang_trace != NULL ) {
		gs->api.Trace( wallbang_trace, start, Vec3( 0.0f ), Vec3( 0.0f ), trace->endpos, ignore, MASK_SHOT, timeDelta );
	}
}

Vec2 RandomSpreadPattern( u16 entropy, float spread ) {
	RNG rng = NewRNG( entropy, 0 );
	return spread * UniformSampleInsideCircle( &rng );
}

float ZoomSpreadness( s16 zoom_time, const WeaponDef * def ) {
	float frac = 1.0f - float( zoom_time ) / float( ZOOMTIME );
	return frac * def->range * atanf( Radians( def->zoom_spread ) );
}

Vec2 FixedSpreadPattern( int i, float spread ) {
	float x = i * 2.4f;
	return Vec2( cosf( x ), sinf( x ) ) * spread * sqrtf( x );
}

void GS_TraceLaserBeam( const gs_state_t * gs, trace_t * trace, Vec3 origin, Vec3 angles, float range, int ignore, int timeDelta, void ( *impact )( const trace_t * trace, Vec3 dir, void * data ), void * data ) {
	Vec3 maxs = Vec3( 0.5f, 0.5f, 0.5f );

	Vec3 dir;
	AngleVectors( angles, &dir, NULL, NULL );
	Vec3 end = origin + dir * range;

	trace->ent = 0;

	gs->api.Trace( trace, origin, -maxs, maxs, end, ignore, MASK_SHOT, timeDelta );
	if( trace->ent != -1 && impact != NULL ) {
		impact( trace, dir, data );
	}
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

static bool HasAmmo( const WeaponDef * def, const WeaponSlot * slot ) {
	return def->clip_size == 0 || slot->ammo > 0;
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

	ItemState( WeaponState s, ItemStateThinkCallback t ) {
		state = s;
		think = t;
	}
};

static ItemStateTransition GenericDelay( WeaponState state, SyncPlayerState * ps, s64 delay, ItemStateTransition next ) {
	return ps->weapon_state_time >= delay ? next : state;
}

template< size_t N >
constexpr static Span< const ItemState > MakeStateMachine( const ItemState ( &states )[ N ] ) {
	return Span< const ItemState >( states, N );
}

static void HandleZoom( const gs_state_t * gs, SyncPlayerState * ps, const UserCommand * cmd ) {
	const WeaponDef * def = GS_GetWeaponDef( ps->weapon );
	const WeaponSlot * slot = GetSelectedWeapon( ps );

	s16 last_zoom_time = ps->zoom_time;
	bool can_zoom = ( ps->weapon_state == WeaponState_Idle || ( ps->weapon_state == WeaponState_Firing && HasAmmo( def, slot ) ) ) && ( ps->pmove.features & PMFEAT_SCOPE );

	if( can_zoom && def->zoom_fov != 0 && ( cmd->buttons & BUTTON_SPECIAL ) != 0 ) {
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

static void FireWeapon( const gs_state_t * gs, SyncPlayerState * ps, const UserCommand * cmd, bool smooth ) {
	const WeaponDef * def = GS_GetWeaponDef( ps->weapon );
	WeaponSlot * slot = GetSelectedWeapon( ps );

	if( smooth ) {
		gs->api.PredictedEvent( ps->POVnum, EV_SMOOTHREFIREWEAPON, ps->weapon );
	}
	else {
		u64 parm = ps->weapon | ( cmd->entropy << 8 ) | ( u64( ps->zoom_time ) << 24 ) ;
		gs->api.PredictedFireWeapon( ps->POVnum, parm );
	}

	if( def->clip_size > 0 ) {
		slot->ammo--;
		if( slot->ammo == 0 ) {
			gs->api.PredictedEvent( ps->POVnum, EV_NOAMMOCLICK, 0 );
		}
	}
}

static ItemStateTransition Dispatch( const gs_state_t * gs, WeaponState state, SyncPlayerState * ps, const UserCommand * cmd ) {
	if( ps->pending_weapon == Weapon_None && !ps->pending_gadget ) {
		return state;
	}

	if( ps->pending_gadget ) {
		ps->using_gadget = true;
	}
	else {
		ps->weapon = ps->pending_weapon;
	}

	ps->pending_weapon = Weapon_None;
	ps->pending_gadget = false;

	u64 quiet_bit = state == WeaponState_DispatchQuiet ? 1 : 0;
	gs->api.PredictedEvent( ps->POVnum, EV_WEAPONACTIVATE, ( ps->weapon << 1 ) | quiet_bit );

	return WeaponState_SwitchingIn;
}

static ItemState dispatch_states[] = {
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

static ItemState generic_gun_switching_in_state =
	ItemState( WeaponState_SwitchingIn, []( const gs_state_t * gs, WeaponState state, SyncPlayerState * ps, const UserCommand * cmd ) -> ItemStateTransition {
		const WeaponDef * def = GS_GetWeaponDef( ps->weapon );
		return AllowWeaponSwitch( gs, ps, GenericDelay( state, ps, def->switch_in_time, WeaponState_Idle ) );
	} );

static ItemState generic_gun_switching_out_state =
	ItemState( WeaponState_SwitchingOut, []( const gs_state_t * gs, WeaponState state, SyncPlayerState * ps, const UserCommand * cmd ) -> ItemStateTransition {
		const WeaponDef * def = GS_GetWeaponDef( ps->weapon );
		if( ps->weapon_state_time >= def->switch_out_time ) {
			ps->last_weapon = ps->weapon;
			ps->weapon = Weapon_None;
			return WeaponState_Dispatch;
		}

		return state;
	} );

static ItemState generic_gun_refire_state =
	ItemState( WeaponState_Firing, []( const gs_state_t * gs, WeaponState state, SyncPlayerState * ps, const UserCommand * cmd ) -> ItemStateTransition {
		const WeaponDef * def = GS_GetWeaponDef( ps->weapon );
		return GenericDelay( state, ps, def->refire_time, AllowWeaponSwitch( gs, ps, WeaponState_Idle ) );
	} );

static ItemState generic_gun_states[] = {
	generic_gun_switching_in_state,
	generic_gun_switching_out_state,
	generic_gun_refire_state,

	ItemState( WeaponState_Idle, []( const gs_state_t * gs, WeaponState state, SyncPlayerState * ps, const UserCommand * cmd ) -> ItemStateTransition {
		const WeaponDef * def = GS_GetWeaponDef( ps->weapon );
		WeaponSlot * slot = GetSelectedWeapon( ps );

		if( cmd->buttons & BUTTON_ATTACK ) {
			if( HasAmmo( def, slot ) ) {
				FireWeapon( gs, ps, cmd, false );

				switch( def->firing_mode ) {
					case FiringMode_Auto: return WeaponState_Firing;
					case FiringMode_SemiAuto: return WeaponState_FiringSemiAuto;
					case FiringMode_Smooth: return WeaponState_FiringSmooth;
					case FiringMode_Clip: return WeaponState_FiringEntireClip;
				}
			}
		}

		bool wants_reload = ( cmd->buttons & BUTTON_RELOAD ) && def->clip_size != 0 && slot->ammo < def->clip_size;
		if( wants_reload || !HasAmmo( def, slot ) ) {
			return WeaponState_Reloading;
		}

		return AllowWeaponSwitch( gs, ps, WeaponState_Idle );
	} ),

	ItemState( WeaponState_FiringSemiAuto, []( const gs_state_t * gs, WeaponState state, SyncPlayerState * ps, const UserCommand * cmd ) -> ItemStateTransition {
		if( cmd->buttons & BUTTON_ATTACK ) {
			const WeaponDef * def = GS_GetWeaponDef( ps->weapon );
			ps->weapon_state_time = Min2( def->refire_time, ps->weapon_state_time );

			if( ps->weapon_state_time >= def->refire_time ) {
				return AllowWeaponSwitch( gs, ps, state );
			}

			return state;
		}

		return NoReset( WeaponState_Firing );
	} ),

	ItemState( WeaponState_FiringSmooth, []( const gs_state_t * gs, WeaponState state, SyncPlayerState * ps, const UserCommand * cmd ) -> ItemStateTransition {
		const WeaponDef * def = GS_GetWeaponDef( ps->weapon );
		if( ps->weapon_state_time < def->refire_time ) {
			return state;
		}

		WeaponSlot * slot = GetSelectedWeapon( ps );

		if( ( cmd->buttons & BUTTON_ATTACK ) == 0 || slot->ammo == 0 ) {
			return WeaponState_Idle;
		}

		FireWeapon( gs, ps, cmd, true );

		return AllowWeaponSwitch( gs, ps, ForceReset( state ) );
	} ),

	ItemState( WeaponState_FiringEntireClip, []( const gs_state_t * gs, WeaponState state, SyncPlayerState * ps, const UserCommand * cmd ) -> ItemStateTransition {
		const WeaponDef * def = GS_GetWeaponDef( ps->weapon );
		if( ps->weapon_state_time < def->refire_time ) {
			return state;
		}

		WeaponSlot * slot = GetSelectedWeapon( ps );
		if( slot->ammo == 0 ) {
			return WeaponState_Idle;
		}

		FireWeapon( gs, ps, cmd, false );

		return ForceReset( state );
	} ),

	ItemState( WeaponState_Reloading, []( const gs_state_t * gs, WeaponState state, SyncPlayerState * ps, const UserCommand * cmd ) -> ItemStateTransition {
		if( ps->weapon_state_time == 0 ) {
			gs->api.PredictedEvent( ps->POVnum, EV_RELOAD, ps->weapon );
		}

		const WeaponDef * def = GS_GetWeaponDef( ps->weapon );
		WeaponSlot * slot = GetSelectedWeapon( ps );

		if( ps->weapon_state_time < def->reload_time ) {
			if( def->staged_reload_time == 0 ) {
				slot->ammo = 0;
			}
			return AllowWeaponSwitch( gs, ps, state );
		}

		if( def->staged_reload_time != 0 ) {
			ps->weapon_state_time = def->staged_reload_time;
			return NoReset( WeaponState_StagedReloading );
		}

		slot->ammo = def->clip_size;

		return WeaponState_Idle;
	} ),

	ItemState( WeaponState_StagedReloading, []( const gs_state_t * gs, WeaponState state, SyncPlayerState * ps, const UserCommand * cmd ) -> ItemStateTransition {
		if( ps->weapon_state_time == 0 ) {
			gs->api.PredictedEvent( ps->POVnum, EV_RELOAD, ps->weapon );
		}

		const WeaponDef * def = GS_GetWeaponDef( ps->weapon );
		WeaponSlot * slot = GetSelectedWeapon( ps );

		if( ( cmd->buttons & BUTTON_ATTACK ) != 0 && HasAmmo( def, slot ) ) {
			return WeaponState_Idle;
		}

		if( ps->weapon_state_time < def->staged_reload_time ) {
			return AllowWeaponSwitch( gs, ps, state );
		}

		slot->ammo++;

		return slot->ammo == def->clip_size ? WeaponState_Idle : ForceReset( WeaponState_StagedReloading );
	} ),
};

static ItemState railgun_states[] = {
	generic_gun_switching_in_state,
	generic_gun_switching_out_state,
	generic_gun_refire_state,

	ItemState( WeaponState_Idle, []( const gs_state_t * gs, WeaponState state, SyncPlayerState * ps, const UserCommand * cmd ) -> ItemStateTransition {
		if( cmd->buttons & BUTTON_ATTACK ) {
			return WeaponState_Cooking;
		}

		return AllowWeaponSwitch( gs, ps, WeaponState_Idle );
	} ),

	ItemState( WeaponState_Cooking, []( const gs_state_t * gs, WeaponState state, SyncPlayerState * ps, const UserCommand * cmd ) -> ItemStateTransition {
		const WeaponDef * def = GS_GetWeaponDef( Weapon_Railgun );
		if( ( cmd->buttons & BUTTON_ATTACK ) == 0 && ps->weapon_state_time >= def->reload_time ) {
			gs->api.PredictedFireWeapon( ps->POVnum, Weapon_Railgun );
			return WeaponState_Firing;
		}

		ps->weapon_state_time = Min2( def->reload_time, ps->weapon_state_time );

		return state;
	} ),
};

static const ItemState generic_throwable_states[] = {
	ItemState( WeaponState_SwitchingIn, []( const gs_state_t * gs, WeaponState state, SyncPlayerState * ps, const UserCommand * cmd ) -> ItemStateTransition {
		const GadgetDef * def = GetGadgetDef( ps->gadget );
		return AllowWeaponSwitch( gs, ps, GenericDelay( state, ps, def->switch_in_time, WeaponState_Cooking ) );
	} ),

	ItemState( WeaponState_Cooking, []( const gs_state_t * gs, WeaponState state, SyncPlayerState * ps, const UserCommand * cmd ) -> ItemStateTransition {
		if( ( cmd->buttons & BUTTON_GADGET ) == 0 ) {
			gs->api.PredictedUseGadget( ps->POVnum, ps->gadget, ps->weapon_state_time );
			ps->gadget_ammo--;
			return WeaponState_Throwing;
		}

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

static bool SuicideBombStage( SyncPlayerState * ps, int stage, u64 delay ) {
	if( ps->gadget_ammo >= stage )
		return false;

	if( ps->weapon_state_time < delay )
		return false;

	ps->gadget_ammo = stage;
	return true;
}

static const ItemState suicide_bomb_states[] = {
	ItemState( WeaponState_Firing, []( const gs_state_t * gs, WeaponState state, SyncPlayerState * ps, const UserCommand * cmd ) -> ItemStateTransition {
		if( SuicideBombStage( ps, 2, 0 ) ) {
			// TODO: randomise
			gs->api.PredictedEvent( ps->POVnum, EV_SUICIDE_BOMB_ANNOUNCEMENT, 0 );
		}

		u64 beep_interval = 1024;
		u64 beep_delay = beep_interval;
		constexpr int num_beeps = 10;

		for( int beep = 3; beep < 3 + num_beeps; beep++ ) {
			if( SuicideBombStage( ps, beep, beep_delay ) ) {
				gs->api.PredictedEvent( ps->POVnum, EV_SUICIDE_BOMB_BEEP, 0 );
			}

			beep_interval = Max2( u64( 128 ), beep_interval / 2 );
			beep_delay += beep_interval;
		}

		if( SuicideBombStage( ps, 100, 2500 ) ) {
			gs->api.PredictedEvent( ps->POVnum, EV_SUICIDE_BOMB_EXPLODE, 0 );
		}

		return state;
	} ),
};

constexpr static Span< const ItemState > dispatch_state_machine = MakeStateMachine( dispatch_states );
constexpr static Span< const ItemState > generic_gun_state_machine = MakeStateMachine( generic_gun_states );
constexpr static Span< const ItemState > railgun_state_machine = MakeStateMachine( railgun_states );
constexpr static Span< const ItemState > generic_throwable_state_machine = MakeStateMachine( generic_throwable_states );
constexpr static Span< const ItemState > suicide_bomb_state_machine = MakeStateMachine( suicide_bomb_states );

static Span< const ItemState > FindItemStateMachine( SyncPlayerState * ps ) {
	if( ps->weapon == Weapon_None && !ps->using_gadget ) {
		return dispatch_state_machine;
	}

	if( ps->using_gadget ) {
		switch( ps->gadget ) {
			// case Gadget_FragGrenade:
			// 	return generic_throwable_state_machine;
			case Gadget_ThrowingAxe:
			case Gadget_StunGrenade:
				return generic_throwable_state_machine;

			case Gadget_SuicideBomb:
				return suicide_bomb_state_machine;
		}
	}

	if( ps->weapon == Weapon_Railgun ) {
		return railgun_state_machine;
	}

	return generic_gun_state_machine;
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
	if( GS_MatchPaused( gs ) ) {
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
		cmd.buttons = cmd.buttons & ~BUTTON_ATTACK;
		cmd.buttons = cmd.buttons & ~BUTTON_GADGET;
	}

	if( ( cmd.buttons & BUTTON_GADGET ) != 0 && ( cmd.down_edges & BUTTON_GADGET ) != 0 ) {
		ps->pending_gadget = true;
	}

	HandleZoom( gs, ps, &cmd );
	ps->flashed -= Min2( ps->flashed, u16( cmd.msec * 0.001f * U16_MAX / 3.0f ) );

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
	return ( ps->pmove.features & PMFEAT_WEAPONSWITCH ) != 0 && FindWeapon( ps, weapon ) != NULL;
}
