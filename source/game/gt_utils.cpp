#include "qcommon/base.h"
#include "qcommon/array.h"

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
	loadout.weapons[ WeaponCategory_Primary ] = Weapon_RocketLauncher;
	loadout.weapons[ WeaponCategory_Secondary ] = Weapon_Shotgun;
	loadout.weapons[ WeaponCategory_Backup ] = Weapon_StakeGun;
	loadout.weapons[ WeaponCategory_Melee ] = Weapon_Knife;

	loadout.perk = Perk_Hooligan;

	for( int i = 0; i < WeaponCategory_Count; i++ ) {
		assert( loadout.weapons[ i ] != Weapon_None );
	}

	loadout.gadget = Gadget_ThrowingAxe;

	return loadout;
}

static bool ParseLoadout( Loadout * loadout, const char * loadout_string ) {
	if( loadout_string == NULL )
		return false;

	*loadout = { };

	Span< const char > cursor = MakeSpan( loadout_string );

	for( int i = 0; i < WeaponCategory_Count; i++ ) {
		Span< const char > token = ParseToken( &cursor, Parse_DontStopOnNewLine );
		int weapon;
		if( !TrySpanToInt( token, &weapon ) )
			return false;

		if( weapon <= Weapon_None || weapon >= Weapon_Count )
			return false;

		WeaponCategory category = GS_GetWeaponDef( WeaponType( weapon ) )->category;
		if( category != i )
			return false;

		loadout->weapons[ category ] = WeaponType( weapon );
	}

	{
		Span< const char > token = ParseToken( &cursor, Parse_DontStopOnNewLine );
		int perk;
		if( !TrySpanToInt( token, &perk ) || perk <= Perk_None || perk >= Perk_Count )
			return false;
		if( !GetPerkDef( PerkType( perk ) )->enabled )
			return false;
		loadout->perk = PerkType( perk );
	}

	{
		Span< const char > token = ParseToken( &cursor, Parse_DontStopOnNewLine );
		int gadget;
		if( !TrySpanToInt( token, &gadget ) || gadget <= Gadget_None || gadget >= Gadget_Count )
			return false;
		loadout->gadget = GadgetType( gadget );
	}

	return cursor.ptr != NULL && cursor.n == 0;
}

void SetLoadout( edict_t * ent, const char * loadout_string, bool fallback_to_default ) {
	Loadout loadout;
	if( !ParseLoadout( &loadout, loadout_string ) ) {
		if( !fallback_to_default )
			return;
		loadout = DefaultLoadout();
	}

	TempAllocator temp = svs.frame_arena.temp();
	PF_GameCmd( ent, temp( "saveloadout {}", loadout ) );

	loadouts[ PLAYERNUM( ent ) ] = loadout;

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