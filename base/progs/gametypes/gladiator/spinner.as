int randWeap;
int64 spinnerStartTime;
int switchesSoFar;
float currentDelay;

const float CountdownSwitchScale = float( pow( double( CountdownSeconds ) / double( CountdownInitialSwitchDelay ), 1.0 / CountdownNumSwitches ) );

void DoSpinner() {
	randWeap = random_uniform( 0, Weapon_Count );
	spinnerStartTime = levelTime;
	switchesSoFar = 0;
	currentDelay = CountdownInitialSwitchDelay;

	Team @team = @G_GetTeam( TEAM_PLAYERS );
	for( int j = 0; @team.ent( j ) != null; j++ ) {
		Entity @ent = @team.ent( j );
		for( int i = 0; i < Weapon_Count; i++ ) {
			ent.client.giveWeapon( WeaponType( i ), true );
		}
		ent.client.pmoveFeatures = ent.client.pmoveFeatures & ~PMFEAT_WEAPONSWITCH;
		ent.client.selectWeapon( randWeap );
	}

	Entity@ spinner = @G_SpawnEntity( "spinner" );
	@spinner.think = spinner_think;
	spinner.nextThink = spinnerStartTime + int64( currentDelay * 1000 );
}

void spinner_think( Entity@ self ) {
	bool last = switchesSoFar == CountdownNumSwitches + 1;
	Team @team = @G_GetTeam( TEAM_PLAYERS );
	for( int j = 0; @team.ent( j ) != null; j++ ) {
		Entity @ent = @team.ent( j );
		Client @client = @ent.client;
		int curr = client.pendingWeapon + 1;
		if( curr == Weapon_Count )
			curr = Weapon_Knife;

		if( last ) {
			for( int i = 0; i < Weapon_Count; i++ ) {
				if( i == curr )
					continue;
				if( i != Weapon_Knife )
					client.giveWeapon( WeaponType( i ), false );
			}
			client.pmoveFeatures = ent.client.pmoveFeatures | PMFEAT_WEAPONSWITCH;
		}

		client.selectWeapon( curr );
	}

	if( !last ) {
		self.nextThink = spinnerStartTime + int64( currentDelay * 1000 );
		currentDelay *= CountdownSwitchScale;
		switchesSoFar++;
	}
}
