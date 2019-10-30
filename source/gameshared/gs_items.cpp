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

// gs_items.c	-	game shared items definitions

#include "q_arch.h"
#include "q_math.h"
#include "q_shared.h"
#include "q_comref.h"
#include "q_collision.h"
#include "gs_public.h"

/*
*
* ITEM DEFS
*
*/

const gsitem_t itemdefs[] = {
	{ }, // weapons start from 1

	{
		WEAP_GUNBLADE, // item tag, weapon model for weapons
		IT_WEAPON,

		"Knife", "gb",
		RGB8( 255, 255, 255 ),
		"Knife people in the face",
		0,

		AMMO_GUNBLADE,
		NULL, NULL, NULL
	},

	{
		WEAP_MACHINEGUN,
		IT_WEAPON,

		"AK-69", "mg",
		RGB8( 254, 235, 98 ),
		"Shoots fast direct bullets touching enemies at any range",
		100,

		AMMO_BULLETS,
		NULL, NULL, NULL
	},

	{
		WEAP_RIOTGUN,
		IT_WEAPON,

		"Shotgun", "rg",
		RGB8( 255, 172, 30 ),
		"Basically a shotgun",
		100,

		AMMO_SHELLS,
		NULL, NULL, NULL
	},

	{
		WEAP_GRENADELAUNCHER,
		IT_WEAPON,

		"Grenades", "gl",
		RGB8( 62, 141, 255 ),
		"Deprecated gun, enjoy it while it lasts nerds",
		100,

		AMMO_GRENADES,
		PATH_GRENADE_MODEL,
		NULL, NULL
	},

	{
		WEAP_ROCKETLAUNCHER,
		IT_WEAPON,

		"Rockets", "rl",
		RGB8( 255, 58, 66 ),
		"Shoots slow moving rockets that deal damage in an area and push bodies away",
		200,

		AMMO_ROCKETS,
		PATH_ROCKET_MODEL,
		S_WEAPON_ROCKET_FLY,
		NULL
	},

	{
		WEAP_PLASMAGUN,
		IT_WEAPON,

		"Plasma", "pg",
		RGB8( 172, 80, 255 ),
		"Shoots fast projectiles that deal damage in an area",
		100,

		AMMO_PLASMA,
		PATH_PLASMA_MODEL,
		S_WEAPON_PLASMAGUN_FLY,
		NULL
	},

	{
		WEAP_LASERGUN,
		IT_WEAPON,

		"Laser", "lg",
		RGB8( 82, 252, 95 ),
		"Shoots a continuous trail doing quick but low damage at a certain range",
		200,

		AMMO_LASERS,
		NULL,
		S_WEAPON_LASERGUN_HUM " " S_WEAPON_LASERGUN_STOP " " S_WEAPON_LASERGUN_HIT,
		NULL
	},

	{
		WEAP_ELECTROBOLT,
		IT_WEAPON,

		"Railgun", "eb",
		RGB8( 80, 243, 255 ),
		"Shoots a direct laser hit doing pretty high damage",
		200,

		AMMO_BOLTS,
		NULL,
		S_WEAPON_ELECTROBOLT_HIT,
		NULL
	},

	//------------------------
	// AMMO ITEMS
	//------------------------

	{ AMMO_GUNBLADE },
	{ AMMO_BULLETS },
	{ AMMO_SHELLS },
	{ AMMO_GRENADES },
	{ AMMO_ROCKETS },
	{ AMMO_PLASMA },
	{ AMMO_LASERS },
	{ AMMO_BOLTS },

	// end of list marker
	{ },
};

// +1 for the { } at the end
STATIC_ASSERT( ARRAY_COUNT( itemdefs ) == GS_MAX_ITEM_TAGS + 1 );

//====================================================================

/*
* GS_FindItemByTag
*/
const gsitem_t *GS_FindItemByTag( const int tag ) {
	assert( tag >= 0 );
	assert( tag < GS_MAX_ITEM_TAGS );
	assert( tag == itemdefs[ tag ].tag );
	return &itemdefs[ tag ];
}

/*
* GS_FindItemByName
*/
const gsitem_t *GS_FindItemByName( const char *name ) {
	if( !name ) {
		return NULL;
	}

	for( const gsitem_t * it = &itemdefs[1]; it->tag != 0; it++ ) {
		if( it->name != NULL && !Q_stricmp( name, it->name ) )
			return it;
		if( it->shortname != NULL && !Q_stricmp( name, it->shortname ) )
			return it;
	}

	return NULL;
}

