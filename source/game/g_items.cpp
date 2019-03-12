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

//======================================================================

void DoRespawn( edict_t *ent ) {
	if( ent->team ) {
		edict_t *master;
		int count;
		int choice;

		master = ent->teammaster;

		assert( master != NULL );

		if( master ) {
			for( count = 0, ent = master; ent; ent = ent->chain, count++ ) ;

			choice = rand() % count;

			for( count = 0, ent = master; count < choice; ent = ent->chain, count++ ) ;
		}
	}

	ent->r.solid = SOLID_TRIGGER;
	ent->r.svflags &= ~SVF_NOCLIENT;
	ent->s.effects &= ~EF_GHOST;

	GClip_LinkEntity( ent );

	// send an effect
	G_AddEvent( ent, EV_ITEM_RESPAWN, ent->item ? ent->item->tag : 0, true );
}

void SetRespawn( edict_t *ent, int delay ) {
	if( !ent->item ) {
		return;
	}

	if( delay < 0 ) {
		G_FreeEdict( ent );
		return;
	}

	ent->r.solid = SOLID_NOT;
	ent->nextThink = level.time + delay;
	ent->think = DoRespawn;
	if( GS_MatchState() == MATCH_STATE_WARMUP ) {
		ent->s.effects |= EF_GHOST;
	} else {
		ent->r.svflags |= SVF_NOCLIENT;
	}

	GClip_LinkEntity( ent );
}

void G_Items_RespawnByType( unsigned int typeMask, int item_tag, float delay ) {
	edict_t *ent;
	int msecs;

	for( ent = game.edicts + gs.maxclients + BODY_QUEUE_SIZE; ENTNUM( ent ) < game.maxentities; ent++ ) {
		if( !ent->r.inuse || !ent->item ) {
			continue;
		}

		if( typeMask && !( ent->item->type & typeMask ) ) {
			continue;
		}

		if( ent->spawnflags & DROPPED_ITEM ) {
			G_FreeEdict( ent );
			continue;
		}

		if( !G_Gametype_CanRespawnItem( ent->item ) ) {
			continue;
		}

		// if a tag is specified, ignore others of the same type
		if( item_tag > 0 && ( ent->item->tag != item_tag ) ) {
			continue;
		}

		msecs = (int)( delay * 1000 );
		if( msecs >= 0 ) {
			clamp_low( msecs, 1 );
		}

		SetRespawn( ent, msecs );
	}
}

//======================================================================

static bool Pickup_Powerup( edict_t *other, const gsitem_t *item, int flags, int count ) {
	if( !item || !item->tag ) {
		return false;
	}

	if( item->quantity ) {
		int timeout;

		if( flags & DROPPED_ITEM ) {
			timeout = count + 1;
		} else {
			timeout = item->quantity + 1;
		}

		other->r.client->ps.inventory[item->tag] += timeout;
	} else {
		other->r.client->ps.inventory[item->tag]++;
	}

	return true;
}

//======================================================================

