enum BombState {
	BombState_Idle,
	BombState_Carried,
	BombState_Dropped,
	BombState_Planting,
	BombState_Planted,
	BombState_Exploding,
}

enum BombDrop {
	BombDrop_Normal, // dropped manually
	BombDrop_Killed, // died
	BombDrop_Team, // changed teams
}

const int BOMB_HUD_OFFSET = 32;

BombState bombState = BombState_Idle;

cBombSite @bombSite;

int64 bombPickTime;
Entity @bombDropper;

// used for time of last action (eg planting) or when the bomb should explode
int64 bombActionTime;

Entity @bombCarrier = null; // also used for the planter
int64 bombCarrierCanPlantTime = -1;
Vec3 bombCarrierLastPos; // so it drops in the right place when they change teams
Vec3 bombCarrierLastVel;

Entity @defuser = null;
uint defuseProgress;

Entity @bombModel;
Entity @bombHud;

void show( Entity @ent ) {
	ent.svflags &= ~SVF_NOCLIENT;
	ent.linkEntity();
}

void hide( Entity @ent ) {
	ent.svflags |= SVF_NOCLIENT;
}

Vec3 getMiddle( Entity @ent ) {
	Vec3 mins, maxs;
	ent.getSize( mins, maxs );
	return 0.5f * ( mins + maxs );
}

void bombModelCreate() {
	@bombModel = @G_SpawnEntity( "bomb" );
	bombModel.type = ET_GENERIC;
	bombModel.setSize( BOMB_MINS, BOMB_MAXS );
	bombModel.solid = SOLID_TRIGGER;
	bombModel.model = modelBomb;
	bombModel.silhouetteColor = uint( 255 << 0 ) | uint( 255 << 8 ) | uint( 255 << 16 ) | uint( 255 << 24 );
	bombModel.svflags |= SVF_BROADCAST;
	@bombModel.touch = bomb_touch;
	@bombModel.stop = bomb_stop;
}

void bombInit() {
	bombModelCreate();

	// don't set ~SVF_NOCLIENT yet
	@bombHud = @G_SpawnEntity( "hud_bomb" );
	bombHud.type = ET_BOMB;
	bombHud.solid = SOLID_NOT;
	bombHud.svflags |= SVF_BROADCAST;

	bombActionTime = -1;
}

void bombPickUp() {
	bombCarrier.effects |= EF_CARRIER;
	bombCarrier.model2 = modelBomb;

	hide( @bombModel );
	hide( @bombHud );

	bombModel.moveType = MOVETYPE_NONE;

	bombState = BombState_Carried;
}

void bombSetCarrier( Entity @ent, bool no_sound ) {
	if( @bombCarrier != null ) {
		bombCarrier.effects &= ~EF_CARRIER;
		bombCarrier.model2 = 0;
	}

	@bombCarrier = @ent;
	bombPickUp();

	Client @client = @bombCarrier.client;
	client.addAward( S_COLOR_GREEN + "You've got the bomb!" );
	if( !no_sound ) {
		G_AnnouncerSound( @client, sndBombTaken, attackingTeam, true, null );
	}
}

