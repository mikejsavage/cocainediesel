void DoSpinner()
{

	Entity@ spinner = @G_SpawnEntity("spinner");
	@spinner.think = spinner_start;
	spinner.nextThink = levelTime + 1;
	spinner.wait = 1;
	spinner.linkEntity();
	randWeap = random_uniform( WEAP_GUNBLADE, WEAP_ELECTROBOLT );
}

int randWeap = -1;

void spinner_start(Entity@ self)
{
	Entity @ent;
	Client @client;
	Team @team;

	@team = @G_GetTeam( TEAM_PLAYERS );
	Item @item;
	Item @ammoItem;

	int startWeap = randWeap + 5;
	if ( startWeap > WEAP_ELECTROBOLT )
		startWeap -= 8;

	// dafuq is this, i give up :p
	if ( randWeap == 3 )
		startWeap--;
	if ( randWeap == 4 )
		startWeap = 8;
	if ( randWeap == 5 )
		startWeap--;

	for ( int j = 0; @team.ent( j ) != null; j++ )
	{
		@ent = @team.ent( j );
		for ( int i = WEAP_GUNBLADE ; i < WEAP_TOTAL; i++ )
		{
			ent.client.inventoryGiveItem( i );

			@item = @G_GetItem( i );

			@ammoItem = @G_GetItem( item.ammoTag );
			if ( @ammoItem != null )
				ent.client.inventorySetCount( ammoItem.tag, 66 );
		}
		ent.client.pmoveFeatures = ent.client.pmoveFeatures & ~PMFEAT_WEAPONSWITCH;
		ent.client.selectWeapon(startWeap);
	}

	@self.think = spinner_think;
	self.nextThink = levelTime;

}

void spinner_think(Entity@ self)
{

	Entity @ent;
	Client @client;
	Team @team;
	Item @item;
	Item @ammoItem;
	int curr;

	bool doneSpinning = false;


	self.wait *= 1.2f;
	if ( self.wait >= 1000 )
	{
		doneSpinning = true;
	} else {
		self.nextThink = levelTime + uint(self.wait);
	}


	@team = @G_GetTeam( TEAM_PLAYERS );

	for ( int j = 0; @team.ent( j ) != null; j++ )
	{
		@ent = @team.ent( j );
		if ( ent.isGhosting() )
			continue;
		@client = @ent.client;

		curr = client.pendingWeapon;
		curr++;
		if ( curr > WEAP_ELECTROBOLT )
			curr = WEAP_GUNBLADE;
		client.selectWeapon(curr);

		if ( doneSpinning )
		{
			for ( int i = WEAP_GUNBLADE; i < WEAP_TOTAL; i++ )
			{
				if ( i == curr )
					continue;

				@item = @G_GetItem( i );

				@ammoItem = @G_GetItem( item.ammoTag );

				if ( i != WEAP_GUNBLADE )
					client.inventorySetCount(i, 0);
				client.inventorySetCount(ammoItem.tag, 0);
			}
			client.pmoveFeatures = ent.client.pmoveFeatures | PMFEAT_WEAPONSWITCH;

			client.selectWeapon( -1 );
		}
	}
}
