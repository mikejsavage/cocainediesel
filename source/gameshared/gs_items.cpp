#include "qcommon/base.h"
#include "gameshared/gs_public.h"

const Item itemdefs[] = {
	{ "", "", ItemCategory_Count },

	{
		"Dummy bomb", "dummybomb",
		ItemCategory_Utility,
	},

	{
		"Grenade", "grenade",
		ItemCategory_Utility,
		3, 1000, 20.0f
	},

	{
		"Jetpack", "jetpack",
		ItemCategory_Perk,
	}
};

STATIC_ASSERT( ARRAY_COUNT( itemdefs ) == Item_Count );

const Item * GS_GetItem( ItemType item ) {
	assert( item < Item_Count );
	return &itemdefs[ item ];
}

ItemType GS_ThinkPlayerItem( const gs_state_t * gs, SyncPlayerState * player, const usercmd_t * cmd, int timeDelta ) {
	ItemType item = player->items[ ItemCategory_Utility ].item;
	if( player->items[ ItemCategory_Utility ].uses == 0 ) {
		return Item_None;
	}
	const Item * def = GS_GetItem( item );
	
	int buttons = player->pmove.no_control_time > 0 ? 0 : cmd->buttons;

	if( ( buttons & BUTTON_UTILITY ) ) {
		if ( player->weapon_state == WeaponState_Disabled ) {
			player->item_time = Min2( player->item_time + cmd->msec, s32( def->charge_time ) );
			// gs->api.PredictedFireWeapon( player->POVnum, Weapon_GrenadeLauncher );
		}
		else if( player->pmove.features & PMFEAT_WEAPONSWITCH ) {
			if( player->weapon_state == WeaponState_Ready ) {
				player->weapon_state = WeaponState_Disabling;
				player->weapon_time = GS_GetWeaponDef( player->weapon )->weapondown_time;
				gs->api.PredictedEvent( player->POVnum, EV_WEAPONDROP, 0 );
			}
		}
	} else {
		if( player->weapon_state == WeaponState_Disabled ) {
			gs->api.PredictedFireWeapon( player->POVnum, Weapon_GrenadeLauncher );
			player->items[ ItemCategory_Utility ].uses--;
		}
		if( player->weapon_state == WeaponState_Disabling || player->weapon_state == WeaponState_Disabled ) {
			player->weapon = Weapon_None;
			player->weapon_state = WeaponState_SwitchingOut;
			player->weapon_time = 0;
		}
	}

	return Item_None;
}
