void MOD_knockback_init()
{
	Cvar g_knockback_scale( "g_knockback_scale", "1.0", 0 );
	g_knockback_scale.set("4.0");
}

void MOD_knockback_stop()
{
	Cvar g_knockback_scale( "g_knockback_scale", "1.0", 0 );
	g_knockback_scale.reset();
}

//------------------------

void MOD_tripleshot_think()
{
	if ( randWeap == WEAP_LASERGUN )
		return;
	float spread = 8.0f;

	for ( int i = 0; i < numEntities; i++ )
	{
		Entity@ ent = @G_GetEntity(i);
		if ( ent.type == ET_ROCKET && ent.random <= 1 )
		{
			Entity@ clientent = @G_GetEntity(ent.ownerNum);
			if ( @clientent == null )
				continue;
			ent.random = 100;


			//Entity @G_FireRocket( const Vec3 &in origin, const Vec3 &in angles, int speed, int radius, int damage, int knockback, int stun, Entity @owner );
			Entity@ rocket = @G_FireRocket( ent.origin, ent.angles - Vec3(0,spread,0), 1150, 125, 80, 100, 1250, clientent );
			rocket.random = 100;
			@rocket = 		 @G_FireRocket( ent.origin, ent.angles + Vec3(0,spread,0), 1150, 125, 80, 100, 1250, clientent );
			rocket.random = 100;
		}
		if ( ent.type == ET_GRENADE && ent.random <= 1 )
		{
			Entity@ clientent = @ent.owner;
			if ( @clientent.client == null )
				continue;
			ent.random = 100;

			//Entity @G_FireGrenade( const Vec3 &in origin, const Vec3 &in angles, int speed, int radius, int damage, int knockback, int stun, Entity @owner );
			Entity@ grenade = @G_FireGrenade( ent.origin, clientent.angles - Vec3(0,spread,0), 1000, 125, 80, 100, 1250, clientent );
			grenade.random = 100;
			@grenade = 		  @G_FireGrenade( ent.origin, clientent.angles + Vec3(0,spread,0), 1000, 125, 80, 100, 1250, clientent );
			grenade.random = 100;
		}
		if ( ent.type == ET_BLASTER && ent.random <= 1 )
		{
			Entity@ clientent = @G_GetEntity(ent.ownerNum);
			if ( @clientent.client == null )
				continue;
			ent.random = 100;

			//Entity @G_FireBlast( const Vec3 &in origin, const Vec3 &in angles, int speed, int radius, int damage, int knockback, int stun, Entity @owner );
			Entity@ blast = @G_FireBlast( ent.origin, clientent.angles - Vec3(0,spread,0), 3000, 80, 35, 90, 0, clientent );
			blast.random = 100;
			@blast = 		  @G_FireBlast( ent.origin, clientent.angles + Vec3(0,spread,0), 3000, 80, 35, 90, 0, clientent );
			blast.random = 100;
		}
		if ( ent.type == ET_PLASMA && ent.random <= 1 )
		{
			Entity@ clientent = @G_GetEntity(ent.ownerNum);
			if ( @clientent.client == null )
				continue;
			ent.random = 100;

			//Entity @G_FirePlasma( const Vec3 &in origin, const Vec3 &in angles, int speed, int radius, int damage, int knockback, int stun, Entity @owner );
			Entity@ plasma = @G_FirePlasma( ent.origin, clientent.angles - Vec3(0,spread,0), 2500, 80, 15, 20, 0, clientent );
			plasma.random = 100;
			@plasma = 		  @G_FirePlasma( ent.origin, clientent.angles + Vec3(0,spread,0), 2500, 80, 15, 20, 0, clientent );
			plasma.random = 100;
		}
		if ( ent.type == ET_EVENT )
		{
			for ( int j = 0; j < maxClients; j++ )
			{
				Client@ client = G_GetClient(j);
				if ( @client == null )
					continue;
				Entity@ clientent = @client.getEnt();
				if ( clientent.isGhosting() )
					continue;

				if ( ent.origin.distance( clientent.origin ) <= 31.0 // why?
				    && int(ent.svflags) == SVF_TRANSMITORIGIN2
				    && ( client.weapon == WEAP_INSTAGUN || client.weapon == WEAP_ELECTROBOLT )
				    && ent.random <= 0 )
				{
					ent.random = 100;

					//Entity @G_FireWeakBolt( const Vec3 &in origin, const Vec3 &in angles, int speed, int damage, int knockback, int stun, Entity @owner );
					Entity @bolt = @G_FireWeakBolt( ent.origin, clientent.angles - Vec3(0,spread,0), 9001, 75, 80, 1000, clientent );
					@bolt = @G_FireWeakBolt( ent.origin, clientent.angles + Vec3(0,spread,0), 9001, 75, 80, 1000, clientent );
				}
			}

			if ( ent.weapon == WEAP_MACHINEGUN && ent.random <= 1 )
			{
				ent.random = 100;
				Entity@ clientent = @G_GetEntity(ent.ownerNum);
				if ( @clientent == null )
					continue;
				//void G_FireBullet( const Vec3 &in origin, const Vec3 &in angles, int range, int spread, int damage, int knockback, int stun, Entity @owner );
				G_FireBullet( ent.origin, clientent.angles - Vec3(0,spread,0), 5096, 10, 11, 10, 50, null );
				G_FireBullet( ent.origin, clientent.angles + Vec3(0,spread,0), 5096, 10, 11, 10, 50, null );
				// TODO: find a way to have client as owner of bullet.
			}

			if ( ent.weapon == WEAP_RIOTGUN && ent.random <= 1 )
			{
				ent.random = 100;
				Entity@ clientent = @G_GetEntity(ent.ownerNum);
				if ( @clientent == null )
					continue;
				//void G_FireRiotgun( const Vec3 &in origin, const Vec3 &in angles, int range, int spread, int count, int damage, int knockback, int stun, Entity @owner );
				G_FireRiotgun( ent.origin, clientent.angles - Vec3(0,spread,0), 5096, 160, 20, 5, 7, 85, null );
				G_FireRiotgun( ent.origin, clientent.angles + Vec3(0,spread,0), 5096, 160, 20, 5, 7, 85, null );
				// TODO: find a way to have client as owner of bullet.
			}
		}

		// TODO: no lasergun
	}
}


