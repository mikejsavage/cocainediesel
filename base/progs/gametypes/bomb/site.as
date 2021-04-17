cBombSite @siteHead = null;
uint siteCount = 0;

class PendingExplosion {
	Vec3 pos;
	int64 time;

	PendingExplosion( Vec3 p, int64 t ) {
		this.pos = p;
		this.time = t;
	}
}

const float PI = 3.14159265f;
const int64 EXPLOSION_COMEDIC_DELAY = 1000;

float max( float a, float b ) {
	return a > b ? a : b;
}

Vec3 random_point_on_hemisphere() {
	float z = random_float01();
	float r = sqrt( max( 0.0f, 1.0f - z * z ) );
	float phi = 2 * PI * random_float01();
	return Vec3( r * cos( phi ), r * sin( phi ), z );
}

class cBombSite
{
	Entity @indicator;

	String letter;

	Entity @hud;

	Vec3[] explosionPoints;
	bool targetsUsed;

	PendingExplosion[] pendingExplosions;
	int numPendingExplosions;
	int numExploded;

	cBombSite @next;

	cBombSite( Entity @ent, int team ) {
		if( siteCount >= MAX_SITES ) {
			G_Print( "Too many bombsites... ignoring\n" );

			return;
		}

		@this.indicator = @ent;

		this.indicator.model = 0;
		this.indicator.solid = SOLID_TRIGGER;
		this.indicator.nextThink = levelTime + 1;
		this.indicator.team = 0;
		this.indicator.linkEntity();

		Vec3 origin = this.indicator.origin;

		this.letter = 'A';
		this.letter[0] += siteCount++;

		@this.hud = @G_SpawnEntity( "hud_bomb_site" );
		this.hud.type = ET_BOMB_SITE;
		this.hud.solid = SOLID_NOT;
		this.hud.origin = origin;
		this.hud.team = team;
		this.hud.svflags = SVF_BROADCAST;
		this.hud.counterNum = letter[0];
		this.hud.linkEntity();

		this.generateExplosionPoints();

		@this.next = @siteHead;
		@siteHead = @this;
	}

	void carrierTouched() {
		bombCarrierCanPlantTime = levelTime;
		if( bombCanPlant() ) {
			Vec3 mins, maxs;
			bombCarrier.getSize( mins, maxs );
			Vec3 velocity = bombCarrier.velocity;

			if( maxs.z < 40 && levelTime - bombActionTime >= 1000 && velocity * velocity < BOMB_MAX_PLANT_SPEED * BOMB_MAX_PLANT_SPEED ) {
				bombStartPlanting( this );
			}
		}
	}

	void explode() {
		numPendingExplosions = random_uniform( SITE_EXPLOSION_POINTS / 2, SITE_EXPLOSION_POINTS );
		numExploded = 0;

		for( int i = 0; i < numPendingExplosions; i++ ) {
			Vec3 point = explosionPoints[ random_uniform( 0, explosionPoints.length() ) ];
			int64 time = int64( ( float( i ) / float( numPendingExplosions - 1 ) ) * SITE_EXPLOSION_MAX_DELAY );
			pendingExplosions[ i ] = PendingExplosion( point, time );
		}
	}

	void stepExplosion() {
		int64 t = levelTime - bombActionTime;

		hide( @bombModel );

		while( numExploded < numPendingExplosions && t >= pendingExplosions[ numExploded ].time ) {
			Entity @ent = @G_SpawnEntity( "func_explosive" );

			ent.origin = pendingExplosions[ numExploded ].pos;
			ent.linkEntity();

			ent.explosionEffect( BOMB_EXPLOSION_EFFECT_RADIUS );

			ent.freeEntity();

			numExploded++;
		}
	}