/*
* GS_Cmd_UseItem
*/
const gsitem_t *GS_Cmd_UseItem( const gs_state_t * gs, player_state_t *playerState, const char *string, int typeMask ) {
	const gsitem_t *item = NULL;

	assert( playerState );

	if( playerState->pmove.pm_type >= PM_SPECTATOR ) {
		return NULL;
	}

	if( !string || !string[0] ) {
		return NULL;
	}

	if( Q_isdigit( string ) ) {
		int tag = atoi( string );
		item = GS_FindItemByTag( tag );
	} else {
		item = GS_FindItemByName( string );
	}

	if( !item ) {
		return NULL;
	}

	if( typeMask && !( item->type & typeMask ) ) {
		return NULL;
	}

	// we don't have this item in the inventory
	if( !playerState->inventory[item->tag] ) {
		if( gs->module == GS_MODULE_CGAME && !( item->type & IT_WEAPON ) ) {
			gs->api.Printf( "Item %s is not in inventory\n", item->name );
		}
		return NULL;
	}

	// see if we can use it

	if( item->type & IT_WEAPON ) {
		if( !( playerState->pmove.stats[PM_STAT_FEATURES] & PMFEAT_WEAPONSWITCH ) ) {
			return NULL;
		}

		if( item->tag == playerState->stats[STAT_PENDING_WEAPON] ) { // it's already being loaded
			return NULL;
		}

		// check for need of any kind of ammo/fuel/whatever
		if( item->ammo_tag != AMMO_NONE ) {
			gs_weapon_definition_t *weapondef = GS_GetWeaponDef( item->tag );

			if( weapondef ) {
				// do we have any of these ammos ?
				if( playerState->inventory[item->ammo_tag] >= weapondef->firedef.usage_count ) {
					return item;
				}
			}

			return NULL;
		}

		return item; // one of the weapon modes doesn't require ammo to be fired
	}

	return NULL;
}

/*
* GS_Cmd_UseWeaponStep_f
*/
static const gsitem_t *GS_Cmd_UseWeaponStep_f( const gs_state_t * gs, player_state_t *playerState, int step, int predictedWeaponSwitch ) {
	const gsitem_t *item;
	int curSlot, newSlot;

	assert( playerState );

	if( playerState->pmove.pm_type >= PM_SPECTATOR ) {
		return NULL;
	}

	if( !( playerState->pmove.stats[PM_STAT_FEATURES] & PMFEAT_WEAPONSWITCH ) ) {
		return NULL;
	}

	if( step != -1 && step != 1 ) {
		step = 1;
	}

	if( predictedWeaponSwitch && predictedWeaponSwitch != playerState->stats[STAT_PENDING_WEAPON] ) {
		curSlot = predictedWeaponSwitch;
	} else {
		curSlot = playerState->stats[STAT_PENDING_WEAPON];
	}

	curSlot = Clamp( 0, curSlot, WEAP_TOTAL - 1 );
	newSlot = curSlot;
	do {
		newSlot += step;
		if( newSlot >= WEAP_TOTAL ) {
			newSlot = 0;
		}
		if( newSlot < 0 ) {
			newSlot = WEAP_TOTAL - 1;
		}

		if( ( item = GS_Cmd_UseItem( gs, playerState, va( "%i", newSlot ), IT_WEAPON ) ) != NULL ) {
			return item;
		}
	} while( newSlot != curSlot );

	return NULL;
}

/*
* GS_Cmd_NextWeapon_f
*/
const gsitem_t *GS_Cmd_NextWeapon_f( const gs_state_t * gs, player_state_t *playerState, int predictedWeaponSwitch ) {
	return GS_Cmd_UseWeaponStep_f( gs, playerState, 1, predictedWeaponSwitch );
}

/*
* GS_Cmd_PrevWeapon_f
*/
const gsitem_t *GS_Cmd_PrevWeapon_f( const gs_state_t * gs, player_state_t *playerState, int predictedWeaponSwitch ) {
	return GS_Cmd_UseWeaponStep_f( gs, playerState, -1, predictedWeaponSwitch );
}
