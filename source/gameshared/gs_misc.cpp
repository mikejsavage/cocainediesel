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

#include "qcommon/qcommon.h"
#include "gameshared/movement.h"

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

	// reset walljump counter
	gs->api.PredictedEvent( playerState->POVnum, EV_JUMP_PAD, 0 );
}

int GS_WaterLevel( const gs_state_t * gs, SyncEntityState *state, Vec3 mins, Vec3 maxs ) {
	int waterlevel = 0;

	Vec3 point = state->origin;
	point.z += mins.z + 1;

	int cont = gs->api.PointContents( point, 0 );
	if( cont & MASK_WATER ) {
		waterlevel = 1;
		point.z += 26;
		cont = gs->api.PointContents( point, 0 );
		if( cont & MASK_WATER ) {
			waterlevel = 2;
			point.z += 22;
			cont = gs->api.PointContents( point, 0 );
			if( cont & MASK_WATER ) {
				waterlevel = 3;
			}
		}
	}

	return waterlevel;
}

DamageType::DamageType( WeaponType weapon ) {
	encoded = weapon;
}

DamageType::DamageType( GadgetType gadget ) {
	encoded = gadget + Weapon_Count;
}

DamageType::DamageType( WorldDamage world ) {
	encoded = world + Weapon_Count + Gadget_Count;
}

bool operator==( DamageType a, DamageType b ) {
	return a.encoded == b.encoded;
}

bool operator!=( DamageType a, DamageType b ) {
	return !( a == b );
}

DamageCategory DecodeDamageType( DamageType type, WeaponType * weapon, GadgetType * gadget, WorldDamage * world ) {
	if( type.encoded < Weapon_Count ) {
		if( weapon != NULL ) {
			*weapon = WeaponType( type.encoded );
		}
		return DamageCategory_Weapon;
	}

	if( type.encoded < Weapon_Count + Gadget_Count ) {
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