	void generateExplosionPoints() {
		this.explosionPoints.resize( SITE_EXPLOSION_POINTS );
		this.pendingExplosions.resize( SITE_EXPLOSION_POINTS );

		Vec3 origin = this.indicator.origin;
		origin.z += 96;

		for( int i = 0; i < SITE_EXPLOSION_POINTS; i++ ) {
			Vec3 dir = random_point_on_hemisphere();
			Vec3 end = origin + dir * SITE_EXPLOSION_MAX_DIST;

			Trace trace;
			trace.doTrace( origin, vec3Origin, vec3Origin, end, this.indicator.entNum, MASK_SOLID );

			// pick a random point along the line
			this.explosionPoints[i] = origin + random_float01() * ( trace.endPos - origin );
		}
	}
}

cBombSite @getSiteFromIndicator( Entity @ent ) {
	for( cBombSite @site = @siteHead; @site != null; @site = @site.next ) {
		if( @site.indicator == @ent ) {
			return @site;
		}
	}

	assert( false, "site.as getSiteFromIndicator: couldn't find a site" );

	return null; // shut up compiler
}

void resetBombSites() {
	@siteHead = null;
	siteCount = 0;
}

void bomb_site( Entity @ent ) {
	@ent.think = bomb_site_think;
	@ent.die = shits_fucked;
	cBombSite( @ent, defendingTeam );
}

void shits_fucked( Entity @dont, Entity @care, Entity @bro ) {
	G_Print( "shit is fucked\n" );
}

void bomb_site_think( Entity @ent ) {
	// if AS had static this could be approx 1 bajillion times
	// faster on subsequent calls

	array<Entity @> @triggers = @ent.findTargeting();

	// we are being targeted, never think again
	if( !triggers.empty() ) {
		return;
	}

	ent.nextThink = levelTime + 1;

	if( roundState != RoundState_Round ) {
		return;
	}

	if( bombState != BombState_Carried ) {
		return;
	}

	if( !bombCanPlant() ) {
		return;
	}

	Vec3 origin = ent.origin;
	Vec3 carrierOrigin = bombCarrier.origin;

	if( origin.distance( carrierOrigin ) > BOMB_AUTODROP_DISTANCE ) {
		return;
	}

	origin.z += 96;

	Vec3 center = carrierOrigin + getMiddle( @bombCarrier );

	Trace trace;
	if( !trace.doTrace( origin, vec3Origin, vec3Origin, center, bombCarrier.entNum, MASK_SOLID ) ) {
		// let's plant it

		cBombSite @site = @getSiteFromIndicator( @ent );

		// we know site isn't null but for debugging purposes...
		assert( @site != null, "site.as trigger_capture_area_touch: @site == null" );

		site.carrierTouched();
	}
}

void plant_area( Entity @ent ) {
	@ent.think = plant_area_think;
	@ent.touch = plant_area_touch;
	ent.setupModel(); // set up the brush model
	ent.solid = SOLID_TRIGGER;
	ent.linkEntity();

	ent.nextThink = levelTime + 1000;
}

String @vec3ToString( Vec3 vec ) {
	 return "" + vec.x + " " + vec.y + " " + vec.z;
}

void plant_area_think( Entity @ent ) {
	array<Entity @> @targets = ent.findTargets();
	if( targets.empty() ) {
		G_Print( "trigger_capture_area at " + vec3ToString( ent.origin ) + " has no target, removing...\n" );
		ent.freeEntity();
	}
}

void plant_area_touch( Entity @ent, Entity @other, const Vec3 planeNormal, int surfFlags ) {
	if( @other.client == null ) {
		return;
	}

	if( roundState != RoundState_Round ) {
		return;
	}

	if( bombState != BombState_Carried || @other != @bombCarrier ) {
		return;
	}

	if( match.getState() != MATCH_STATE_PLAYTIME ) {
		return;
	}

	cBombSite @site = null;

	array<Entity @> @targets = ent.findTargets();
	@site = getSiteFromIndicator( targets[0] );
	site.carrierTouched();
}

void misc_capture_area_indicator( Entity @ent ) {
	bomb_site( @ent );
}

void trigger_capture_area( Entity @ent ) {
	plant_area( @ent );
}