bool Add_Ammo( gclient_t *client, const gsitem_t *item, int count, bool add_it ) {
	int max;

	if( !client || !item ) {
		return false;
	}

	max = item->inventory_max;

	if( max <= 0 ) {
		max = 255;
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

//======================================================================

void Touch_ItemSound( edict_t *other, const gsitem_t *item ) {
	if( item->pickup_sound ) {
		if( item->type & IT_POWERUP ) {
			G_Sound( other, CHAN_ITEM, trap_SoundIndex( item->pickup_sound ), ATTN_NORM );
		} else {
			G_Sound( other, CHAN_AUTO, trap_SoundIndex( item->pickup_sound ), ATTN_NORM );
		}
	}
}

/*
* Touch_Item
*/
void Touch_Item( edict_t *ent, edict_t *other, cplane_t *plane, int surfFlags ) {
	bool taken;
	const gsitem_t *item = ent->item;

	if( !other->r.client || G_ISGHOSTING( other ) ) {
		return;
	}

	if( !( other->r.client->ps.pmove.stats[PM_STAT_FEATURES] & PMFEAT_ITEMPICK ) ) {
		return;
	}

	if( !item || !( item->flags & ITFLAG_PICKABLE ) ) {
		return; // not a grabbable item

	}
	if( !G_Gametype_CanPickUpItem( item ) ) {
		return;
	}

	taken = G_PickupItem( other, item, ent->spawnflags, ent->count, ent->invpak );

	if( !( ent->spawnflags & ITEM_TARGETS_USED ) ) {
		G_UseTargets( ent, other );
		ent->spawnflags |= ITEM_TARGETS_USED;
	}

	if( !taken ) {
		return;
	}

	// flash the screen
	G_AddPlayerStateEvent( other->r.client, PSEV_PICKUP, ( item->flags & IT_WEAPON ? item->tag : 0 ) );

	// for messages
	other->r.client->teamstate.last_pickup = ent;

	// show icon and name on status bar
	other->r.client->ps.stats[STAT_PICKUP_ITEM] = item->tag;
	other->r.client->resp.pickup_msg_time = level.time + 3000;

	if( ent->attenuation ) {
		Touch_ItemSound( other, item );
	}

	if( !( ent->spawnflags & DROPPED_ITEM ) && G_Gametype_CanRespawnItem( item ) ) {
		if( ( item->type & IT_WEAPON ) && GS_RaceGametype() ) {
			return; // weapons stay in race
		}
		SetRespawn( ent, G_Gametype_RespawnTimeForItem( item ) );
		return;
	}
	G_FreeEdict( ent );
}

//======================================================================

static void drop_temp_touch( edict_t *ent, edict_t *other, cplane_t *plane, int surfFlags ) {
	if( other == ent->r.owner ) {
		return;
	}
	Touch_Item( ent, other, plane, surfFlags );
}

static void drop_make_touchable( edict_t *ent ) {
	int timeout;
	ent->touch = Touch_Item;
	timeout = G_Gametype_DroppedItemTimeout( ent->item );
	if( timeout ) {
		ent->nextThink = level.time + 1000 * timeout;
		ent->think = G_FreeEdict;
	}
}

edict_t *Drop_Item( edict_t *ent, const gsitem_t *item ) {
	edict_t *dropped;
	vec3_t forward, right;
	vec3_t offset;

	if( !G_Gametype_CanDropItem( item, false ) ) {
		return NULL;
	}

	dropped = G_Spawn();
	dropped->classname = item->classname;
	dropped->item = item;
	dropped->spawnflags = DROPPED_ITEM;
	VectorCopy( item_box_mins, dropped->r.mins );
	VectorCopy( item_box_maxs, dropped->r.maxs );
	dropped->r.solid = SOLID_TRIGGER;
	dropped->movetype = MOVETYPE_TOSS;
	dropped->touch = drop_temp_touch;
	dropped->r.owner = ent;
	dropped->r.svflags &= ~SVF_NOCLIENT;
	dropped->s.team = ent->s.team;
	dropped->s.type = ET_ITEM;
	dropped->s.itemNum = item->tag;
	dropped->s.effects = 0; // default effects are applied client side
	dropped->s.modelindex = trap_ModelIndex( dropped->item->world_model[0] );
	dropped->s.modelindex2 = trap_ModelIndex( dropped->item->world_model[1] );
	dropped->attenuation = 1;

	if( ent->r.client ) {
		trace_t trace;

		AngleVectors( ent->r.client->ps.viewangles, forward, right, NULL );
		VectorSet( offset, 24, 0, -16 );
		G_ProjectSource( ent->s.origin, offset, forward, right, dropped->s.origin );
		G_Trace( &trace, ent->s.origin, dropped->r.mins, dropped->r.maxs,
				 dropped->s.origin, ent, CONTENTS_SOLID );
		VectorCopy( trace.endpos, dropped->s.origin );

		dropped->spawnflags |= DROPPED_PLAYER_ITEM;

		// power-ups are special
		if( ( item->type & IT_POWERUP ) && item->quantity ) {
			if( ent->r.client->ps.inventory[item->tag] ) {
				dropped->count = ent->r.client->ps.inventory[item->tag];
				ent->r.client->ps.inventory[item->tag] = 0;
			} else {
				dropped->count = item->quantity;
			}
		}

		ent->r.client->teamstate.last_drop_item = item;
		VectorCopy( dropped->s.origin, ent->r.client->teamstate.last_drop_location );
	} else {
		AngleVectors( ent->s.angles, forward, right, NULL );
		VectorCopy( ent->s.origin, dropped->s.origin );

		// power-ups are special
		if( ( item->type & IT_POWERUP ) && item->quantity ) {
			dropped->count = item->quantity;
		}
	}

	VectorScale( forward, 100, dropped->velocity );
	dropped->velocity[2] = 300;

	dropped->think = drop_make_touchable;
	dropped->nextThink = level.time + 1000;

	GClip_LinkEntity( dropped );

	return dropped;
}

//======================================================================

/*
* G_PickupItem
*/
bool G_PickupItem( edict_t *other, const gsitem_t *it, int flags, int count, const int *invpack ) {
	bool taken = false;

	if( other->r.client && G_ISGHOSTING( other ) ) {
		return false;
	}

	if( !it || !( it->flags & ITFLAG_PICKABLE ) ) {
		return false;
	}

	if( it->type & IT_WEAPON ) {
		taken = Pickup_Weapon( other, it, flags, count );
	} else if( it->type & IT_POWERUP ) {
		taken = Pickup_Powerup( other, it, flags, count );
	}

	if( taken && other->r.client ) {
		G_Gametype_ScoreEvent( other->r.client, "pickup", it->classname );
	}

	return taken;
}

static edict_t *Drop_General( edict_t *ent, const gsitem_t *item ) {
	edict_t *dropped = Drop_Item( ent, item );
	if( dropped ) {
		if( ent->r.client && ent->r.client->ps.inventory[item->tag] > 0 ) {
			ent->r.client->ps.inventory[item->tag]--;
		}
	}
	return dropped;
}

/*
* G_DropItem
*/
edict_t *G_DropItem( edict_t *ent, const gsitem_t *it ) {
	if( !it || !( it->flags & ITFLAG_DROPABLE ) ) {
		return NULL;
	}

	if( !G_Gametype_CanDropItem( it, false ) ) {
		return NULL;
	}

	if( it->type & IT_WEAPON ) {
		return Drop_Weapon( ent, it );
	} else {
		return Drop_General( ent, it );
	}
}

/*
* G_UseItem
*/
void G_UseItem( edict_t *ent, const gsitem_t *it ) {
	if( !it || !( it->flags & ITFLAG_USABLE ) ) {
		return;
	}

	if( it->type & IT_WEAPON ) {
		Use_Weapon( ent, it );
	}
}

//======================================================================

/*
* Finish_SpawningItem
*/
static void Finish_SpawningItem( edict_t *ent ) {
	trace_t tr;
	vec3_t dest;
	const gsitem_t *item = ent->item;

	assert( item );

	ent->s.itemNum = item->tag;
	VectorCopy( item_box_mins, ent->r.mins );
	VectorCopy( item_box_maxs, ent->r.maxs );

	if( ent->model ) {
		ent->s.modelindex = trap_ModelIndex( ent->model );
	} else {
		if( item->world_model[0] ) {
			ent->s.modelindex = trap_ModelIndex( item->world_model[0] );
		}
		if( item->world_model[1] ) {
			ent->s.modelindex2 = trap_ModelIndex( item->world_model[1] );
		}
	}

	ent->r.solid = SOLID_TRIGGER;
	ent->r.svflags &= ~SVF_NOCLIENT;
	ent->movetype = MOVETYPE_TOSS;
	ent->touch = Touch_Item;
	ent->attenuation = 1;

	if( ent->spawnflags & 1 ) {
		ent->gravity = 0;
	}

	// drop the item to floor
	if( ent->gravity ) {
		G_Trace( &tr, ent->s.origin, ent->r.mins, ent->r.maxs, ent->s.origin, ent, MASK_SOLID );
		if( tr.startsolid ) {
			vec3_t end;

			// move it 16 units up, cause it's typical they share the leaf with the floor
			VectorCopy( ent->s.origin, end );
			end[2] += 16;

			G_Trace( &tr, end, ent->r.mins, ent->r.maxs, ent->s.origin, ent, MASK_SOLID );
			if( tr.startsolid ) {
				G_Printf( "Warning: %s %s spawns inside solid. Inhibited\n", ent->classname, vtos( ent->s.origin ) );
				G_FreeEdict( ent );
				return;
			}

			VectorCopy( tr.endpos, ent->s.origin );
		}

		VectorSet( dest, ent->s.origin[0], ent->s.origin[1], ent->s.origin[2] - 4096 );
		G_Trace( &tr, ent->s.origin, ent->r.mins, ent->r.maxs, dest, ent, MASK_SOLID );
		VectorCopy( tr.endpos, ent->s.origin );
	}

	if( ent->team ) {
		ent->flags &= ~FL_TEAMSLAVE;
		ent->chain = ent->teamchain;
		ent->teamchain = NULL;

		ent->r.svflags |= SVF_NOCLIENT;
		ent->r.solid = SOLID_NOT;

		// team slaves and targeted items aren't present at start
		if( ent == ent->teammaster && !ent->targetname ) {
			ent->nextThink = level.time + 1;
			ent->think = DoRespawn;
			GClip_LinkEntity( ent );
		}
	} else if( ent->targetname ) {
		ent->r.svflags |= SVF_NOCLIENT;
		ent->r.solid = SOLID_NOT;
	}

	GClip_LinkEntity( ent );
}

/*
* Items may be spawned above other entities and they need them spawned before
*/
void G_Items_FinishSpawningItems( void ) {
	for( edict_t *ent = game.edicts + 1 + gs.maxclients; ENTNUM( ent ) < game.numentities; ent++ ) {
		if( !ent->r.inuse || !ent->item || ent->s.type != ET_ITEM ) {
			continue;
		}

		Finish_SpawningItem( ent );

		// spawned inside solid
		if( !ent->r.inuse ) {
			continue;
		}
	}
}

/*
* SpawnItem
*
* Sets the clipping size and plants the object on the floor.
*
* Items can't be immediately dropped to floor, because they might
* be on an entity that hasn't spawned yet.
*/
void SpawnItem( edict_t *ent, const gsitem_t *item ) {
	// set items as ET_ITEM for simpleitems
	ent->s.type = ET_ITEM;
	ent->s.itemNum = item->tag;
	ent->item = item;
	ent->s.effects = 0; // default effects are applied client side
}

/*
* PrecacheItem
*
* Precaches all data needed for a given item.
* This will be called for each item spawned in a level,
* and for each item in each client's inventory.
*/
void PrecacheItem( const gsitem_t *it ) {
	int i;
	const char *s, *start;
	char data[MAX_QPATH];
	int len;
	const gsitem_t *ammo;

	if( !it ) {
		return;
	}

	if( it->pickup_sound ) {
		trap_SoundIndex( it->pickup_sound );
	}
	for( i = 0; i < MAX_ITEM_MODELS; i++ ) {
		if( it->world_model[i] ) {
			trap_ModelIndex( it->world_model[i] );
		}
	}

	if( it->icon ) {
		trap_ImageIndex( it->icon );
	}

	// parse everything for its ammo
	if( it->ammo_tag ) {
		ammo = GS_FindItemByTag( it->ammo_tag );
		if( ammo != it ) {
			PrecacheItem( ammo );
		}
	}

	// parse the space separated precache string for other items
	for( i = 0; i < 3; i++ ) {
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

			len = s - start;
			if( len >= MAX_QPATH || len < 5 ) {
				G_Error( "PrecacheItem: %s has bad precache string", it->classname );
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

//======================================================================

/*
* SetItemNames
*
* Called by worldspawn
*/
void G_PrecacheItems( void ) {
	int i;
	const gsitem_t *item;

	// precache item names and weapondefs
	for( i = 1; i < GS_MAX_ITEM_TAGS; i++ ) {
		item = GS_FindItemByTag( i );
		if( !item )
			break;

		trap_ConfigString( CS_ITEMS + i, item->name );

		if( item->type & IT_WEAPON && GS_GetWeaponDef( item->tag ) ) {
			G_PrecacheWeapondef( i, &GS_GetWeaponDef( item->tag )->firedef );
		}
	}

	// precache items
	for( i = WEAP_GUNBLADE; i < WEAP_TOTAL; i++ ) {
		item = GS_FindItemByTag( i );
		PrecacheItem( item );
	}

	// Vic: precache ammo pack if it's droppable
	item = GS_FindItemByClassname( "item_ammopack" );
	if( item && G_Gametype_CanDropItem( item, true ) ) {
		PrecacheItem( item );
	}
}
