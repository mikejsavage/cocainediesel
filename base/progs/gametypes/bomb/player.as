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

const int MAX_CASH = 500;

cPlayer@[] players( maxClients ); // array of handles
bool playersInitialized = false;

class cPlayer {
	Client @client;

	uint[] loadout( Weapon_Count - 1 );
	int num_weapons;

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
		this.client.giveWeapon( Weapon_Knife );

		for( int i = 0; i < this.num_weapons; i++ ) {
			this.client.giveWeapon( WeaponType( this.loadout[ i ] ) );
		}

		this.client.selectWeapon( Weapon_None );
	}


	void showShop() {
		if( this.client.team == TEAM_SPECTATOR ) {
			return;
		}

		String command = "changeloadout";
		for( int i = 0; i < this.num_weapons; i++ ) {
			command += " " + this.loadout[ i ];
		}
		this.client.execGameCommand( command );
	}

	void setLoadout( String &cmd ) {
		int cash = MAX_CASH;

		for( uint i = 0; i < this.loadout.length(); i++ ) {
			this.loadout[ i ] = Weapon_None;
		}

		this.num_weapons = 0;

		{
			int i = 0;
			while( true ) {
				String token = cmd.getToken( i );
				i++;
				if( token == "" )
					break;
				int weapon = token.toInt();
				if( weapon > Weapon_None && weapon < Weapon_Count && weapon != Weapon_Knife ) {
					this.loadout[ this.num_weapons ] = weapon;
					cash -= WeaponCost( WeaponType( weapon ) );
					this.num_weapons++;
				}
			}
		}

		if( cash < 0 ) {
			G_PrintMsg( @this.client.getEnt(), "You are not wealthy enough\n" );
			return;
		}

		String command = "saveloadout";
		for( int i = 0; i < this.num_weapons; i++ ) {
			command += " " + this.loadout[ i ];
		}
		this.client.execGameCommand( command );

		if( match.getState() == MATCH_STATE_WARMUP ) {
			if( lastLoadoutChangeTime == -1 || levelTime - lastLoadoutChangeTime >= 1000 ) {
				giveInventory();
				lastLoadoutChangeTime = levelTime;
			}
			else {
				G_PrintMsg( @this.client.getEnt(), "You can't change weapons so fast\n" );
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

void dropSpawnToFloor( Entity @ent ) {
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

void spawn_bomb_attacking( Entity @ent ) {
	dropSpawnToFloor( ent );
}

void spawn_bomb_defending( Entity @ent ) {
	dropSpawnToFloor( ent );
}

void team_CTF_alphaspawn( Entity @ent ) {
	dropSpawnToFloor( ent );
}

void team_CTF_betaspawn( Entity @ent ) {
	dropSpawnToFloor( ent );
}