void bombDrop( BombDrop drop_reason ) {
	Vec3 start = bombCarrier.origin;
	Vec3 end, velocity;
	setTeamProgress( attackingTeam, 0, BombProgress_Nothing );

	switch( drop_reason ) {
		case BombDrop_Normal:
		case BombDrop_Killed: {
			bombPickTime = levelTime + BOMB_DROP_RETAKE_DELAY;
			@bombDropper = @bombCarrier;

			// throw from the player's eye
			start.z += bombCarrier.viewHeight;

			// aim it up by 10 degrees like nades
			Vec3 angles = bombCarrier.angles;

			Vec3 forward, right, up;
			angles.angleVectors( forward, right, up );

			// put it a little infront of the player
			Vec3 offset( 24.0, 0.0, -16.0 );
			end.x = start.x + forward.x * offset.x;
			end.y = start.y + forward.y * offset.x;
			end.z = start.z + forward.z * offset.x + offset.z;

			velocity = bombCarrier.velocity + forward * 200;
			velocity.z = BOMB_THROW_SPEED;
			if( drop_reason == BombDrop_Killed ) {
				velocity.z *= 0.5;
			}
		} break;

		case BombDrop_Team: {
			@bombDropper = null;

			// current pos/velocity are outdated
			start = end = bombCarrierLastPos;
			velocity = bombCarrierLastVel;
		} break;
	}

	Trace trace;
	trace.doTrace( start, BOMB_MINS, BOMB_MAXS, end, bombCarrier.entNum, MASK_SOLID );

	bombModel.moveType = MOVETYPE_TOSS;
	@bombModel.owner = @bombCarrier;
	bombModel.origin = trace.endPos;
	bombModel.velocity = velocity;
	show( @bombModel );

	bombCarrier.effects &= ~EF_CARRIER;
	bombCarrier.model2 = 0;

	@bombCarrier = null;

	bombState = BombState_Dropped;
}

void bombStartPlanting( cBombSite @site ) {
	@bombSite = @site;

	Vec3 start = bombCarrier.origin;

	Vec3 end = start;
	end.z -= 512;

	Vec3 center = bombCarrier.origin + getMiddle( @bombCarrier );

	// trace to the ground
	// should this be merged with the ground check in canPlantBomb?
	Trace trace;
	trace.doTrace( start, BOMB_MINS, BOMB_MAXS, end, bombCarrier.entNum, MASK_SOLID );

	Vec3 angles = Vec3( 0, random_float01() * 360.0f, 0 );

	// show stuff
	bombModel.origin = trace.endPos;
	bombModel.angles = angles;
	show( @bombModel );

	bombHud.origin = trace.endPos + Vec3( 0, 0, BOMB_HUD_OFFSET );
	bombHud.angles = angles;
	bombHud.svflags |= SVF_ONLYTEAM;
	bombHud.radius = BombDown_Planting;
	show( @bombHud );

	// make carrier look normal
	bombCarrier.effects &= ~EF_CARRIER;
	bombCarrier.model2 = 0;

	bombActionTime = levelTime;
	bombState = BombState_Planting;

	G_Sound( @bombModel, 0, sndPlant );
}

void bombPlanted() {
	bombActionTime = levelTime + int( cvarExplodeTime.value * 1000.0f );

	bombModel.sound = sndFuse;
	bombModel.effects &= ~EF_TEAM_SILHOUETTE;

	// show to defs too
	bombHud.svflags &= ~SVF_ONLYTEAM;

	announce( Announcement_Planted );

	G_CenterPrintMsg( null, "Bomb planted at " + bombSite.letter + "!" );

	@bombCarrier = null;
	defuseProgress = 0;
	bombState = BombState_Planted;
}

void bombDefused() {
	bombModel.sound = 0;

	hide( @bombHud );

	announce( Announcement_Defused );

	bombState = BombState_Idle;

	Client @client = @defuser.client;
	cPlayer @player = @playerFromClient( @client );
	player.defuses++;
	GT_updateScore( @client );

	client.addAward( "Bomb defused!" );
	G_PrintMsg( null, client.name + " defused the bomb!\n" );

	G_Sound( @bombModel, 0, sndFuseExtinguished );

	roundWonBy( defendingTeam );

	@defuser = null;
}

void bombExplode() {
	// do this first else the attackers can score 2 points when the explosion kills everyone
	roundWonBy( attackingTeam );

	hide( @bombHud );

	bombSite.explode();

	bombState = BombState_Exploding;
	@defuser = null;

	match.exploding = true;
	match.explodedAt = levelTime;

	G_Sound( @bombModel, 0, sndComedy );
}

