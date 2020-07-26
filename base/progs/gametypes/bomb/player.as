const int MAX_CASH = 500;

cPlayer@[] players( maxClients ); // array of handles
bool playersInitialized = false;

class cPlayer {
	Client @client;

	uint[] loadout( Weapon_Count - 1 );
	int num_weapons;

	int killsThisRound;

	uint arms;
	uint defuses;

	bool dueToSpawn; // used for respawning during countdown

	cPlayer( Client @player ) {
		@this.client = @player;

		setLoadout( this.client.getUserInfoKey( "cg_loadout" ) );

		this.arms = 0;
		this.defuses = 0;

		this.dueToSpawn = false;

		@players[player.playerNum] = @this;
	}

	void rearrangeInventory() {
		for( int i = 0; i < this.num_weapons; i++ ) {
			this.client.setWeaponIndex( WeaponType( this.loadout[ i ] ), i + 1 ); // + 1 because of knife
		}
	}

	void giveInventory() {
		this.client.inventoryClear();
		this.client.giveWeapon( Weapon_Knife );

		for( int i = 0; i < this.num_weapons; i++ ) {
			this.client.giveWeapon( WeaponType( this.loadout[ i ] ) );
		}

		this.client.selectWeapon( 0 );
		this.client.selectWeapon( 1 );
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

		int old_num_weapons = this.num_weapons;
		this.num_weapons = 0;

		uint[] newloadout( Weapon_Count - 1 );
		{
			int i = 0;
			while( true ) {
				String token = cmd.getToken( i );
				i++;
				if( token == "" )
					break;
				int weapon = token.toInt();
				if( weapon > Weapon_None && weapon < Weapon_Count && weapon != Weapon_Knife ) {
					newloadout[ this.num_weapons ] = weapon;
					cash -= WeaponCost( WeaponType( weapon ) );
					this.num_weapons++;
				}
			}
		}

		for( uint i = 0; i < newloadout.length(); i++ ) {
			this.loadout[ i ] = newloadout[ i ];
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

		if( this.client.getEnt().isGhosting() ) {
			return;
		}

		if( match.getState() == MATCH_STATE_WARMUP || match.getState() == MATCH_STATE_COUNTDOWN ) {
			giveInventory();
		}

		if( match.getState() == MATCH_STATE_PLAYTIME ) {
			if( roundState == RoundState_Pre ) {
				giveInventory();
			}
			else if( old_num_weapons == this.num_weapons ) {
				bool rearranging = true;

				for( uint i = 0; i < newloadout.length(); i++ ) {
					if( this.loadout.find( newloadout[ i ] ) == -1 ) {
						rearranging = false;
						break;
					}
				}

				if( rearranging ) {
					rearrangeInventory();
				}
			}
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
