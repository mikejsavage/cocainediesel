int randWeap;
int64 spinnerStartTime;
int switchesSoFar;
float currentDelay;

const float CountdownSwitchScale = float( pow( double( CountdownSeconds ) / double( CountdownInitialSwitchDelay ), 1.0 / CountdownNumSwitches ) );

void DoSpinner() {
	randWeap = random_uniform( WEAP_GUNBLADE, WEAP_ELECTROBOLT );
	spinnerStartTime = levelTime;
	switchesSoFar = 0;
	currentDelay = CountdownInitialSwitchDelay;

	Team @team = @G_GetTeam( TEAM_PLAYERS );
	for( int j = 0; @team.ent( j ) != null; j++ ) {
		Entity @ent = @team.ent( j );
		for( int i = WEAP_GUNBLADE; i < WEAP_TOTAL; i++ ) {
			ent.client.inventoryGiveItem( i );
			ent.client.inventorySetCount( AMMO_GUNBLADE + ( i - WEAP_GUNBLADE ), 66 );
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
		if( curr > WEAP_ELECTROBOLT )
			curr = WEAP_GUNBLADE;

		if( last ) {
			for( int i = WEAP_GUNBLADE; i < WEAP_TOTAL; i++ ) {
				if( i == curr )
					continue;
				if( i != WEAP_GUNBLADE )
					client.inventorySetCount( i, 0 );
				client.inventorySetCount( AMMO_GUNBLADE + ( i - WEAP_GUNBLADE ), 0 );
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