void resetBomb() {
	hide( @bombModel );

	bombModel.effects |= EF_TEAM_SILHOUETTE;

	bombModel.team = attackingTeam;
	bombHud.team = attackingTeam;

	bombState = BombState_Idle;
}

void bombThink() {
	switch( bombState ) {
		case BombState_Dropped: {
			// respawn the bomb if it falls in the void
			if( bombModel.origin.z <= -1024.0f ) {
				bombModel.origin = RandomEntity( "spawn_bomb_attacking" ).origin;
				bombModel.velocity = Vec3();
				bombModel.teleported = true;

				G_PositionedSound( bombModel.origin, 0, sndBombRespawn );
				G_VFX( bombModel.origin, vfxBombRespawn );
			}
		} break;

		case BombState_Planting: {
			if( !entCanSee( bombCarrier, bombModel.origin ) || bombCarrier.origin.distance( bombModel.origin ) > BOMB_ARM_DEFUSE_RADIUS ) {
				setTeamProgress( attackingTeam, 0, BombProgress_Nothing );
				bombPickUp();
				break;
			}

			float frac = float( levelTime - bombActionTime ) / ( cvarArmTime.value * 1000.0f );
			if( frac >= 1.0f ) {
				setTeamProgress( attackingTeam, 0, BombProgress_Nothing );
				bombPlanted();
				break;
			}

			if( frac != 0 ) {
				setTeamProgress( attackingTeam, int( frac * 100.0f ), BombProgress_Planting );
			}
		} break;

		case BombState_Planted: {
			if( @defuser == null )
				@defuser = firstNearbyTeammate( bombModel.origin, defendingTeam );

			if( @defuser != null ) {
				if( defuser.isGhosting() || !entCanSee( defuser, bombModel.origin ) || defuser.origin.distance( bombModel.origin ) > BOMB_ARM_DEFUSE_RADIUS ) {
					@defuser = null;
				}
			}

			if( @defuser == null && defuseProgress > 0 ) {
				defuseProgress = 0;
				setTeamProgress( defendingTeam, 0, BombProgress_Nothing );
			}
			else {
				defuseProgress += frameTime;
				float frac = defuseProgress / ( cvarDefuseTime.value * 1000.0f );
				if( frac >= 1.0f ) {
					bombDefused();
					setTeamProgress( defendingTeam, 100, BombProgress_Defusing );
					break;
				}

				setTeamProgress( defendingTeam, int( frac * 100.0f ), BombProgress_Defusing );
			}

			if( levelTime >= bombActionTime ) {
				bombExplode();
				break;
			}
		} break;
	}
}

// fixes the exploding animation from stopping
void bombPostRoundThink() {
	if( bombState == BombState_Exploding ) {
		bombSite.stepExplosion();
	}
}

Entity @ firstNearbyTeammate( Vec3 origin, int team ) {
	array< Entity @ > @nearby = G_FindInRadius( origin, BOMB_ARM_DEFUSE_RADIUS );
	for( uint i = 0; i < nearby.size(); i++ ) {
		Entity @target = nearby[i];
		if( @target.client == null ) {
			continue;
		}
		if( target.team != team || target.isGhosting() || !entCanSee( target, origin ) ) {
			continue;
		}

		return target;
	}

	return null;
}

bool bombCanPlant() {
	// check carrier is on the ground
	// XXX: old bomb checked if they were < 32 units above ground
	Trace trace;

	Vec3 start = bombCarrier.origin;

	Vec3 end = start;
	end.z -= BOMB_MAX_PLANT_HEIGHT;

	Vec3 mins, maxs;
	bombCarrier.getSize( mins, maxs );

	trace.doTrace( start, mins, maxs, end, bombCarrier.entNum, MASK_SOLID );

	if( trace.startSolid ) {
		return false;
	}

	// check carrier is on level ground
	// trace.planeNormal and VEC_UP are both unit vectors
	// XXX: old bomb had a check that did the same thing in a different way
	//      imo the old way is really bad - it traced a ray down and forwards
	//      but it removes some plant spots (wbomb1 B ramp) and makes some
	//      others easier (shepas A rail, reactors B upper)

	// fast dot product with ( 0, 0, 1 )
	if( trace.planeNormal.z < BOMB_MIN_DOT_GROUND ) {
		return false;
	}

	return true;
}

