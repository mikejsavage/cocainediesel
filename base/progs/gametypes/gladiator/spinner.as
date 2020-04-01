int lastWeap1;
int lastWeap2;

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
	}

	Entity@ spinner = @G_SpawnEntity( "spinner" );
	@spinner.think = spinner_think;
	spinner.nextThink = spinnerStartTime + int64( currentDelay * 1000 );
}

void spinner_think( Entity@ self ) {
	bool last = switchesSoFar == CountdownNumSwitches + 1;
	Team @team = @G_GetTeam( TEAM_PLAYERS );

	int tmp1 = random_uniform( 0, Weapon_Count );
	int tmp2 = random_uniform( 0, Weapon_Count );

	if( tmp1 == lastWeap1 || tmp1 == lastWeap2 ) {
		tmp1++;
		if( tmp1 == Weapon_Count  ) {
			tmp1 = Weapon_Knife;
		}
	}

	if( tmp2 == lastWeap1 || tmp2 == lastWeap2 ) {
		tmp2++;
		if( tmp2 == Weapon_Count  ) {
			tmp2 = Weapon_Knife;
		}
	}

	lastWeap1 = tmp1;
	lastWeap2 = tmp2;

	for( int j = 0; @team.ent( j ) != null; j++ ) {
		Entity @ent = @team.ent( j );

		ent.client.inventoryClear();
		ent.client.giveWeapon( WeaponType( lastWeap1 ) );
		ent.client.giveWeapon( WeaponType( lastWeap2 ) );

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
