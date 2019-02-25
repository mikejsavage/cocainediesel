void MOD_drunk_think()
{
	Entity @ent;
	Team @team;

	@team = @G_GetTeam( TEAM_PLAYERS );

	for ( int j = 0; @team.ent( j ) != null; j++ )
	{
		@ent = @team.ent( j );
		if ( ent.isGhosting() )
			continue;

		Vec3 vel = ent.velocity;
		float amount = 50;
		vel += Vec3( brandom(-amount,amount), brandom(-amount,amount), 0 );
		ent.velocity = vel;
	}
}

//------------------------

void MOD_drunk2_think()
{
	Entity @ent;
	Team @team;

	@team = @G_GetTeam( TEAM_PLAYERS );

	for ( int j = 0; @team.ent( j ) != null; j++ )
	{
		@ent = @team.ent( j );
		if ( ent.isGhosting() )
			continue;

		Vec3 angles = ent.angles;
		float amount = 5;
		angles += Vec3( brandom(-amount,amount), brandom(-amount,amount), 0 );
		ent.angles = angles;
	}
}

//------------------------

float MOD_upsidedown_amount = 0.0f;

void MOD_upsidedown_init()
{
	MOD_upsidedown_amount = 0;
}

void MOD_upsidedown_think()
{
	if ( MOD_upsidedown_amount > 180.0f )
		return;

	Entity @ent;
	Team @team;

	@team = @G_GetTeam( TEAM_PLAYERS );

	for ( int j = 0; @team.ent( j ) != null; j++ )
	{
		@ent = @team.ent( j );
		if ( ent.isGhosting() )
			continue;

		Vec3 angles = ent.angles;
		angles.z = MOD_upsidedown_amount;
		ent.angles = angles;
	}
	MOD_upsidedown_amount += 0.5;
}

void MOD_upsidedown_stop()
{
	Entity @ent;
	Team @team;

	@team = @G_GetTeam( TEAM_PLAYERS );

	for ( int j = 0; @team.ent( j ) != null; j++ )
	{
		@ent = @team.ent( j );
		Vec3 angles = ent.angles;
		angles.z = 0;
		ent.angles = angles;
	}
}

//------------------------

void MOD_nomove_init()
{
	Entity @ent;
	Team @team;

	@team = @G_GetTeam( TEAM_PLAYERS );

	for ( int j = 0; @team.ent( j ) != null; j++ )
	{
		@ent = @team.ent( j );
		if ( ent.isGhosting() )
			continue;

		ent.client.pmoveMaxSpeed = 0;
	}
}

void MOD_nomove_stop()
{
	Entity @ent;
	Team @team;

	@team = @G_GetTeam( TEAM_PLAYERS );

	for ( int j = 0; @team.ent( j ) != null; j++ )
	{
		@ent = @team.ent( j );
		if ( ent.isGhosting() )
			continue;

		ent.client.pmoveMaxSpeed = 320;
	}
}