void bombGiveToRandom() {
	Team @team = @G_GetTeam( attackingTeam );
	int bots = 0;

	for( int i = 0; @team.ent( i ) != null; i++ ) {
		if( ( team.ent( i ).svflags & SVF_FAKECLIENT ) != 0 ) {
			bots++;
		}
	}

	bool all_bots = bots == team.numPlayers;
	int n = all_bots ? team.numPlayers : team.numPlayers - bots;
	int carrier = random_uniform( 0, n );
	int seen = 0;

	for( int i = 0; @team.ent( i ) != null; i++ ) {
		Entity @ent = @team.ent( i );
		Client @client = @ent.client;

		cPlayer @player = @playerFromClient( @client );

		if( all_bots || ( ent.svflags & SVF_FAKECLIENT ) == 0 ) {
			if( seen == carrier ) {
				bombSetCarrier( @ent, true );
				break;
			}

			seen++;
		}
	}
}

bool entCanSee( Entity @ent, Vec3 point ) {
	Vec3 center = ent.origin + getMiddle( @ent );

	Trace trace;
	return !trace.doTrace( center, vec3Origin, vec3Origin, point, ent.entNum, MASK_SOLID );
}

// move the camera around the site?
void bombLookAt( Entity @ent ) {
	Entity @target = @bombSite.indicator;

	array<Entity @> @targets = bombSite.indicator.findTargets();
	for( uint i = 0; i < targets.size(); i++ ) {
		if( targets[i].classname == "func_explosive" ) {
			@target = targets[i];
			break;
		}
	}

	Vec3 center = target.origin + getMiddle( @target );
	center.z -= 50; // this tilts the camera down (not by 50 degrees...)

	Vec3 bombOrigin = bombModel.origin;

	float diff = center.z - bombOrigin.z;

	if( diff > 8 ) {
		bombOrigin.z += diff / 2;
	}

	Vec3 dir = bombOrigin - center;

	float dist = dir.length();

	Vec3 end = center + dir.normalize() * ( dist + BOMB_DEAD_CAMERA_DIST );

	Trace trace;
	bool didHit = trace.doTrace( bombOrigin, vec3Origin, vec3Origin, end, -1, MASK_SOLID );

	Vec3 origin = trace.endPos;

	if( trace.fraction != 1 ) {
		origin += 8 * trace.planeNormal;
	}

	Vec3 viewDir = center - origin;
	Vec3 angles = viewDir.toAngles();

	ent.moveType = MOVETYPE_NONE;
	ent.origin = origin;
	ent.angles = angles;

	ent.linkEntity();
}

// ent stuff

void bomb_touch( Entity @ent, Entity @other, const Vec3 planeNormal, int surfFlags ) {
	if( match.getState() != MATCH_STATE_PLAYTIME ) {
		return;
	}

	if( bombState != BombState_Dropped ) {
		return;
	}

	if( @other.client == null ) {
		return;
	}

	if( other.team != attackingTeam ) {
		return;
	}

	// dead players can't carry
	if( other.isGhosting() || other.health < 0 ) {
		return;
	}

	// did this guy drop it recently?
	if( levelTime < bombPickTime && @other == @bombDropper ) {
		return;
	}

	bombSetCarrier( @other, false );
}

void bomb_stop( Entity @ent ) {
	if( bombState == BombState_Dropped ) {
		bombHud.origin = bombModel.origin + Vec3( 0, 0, BOMB_HUD_OFFSET );
		bombHud.svflags |= SVF_ONLYTEAM;
		bombHud.radius = BombDown_Dropped;
		show( @bombHud );
	}
}
