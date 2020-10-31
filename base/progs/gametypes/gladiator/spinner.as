int64 spinnerStartTime;
int switchesSoFar;
float currentDelay;

const float CountdownSwitchScale = float( pow( double( CountdownSeconds ) / double( CountdownInitialSwitchDelay ), 1.0 / CountdownNumSwitches ) );

void DoSpinner() {
	spinnerStartTime = levelTime;
	switchesSoFar = 0;
	currentDelay = CountdownInitialSwitchDelay;

	Team @team = @G_GetTeam( TEAM_PLAYERS );
	for( int j = 0; @team.ent( j ) != null; j++ ) {
		Entity @ent = @team.ent( j );

		ent.client.inventoryClear();
		ent.client.pmoveFeatures = ent.client.pmoveFeatures & ~( PMFEAT_WEAPONSWITCH | PMFEAT_SCOPE );
		ent.client.selectWeapon( 0 );
	}

	Entity@ spinner = @G_SpawnEntity( "spinner" );
	@spinner.think = spinner_think;
	spinner.nextThink = spinnerStartTime + int64( currentDelay * 1000 );
}

void spinner_think( Entity@ self ) {
	bool last = switchesSoFar == CountdownNumSwitches + 1;
	Team @team = @G_GetTeam( TEAM_PLAYERS );

	int weapon1 = random_uniform( Weapon_None + 1, Weapon_Count );
	int weapon2 = random_uniform( Weapon_None + 1, Weapon_Count - 1 );
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
			ent.client.selectWeapon( -1 );
		}
	}

	if( !last ) {
		self.nextThink = spinnerStartTime + int64( currentDelay * 1000 );
		currentDelay *= CountdownSwitchScale;
		switchesSoFar++;
	}
}
