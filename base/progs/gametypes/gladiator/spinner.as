int64 spinnerStartTime;
int switchesSoFar;
float currentDelay;

const float CountdownSwitchScale = float( pow( double( CountdownSeconds ) / double( CountdownInitialSwitchDelay ), 1.0 / CountdownNumSwitches ) );

WeaponType FindWeaponByShortName( const String & weapon ) {
	for( WeaponType i = WeaponType( 0 ); i < Weapon_Count; i++ ) {
		if( GetWeaponShortName( i ) == weapon ) {
			return i;
		}
	}

	return Weapon_Count;
}

void GiveFixedLoadout( const String & loadout ) {
	int i = 0;

	while( true ) {
		String short_name = loadout.getToken( i );
		if( short_name == "" )
			break;

		WeaponType weapon = FindWeaponByShortName( short_name );
		if( weapon == Weapon_Count ) {
			G_Print( "Not a weapon: " + short_name + "\n" );
			break;
		}

		Team @team = @G_GetTeam( TEAM_PLAYERS );
		for( int j = 0; @team.ent( j ) != null; j++ ) {
			Entity @ent = @team.ent( j );
			ent.client.giveWeapon( weapon );
		}

		i++;
	}

	Team @team = @G_GetTeam( TEAM_PLAYERS );
	for( int j = 0; @team.ent( j ) != null; j++ ) {
		Entity @ent = @team.ent( j );
		ent.client.selectWeapon( 0 );
	}
}

void DoSpinner() {
	String loadout = G_GetWorldspawnKey( "loadout" );
	if( loadout != "" ) {
		GiveFixedLoadout( loadout );
		return;
	}

	spinnerStartTime = levelTime;
	switchesSoFar = 0;
	currentDelay = CountdownInitialSwitchDelay;

	Team @team = @G_GetTeam( TEAM_PLAYERS );
	for( int i = 0; @team.ent( i ) != null; i++ ) {
		Entity @ent = @team.ent( i );
		ent.client.pmoveFeatures = ent.client.pmoveFeatures & ~( PMFEAT_WEAPONSWITCH | PMFEAT_SCOPE );
	}

	Entity@ spinner = @G_SpawnEntity( "spinner" );
	@spinner.think = spinner_think;
	spinner.nextThink = spinnerStartTime + int64( currentDelay * 1000 );
}

void spinner_think( Entity@ self ) {
	bool last = switchesSoFar == CountdownNumSwitches + 1;
	Team @team = @G_GetTeam( TEAM_PLAYERS );

	int weapon1 = RandomUniform( Weapon_None + 1, Weapon_Count );
	int weapon2 = RandomUniform( Weapon_None + 1, Weapon_Count - 1 );
	if( weapon2 >= weapon1 )
		weapon2++;

	for( int j = 0; @team.ent( j ) != null; j++ ) {
		Entity @ent = @team.ent( j );

		ent.client.inventoryClear();
		ent.client.giveWeapon( WeaponType( weapon1 ) );
		ent.client.giveWeapon( WeaponType( weapon2 ) );
		ent.client.selectWeapon( 0 );

		if( last ) {
			ent.client.pmoveFeatures = ent.client.pmoveFeatures | PMFEAT_WEAPONSWITCH | PMFEAT_SCOPE;
		}
	}

	if( !last ) {
		self.nextThink = spinnerStartTime + int64( currentDelay * 1000 );
		currentDelay *= CountdownSwitchScale;
		switchesSoFar++;
	}
}
