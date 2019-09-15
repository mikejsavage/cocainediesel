/*
Copyright (C) 2009-2010 Chasseur de bots

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

enum PrimaryWeapon {
	PrimaryWeapon_EBRL,
	PrimaryWeapon_RLLG,
	PrimaryWeapon_EBLG,
}

enum SecondaryWeapon {
	SecondaryWeapon_PG,
	SecondaryWeapon_RG,
	SecondaryWeapon_GL,
	SecondaryWeapon_MG,
}

String[] primaryWeaponNames = { "ebrl", "rllg", "eblg" };
String[] secondaryWeaponNames = { "pg", "rg", "gl", "mg" };

const int AMMO_EB = 15;
const int AMMO_RL = 15;
const int AMMO_LG = 180;
const int AMMO_PG = 140;
const int AMMO_RG = 15;
const int AMMO_GL = 10;
const int AMMO_MG = 150;

cPlayer@[] players( maxClients ); // array of handles
bool playersInitialized = false;

class cPlayer {
	Client @client;

	PrimaryWeapon weapPrimary;
	SecondaryWeapon weapSecondary;

	int64 lastLoadoutChangeTime; // so people can't spam change weapons during warmup

	int killsThisRound;

	uint arms;
	uint defuses;

	bool dueToSpawn; // used for respawning during countdown

	cPlayer( Client @player ) {
		@this.client = @player;

		setLoadout( this.client.getUserInfoKey( "cg_loadout" ) );

		this.lastLoadoutChangeTime = -1;

		this.arms = 0;
		this.defuses = 0;

		this.dueToSpawn = false;

		@players[player.playerNum] = @this;
	}

	void giveInventory() {
		this.client.inventoryClear();

		this.client.inventoryGiveItem( WEAP_GUNBLADE );

		switch( this.weapPrimary ) {
			case PrimaryWeapon_EBRL:
				this.client.inventoryGiveItem( WEAP_ROCKETLAUNCHER );
				this.client.inventorySetCount( AMMO_ROCKETS, AMMO_RL );
				this.client.inventoryGiveItem( WEAP_ELECTROBOLT );
				this.client.inventorySetCount( AMMO_BOLTS, AMMO_EB );
				break;

			case PrimaryWeapon_RLLG:
				this.client.inventoryGiveItem( WEAP_ROCKETLAUNCHER );
				this.client.inventorySetCount( AMMO_ROCKETS, AMMO_RL );
				this.client.inventoryGiveItem( WEAP_LASERGUN );
				this.client.inventorySetCount( AMMO_LASERS, AMMO_LG );
				break;

			case PrimaryWeapon_EBLG:
				this.client.inventoryGiveItem( WEAP_ELECTROBOLT );
				this.client.inventorySetCount( AMMO_BOLTS, AMMO_EB );
				this.client.inventoryGiveItem( WEAP_LASERGUN );
				this.client.inventorySetCount( AMMO_LASERS, AMMO_LG );
				break;
		}

		switch( this.weapSecondary ) {
			case SecondaryWeapon_PG:
				this.client.inventoryGiveItem( WEAP_PLASMAGUN );
				this.client.inventorySetCount( AMMO_PLASMA, AMMO_PG );
				break;

			case SecondaryWeapon_RG:
				this.client.inventoryGiveItem( WEAP_RIOTGUN );
				this.client.inventorySetCount( AMMO_SHELLS, AMMO_RG );
				break;

			case SecondaryWeapon_GL:
				this.client.inventoryGiveItem( WEAP_GRENADELAUNCHER );
				this.client.inventorySetCount( AMMO_GRENADES, AMMO_GL );
				break;

			case SecondaryWeapon_MG:
				this.client.inventoryGiveItem( WEAP_MACHINEGUN );
				this.client.inventorySetCount( AMMO_BULLETS, AMMO_MG );
				break;
		}

		this.client.selectWeapon( -1 );
	}

	void showShop() {
		if( this.client.team == TEAM_SPECTATOR ) {
			return;
		}

		this.client.execGameCommand( "changeloadout " + this.weapPrimary + " " + this.weapSecondary );
	}

	void setLoadout( String &loadout ) {
		String primary = loadout.getToken( 0 );
		String secondary = loadout.getToken( 1 );

		for( uint i = 0; i < primaryWeaponNames.length(); i++ ) {
			if( primary == primaryWeaponNames[ i ] ) {
				this.weapPrimary = PrimaryWeapon( i );
			}
		}

		for( uint i = 0; i < secondaryWeaponNames.length(); i++ ) {
			if( secondary == secondaryWeaponNames[ i ] ) {
				this.weapSecondary = SecondaryWeapon( i );
			}
		}

		this.client.execGameCommand( "saveloadout " + primaryWeaponNames[ this.weapPrimary ] + " " + secondaryWeaponNames[ this.weapSecondary ] );

		if( match.getState() == MATCH_STATE_WARMUP ) {
			if( lastLoadoutChangeTime == -1 || levelTime - lastLoadoutChangeTime >= 1000 ) {
				giveInventory();
				lastLoadoutChangeTime = levelTime;
			}
			else {
				G_PrintMsg( @this.client.getEnt(), "Wait a second\n" );
			}
		}

		if( match.getState() == MATCH_STATE_PLAYTIME && roundState == RoundState_Pre ) {
			giveInventory();
		}
	}
}

// since i am using an array of handles this must
// be done to avoid null references if there are players
// already on the server
void playersInit() {
	// do initial setup (that doesn't spawn any entities, but needs clients to be created) only once, not every round
	if( !playersInitialized ) {
		for( int i = 0; i < maxClients; i++ ) {
			Client @client = @G_GetClient( i );

			if( client.state() >= CS_CONNECTING ) {
				cPlayer( @client );
			}
		}

		playersInitialized = true;
	}
}

// using a global counter would be faster
uint getCarrierCount( int teamNum ) {
	uint count = 0;

	Team @team = @G_GetTeam( teamNum );

	for( int i = 0; @team.ent( i ) != null; i++ ) {
		if( @team.ent( i ) == @bombCarrier ) {
			count++;
		}
	}

	return count;
}

void resetKillCounters() {
	for( int i = 0; i < maxClients; i++ ) {
		if( @players[i] != null ) {
			players[i].killsThisRound = 0;
		}
	}
}

cPlayer @playerFromClient( Client @client ) {
	cPlayer @player = @players[client.playerNum];

	// XXX: as of 0.18 this check shouldn't be needed as playersInit works
	if( @player == null ) {
		assert( false, "player.as playerFromClient: no player exists for client - state: " + client.state() );

		return cPlayer( @client );
	}

	return @player;
}

void dropSpawnToFloor( Entity @ent, int team ) {
	Vec3 mins( -16, -16, -24 ), maxs( 16, 16, 40 );

	Vec3 start = ent.origin + Vec3( 0, 0, 16 );
	Vec3 end = ent.origin - Vec3( 0, 0, 1024 );

	Trace trace;
	trace.doTrace( start, mins, maxs, end, ent.entNum, MASK_SOLID );

	if( trace.startSolid ) {
		G_Print( ent.classname + " starts inside solid, removing...\n" );
		ent.freeEntity();
		return;
	}

	// move it 1 unit away from the plane
	ent.origin = trace.endPos + trace.planeNormal;
}

void team_CTF_alphaspawn( Entity @ent ) {
	dropSpawnToFloor( ent, defendingTeam );
}

void team_CTF_betaspawn( Entity @ent ) {
	dropSpawnToFloor( ent, attackingTeam );
}

void spawn_offense( Entity @ent ) {
	dropSpawnToFloor( ent, attackingTeam );
}

void spawn_defense( Entity @ent ) {
	dropSpawnToFloor( ent, attackingTeam );
}