//------------------------

void MOD_ricochet_think()
{
	int bounces = 3;
	for ( int i = 0; i < numEntities; i++ )
	{
		Entity@ ent = @G_GetEntity(i);
		if ( ent.type == ET_ROCKET || ent.type == ET_BLASTER || ent.type == ET_PLASMA && ent.random >= 0 )
		{
			Trace endpoint;
			endpoint.doTrace( ent.origin, Vec3(), Vec3(), ent.origin + ent.velocity * (float(frameTime)/1000.0f), -1, MASK_SHOT);
			if ( endpoint.fraction < 1.0 )
			{
				Vec3 fwd, right, up;
				Vec3 vel = ent.velocity;
				float dot = vel * endpoint.planeNormal;
				Vec3 R = -2 * dot * endpoint.planeNormal + vel;
				Vec3 angles = R.toAngles();
				Entity@ clientent = @G_GetEntity(ent.ownerNum);

				//Entity @G_FireRocket( const Vec3 &in origin, const Vec3 &in angles, int speed, int radius, int damage, int knockback, int stun, Entity @owner );
				Entity@ projectile;
				if ( ent.type == ET_ROCKET )
					@projectile = @G_FireRocket( ent.origin, angles, 1150, 125, 80, 100, 1250, clientent );
				if ( ent.type == ET_BLASTER )
					@projectile = @G_FireBlast( ent.origin, angles, 3000, 80, 35, 90, 0, clientent );
				if ( ent.type == ET_PLASMA )
					@projectile = @G_FirePlasma( ent.origin, angles, 2500, 80, 15, 20, 0, clientent );

				if ( ent.random == 0 )
				{
					projectile.random = bounces;
					ent.random = -1;
				}
				else
				{
					projectile.random = ent.random - 1;
					if ( projectile.random == 1 )
						projectile.random = -1;
					ent.random = -1;
				}
			}
		}
	}
}
