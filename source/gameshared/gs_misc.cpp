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
#include "gameshared/gs_public.h"

Vec3 GS_EvaluateJumppad( const SyncEntityState * jumppad, Vec3 velocity ) {
	if( jumppad->type == ET_PAINKILLER_JUMPPAD ) {
		Vec3 push = jumppad->origin2;

		Vec3 parallel = Project( velocity, push );
		Vec3 perpendicular = velocity - parallel;

		return perpendicular + push;
	}

	return jumppad->origin2;
}

void GS_TouchPushTrigger( const gs_state_t * gs, SyncPlayerState * playerState, const SyncEntityState * pusher ) {
	// spectators don't use jump pads
	if( playerState->pmove.pm_type != PM_NORMAL ) {
		return;
	}

	playerState->pmove.velocity = GS_EvaluateJumppad( pusher, playerState->pmove.velocity );
	playerState->pmove.no_friction_time = 500;

	// reset walljump counter
	gs->api.PredictedEvent( playerState->POVnum, EV_JUMP_PAD, 0 );
}


static constexpr u8 DamageType_GadgetOffset = u8( Weapon_Count );
static constexpr u8 DamageType_WorldOffset = u8( Gadget_Count ) + DamageType_GadgetOffset;

DamageType::DamageType( WeaponType weapon ) {
	encoded = u8( weapon );
	altfire = u8( false );
}

DamageType::DamageType( WeaponType weapon, bool altfire = false ) {
	encoded = u8( weapon );
	altfire = u8( altfire );
}

DamageType::DamageType( GadgetType gadget ) {
	encoded = u8( gadget ) + DamageType_GadgetOffset;
	altfire = u8( 0 );
}

DamageType::DamageType( WorldDamage world ) {
	encoded = u8( world ) + DamageType_WorldOffset;
	altfire = u8( 0 );
}

bool DamageType::operator==( DamageType d ) { return encoded == d.encoded; }
bool DamageType::operator==( WeaponType w ) { return encoded == w; }
bool DamageType::operator==( GadgetType g ) { return encoded == ( g + DamageType_GadgetOffset ); }
bool DamageType::operator==( WorldDamage w ) { return encoded == ( w + DamageType_WorldOffset ); }
bool DamageType::operator!=( auto g ) { return !( operator==( g ) ); }


DamageCategory DecodeDamageType( DamageType type, WeaponType * weapon, GadgetType * gadget, WorldDamage * world ) {
	if( type.encoded < Weapon_Count ) {
		if( weapon != NULL ) {
			*weapon = WeaponType( type.encoded );
		}
		return type.altfire ? DamageCategory_WeaponAlt : DamageCategory_Weapon;
	}

	if( type.encoded < u8( Weapon_Count ) + u8( Gadget_Count ) ) {
		if( gadget != NULL ) {
			*gadget = GadgetType( type.encoded - Weapon_Count );
		}
		return DamageCategory_Gadget;
	}

	if( world != NULL ) {
		*world = WorldDamage( type.encoded - Weapon_Count - Gadget_Count );
	}
	return DamageCategory_World;
}
