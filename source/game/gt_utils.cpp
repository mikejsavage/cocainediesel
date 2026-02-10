#include "qcommon/base.h"
#include "game/g_local.h"

static Loadout loadouts[ MAX_CLIENTS ];

void ShowShop( edict_t * ent, msg_t m ) {
	if( ent->s.team == Team_None ) {
		return;
	}

	TempAllocator temp = svs.frame_arena.temp();
	const Loadout & loadout = loadouts[ PLAYERNUM( ent ) ];
	PF_GameCmd( ent, temp( "changeloadout {}", loadout ) );
}

void GiveInventory( edict_t * ent ) {
	ClearInventory( &ent->r.client->ps );

	const Loadout & loadout = loadouts[ PLAYERNUM( ent ) ];

	for( u32 category = 0; category < WeaponCategory_Count; category++ ) {
		WeaponType weapon = loadout.weapons[ category ];
		G_GiveWeapon( ent, weapon );
	}

	G_SelectWeapon( ent, 1 );

	ent->r.client->ps.gadget = loadout.gadget;
	ent->r.client->ps.gadget_ammo = GetGadgetDef( loadout.gadget )->uses;

	G_GivePerk( ent, loadout.perk );
}

static Loadout DefaultLoadout() {
	Loadout loadout = { };
	loadout.weapons[ WeaponCategory_Primary ] = Weapon_Bazooka;
	loadout.weapons[ WeaponCategory_Secondary ] = Weapon_Shotgun;
	loadout.weapons[ WeaponCategory_Backup ] = Weapon_Crossbow;
	loadout.weapons[ WeaponCategory_Melee ] = Weapon_Knife;

	loadout.perk = Perk_Hooligan;

	for( int i = 0; i < WeaponCategory_Count; i++ ) {
		Assert( loadout.weapons[ i ] != Weapon_None );
	}

	loadout.gadget = Gadget_Axe;

	return loadout;
}

void SetLoadout( edict_t * ent, const char * loadout_string, bool fallback_to_default ) {
	Span< const char > loadout_span = loadout_string == NULL ? ""_sp : MakeSpan( loadout_string );
	Optional< Loadout > loadout = ParseLoadout( loadout_span );
	if( !loadout.exists ) {
		if( !fallback_to_default )
			return;
		loadout = DefaultLoadout();
	}

	TempAllocator temp = svs.frame_arena.temp();
	PF_GameCmd( ent, temp( "saveloadout {}", loadout.value ) );

	loadouts[ PLAYERNUM( ent ) ] = loadout.value;

	if( G_ISGHOSTING( ent ) ) {
		return;
	}

	if( server_gs.gameState.match_state == MatchState_Warmup || server_gs.gameState.match_state == MatchState_Countdown ) {
		GiveInventory( ent );
	}

	if( server_gs.gameState.match_state == MatchState_Playing && server_gs.gameState.round_state == RoundState_Countdown ) {
		GiveInventory( ent );
	}
}
