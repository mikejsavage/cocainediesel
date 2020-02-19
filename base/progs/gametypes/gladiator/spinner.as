int lastWeap1;
int lastWeap2;

int64 spinnerStartTime;
int switchesSoFar;
float currentDelay;

const float CountdownSwitchScale = float( pow( double( CountdownSeconds ) / double( CountdownInitialSwitchDelay ), 1.0 / CountdownNumSwitches ) );

void DoSpinner() {
	lastWeap1 = random_uniform( 0, Weapon_Count );
	lastWeap2 = lastWeap1;

	while( lastWeap1 == lastWeap2 ) {
		lastWeap2 = random_uniform( 0, Weapon_Count );
	}

	spinnerStartTime = levelTime;
	switchesSoFar = 0;
	currentDelay = CountdownInitialSwitchDelay;

	Team @team = @G_GetTeam( TEAM_PLAYERS );
	for( int j = 0; @team.ent( j ) != null; j++ ) {
		Entity @ent = @team.ent( j );

		ent.client.inventoryClear();
		ent.client.giveWeapon( WeaponType( lastWeap1 ), true );
		ent.client.giveWeapon( WeaponType( lastWeap2 ), true );
		ent.client.pmoveFeatures = ent.client.pmoveFeatures & ~PMFEAT_WEAPONSWITCH;
		ent.client.selectWeapon( -1 );
	}

	Entity@ spinner = @G_SpawnEntity( "spinner" );
	@spinner.think = spinner_think;
	spinner.nextThink = spinnerStartTime + int64( currentDelay * 1000 );
}

void spinner_think( Entity@ self ) {
	bool last = switchesSoFar == CountdownNumSwitches + 1;
	Team @team = @G_GetTeam( TEAM_PLAYERS );

	int tmp1 = lastWeap1;
	int tmp2 = lastWeap2;

	while( tmp1 == lastWeap1 || tmp1 == lastWeap2 ) {
		tmp1 = random_uniform( 0, Weapon_Count );
	}

	while( tmp2 == lastWeap1 || tmp2 == lastWeap2 || tmp2 == tmp1 ) {
		tmp2 = random_uniform( 0, Weapon_Count );
	}

	lastWeap1 = tmp1;
	lastWeap2 = tmp2;

	for( int j = 0; @team.ent( j ) != null; j++ ) {
		Entity @ent = @team.ent( j );
		
		ent.client.inventoryClear();
		ent.cclient.giveWeapon( WeaponType( lastWeap1 ), true );
		ent.cclient.giveWeapon( WeaponType( lastWeap2 ), true );
		ent.cclient.selectWeapon( -1 );

		if( last ) {
			client.pmoveFeatures = ent.client.pmoveFeatures | PMFEAT_WEAPONSWITCH;
		}
	}

	if( !last ) {
		self.nextThink = spinnerStartTime + int64( currentDelay * 1000 );
		currentDelay *= CountdownSwitchScale;
		switchesSoFar++;
	}
}
