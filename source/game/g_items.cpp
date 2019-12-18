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
#include "g_local.h"

bool Add_Ammo( gclient_t *client, const gsitem_t *item, int count, bool add_it ) {
	int max = 255;

	if( !client || !item ) {
		return false;
	}

	if( (int)client->ps.inventory[item->tag] >= max ) {
		return false;
	}

	if( add_it ) {
		client->ps.inventory[item->tag] += count;

		if( (int)client->ps.inventory[item->tag] > max ) {
			client->ps.inventory[item->tag] = max;
		}
	}

	return true;
}

bool G_PickupItem( edict_t *other, const gsitem_t *it, int flags, int count ) {
	if( other->r.client && G_ISGHOSTING( other ) ) {
		return false;
	}

	if( it->type & IT_WEAPON ) {
		other->r.client->ps.inventory[it->tag] = 1;
		return true;
	}

	return false;
}

void G_UseItem( edict_t *ent, const gsitem_t *it ) {
	if( it != NULL && it->type & IT_WEAPON ) {
		Use_Weapon( ent, it );
	}
}

/*
* PrecacheItem
*
* Precaches all data needed for a given item.
* This will be called for each item spawned in a level,
* and for each item in each client's inventory.
*/
void PrecacheItem( const gsitem_t *it ) {
	const char *s, *start;
	char data[MAX_QPATH];

	if( !it ) {
		return;
	}

	// parse everything for its ammo
	if( it->ammo_tag ) {
		const gsitem_t * ammo = GS_FindItemByTag( it->ammo_tag );
		if( ammo != it ) {
			PrecacheItem( ammo );
		}
	}

	// parse the space separated precache string for other items
	for( int i = 0; i < 3; i++ ) {
		if( i == 0 ) {
			s = it->precache_models;
		} else if( i == 1 ) {
			s = it->precache_sounds;
		} else {
			s = it->precache_images;
		}

		if( !s || !s[0] ) {
			continue;
		}

		while( *s ) {
			start = s;
			while( *s && *s != ' ' )
				s++;

			int len = s - start;
			if( len >= MAX_QPATH || len < 5 ) {
				G_Error( "PrecacheItem: %d has bad precache string", it->tag );
				return;
			}
			memcpy( data, start, len );
			data[len] = 0;
			if( *s ) {
				s++;
			}

			if( i == 0 ) {
				trap_ModelIndex( data );
			} else if( i == 1 ) {
				trap_SoundIndex( data );
			} else {
				trap_ImageIndex( data );
			}
		}
	}
}

void G_PrecacheItems() {
	// precache item names and weapondefs
	for( int i = 1; i < GS_MAX_ITEM_TAGS; i++ ) {
		const gsitem_t * item = GS_FindItemByTag( i );
		if( !item )
			break;

		if( item->type & IT_WEAPON && GS_GetWeaponDef( item->tag ) ) {
			G_PrecacheWeapondef( i, &GS_GetWeaponDef( item->tag )->firedef );
		}
	}

	// precache items
	for( int i = WEAP_GUNBLADE; i < WEAP_TOTAL; i++ ) {
		const gsitem_t * item = GS_FindItemByTag( i );
		PrecacheItem( item );
	}
}
