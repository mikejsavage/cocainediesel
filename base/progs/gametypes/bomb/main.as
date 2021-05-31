const uint BOMB_MAX_PLANT_SPEED = 50;
const uint BOMB_MAX_PLANT_HEIGHT = 100; // doesn't detect site above that height above ground

const uint BOMB_DROP_RETAKE_DELAY = 1000; // time (ms) after dropping before you can retake it

// min cos(ang) between ground and up to plant
// 0.90 gives ~26 degrees max slope
// setting this too low breaks the roofplant @ alley b
const float BOMB_MIN_DOT_GROUND = 0.90f;

const Vec3 VEC_UP( 0, 0, 1 ); // this must have length 1! don't change this unless +z is no longer up...

const float BOMB_ARM_DEFUSE_RADIUS = 36.0f;

const uint BOMB_AUTODROP_DISTANCE = 400; // distance from indicator to drop (only some maps)

const uint BOMB_THROW_SPEED = 550; // speed at which the bomb is thrown with drop

const uint BOMB_EXPLOSION_EFFECT_RADIUS = 256;

const uint BOMB_DEAD_CAMERA_DIST = 256;

const int INITIAL_ATTACKERS = TEAM_ALPHA;
const int INITIAL_DEFENDERS = TEAM_BETA;

const int SITE_EXPLOSION_POINTS = 10;

const int SITE_EXPLOSION_MAX_DELAY = 1000; // XXX THIS MUST BE BIGGER THAN BOMB_SPRITE_RESIZE_TIME OR EVERYTHING DIES FIXME?

const float SITE_EXPLOSION_MAX_DIST = 512.0f;

// jit cries if i use const
Vec3 BOMB_MINS( -16, -16, -16 );
Vec3 BOMB_MAXS(  16,  16, 48 ); // same size as player i guess

// cvars
Cvar cvarRoundTime( "g_bomb_roundtime", "61", CVAR_ARCHIVE ); //So round starts with 1:00 and not 0:59
Cvar cvarExplodeTime( "g_bomb_bombtimer", "30", CVAR_ARCHIVE );
Cvar cvarArmTime( "g_bomb_armtime", "1", CVAR_ARCHIVE );
Cvar cvarDefuseTime( "g_bomb_defusetime", "4", CVAR_ARCHIVE );

// read from this later
Cvar cvarScoreLimit( "g_scorelimit", "10", CVAR_ARCHIVE );

const uint MAX_SITES = 26;

const int COUNTDOWN_MAX = 6; // was 4, but this gives people more time to change weapons

// this should really kill the program
// but i'm mostly using it as an indicator that it's about to die anyway
void assert( const bool test, const String msg ) {
	if( !test ) {
		G_Print( S_COLOR_RED + "assert failed: " + msg + "\n" );
	}
}

uint min( uint a, uint b ) {
	return a < b ? a : b;
}

float min( float a, float b ) {
	return a < b ? a : b;
}

void setTeamProgress( int teamNum, int percent, BombProgress type ) {
	Team @team = @G_GetTeam( teamNum );

	for( int i = 0; @team.ent( i ) != null; i++ ) {
		Entity @ent = @team.ent( i );

		Client @client = @ent.client;

		if( ent.isGhosting() ) {
			client.progress = 0;
			client.progressType = BombProgress_Nothing;
			continue;
		}

		client.progress = percent;
		client.progressType = type;
	}
}

bool GT_Command( Client @client, const String &cmdString, const String &argsString, int argc ) {
	if( cmdString == "drop" ) {
		if( @client.getEnt() == @bombCarrier && bombState == BombState_Carried ) {
			bombDrop( BombDrop_Normal );
		}

		return true;
	}

	if( cmdString == "gametypemenu" ) {
		playerFromClient( @client ).showShop();
		return true;
	}

	if( cmdString == "weapselect" ) {
		playerFromClient( @client ).setLoadout( argsString );
		return true;
	}

	return false;
}

void spawn_gladiator( Entity @ent ) { }

Entity @GT_SelectSpawnPoint( Entity @self ) {
	// loading individual gladiator arenas loads bomb gt, so prioritise gladi spawns
	Entity @gladi_spawn = RandomEntity( "spawn_gladiator" );
	if( @gladi_spawn != null )
		return gladi_spawn;

	if( self.team == attackingTeam ) {
		Entity @spawn = RandomEntity( "spawn_bomb_attacking" );
		if( @spawn != null )
			return spawn;
		return RandomEntity( "team_CTF_betaspawn" );
	}

	Entity @spawn = RandomEntity( "spawn_bomb_defending" );
	if( @spawn != null )
		return spawn;
	return RandomEntity( "team_CTF_alphaspawn" );
}

void GT_updateScore( Client @client ) {
	cPlayer @player = @playerFromClient( @client );
	Stats @stats = @client.stats;

	stats.setScore( int( stats.kills * 0.5 + stats.totalDamageGiven * 0.01 ) );
}

// Some game actions trigger score events. These are events not related to killing
// oponents, like capturing a flag
void GT_ScoreEvent( Client @client, const String &score_event, const String &args ) {
	// XXX: i think this can be called but if the client then
	//      doesn't connect (ie hits escape/crashes) then the
	//      disconnect score event doesn't fire, which leaves
	//      us with a teeny tiny bit of wasted memory
	if( score_event == "connect" ) {
		// why not
		cPlayer( @client );

		return;
	}

	if( score_event == "dmg" ) {
		// this does happen when you shoot corpses or something
		if( @client == null ) {
			return;
		}

		GT_updateScore( @client );

		return;
	}

	if( score_event == "kill" ) {
		Entity @attacker = null;

		if( @client != null ) {
			@attacker = @client.getEnt();

			GT_updateScore( @client );
		}

		Entity @victim = @G_GetEntity( args.getToken( 0 ).toInt() );
		Entity @inflictor = @G_GetEntity( args.getToken( 1 ).toInt() );

		playerKilled( @victim, @attacker, @inflictor );

		return;
	}

	if( score_event == "disconnect" ) {
		// clean up
		@players[client.playerNum] = null;

		return;
	}
}

// a player is being respawned. This can happen from several ways, as dying, changing team,
// being moved to ghost state, be placed in respawn queue, being spawned from spawn queue, etc
void GT_PlayerRespawn( Entity @ent, int old_team, int new_team ) {
	Client @client = @ent.client;
	cPlayer @player = @playerFromClient( @client );

	client.pmoveFeatures = client.pmoveFeatures | PMFEAT_TEAMGHOST;

	int matchState = match.getState();

	if( matchState <= MATCH_STATE_WARMUP ) {
		client.canChangeLoadout = new_team >= TEAM_ALPHA;
	}

	if( new_team != old_team ) {
		if( @ent == @bombCarrier ) {
			bombDrop( BombDrop_Team );
		}

		if( matchState == MATCH_STATE_PLAYTIME ) {
			if( match.roundState == RoundState_Round ) {
				if( old_team != TEAM_SPECTATOR && !ent.isGhosting() ) {
					checkPlayersAlive( old_team );
				}
			}
			else if( match.roundState == RoundState_Countdown && new_team != TEAM_SPECTATOR ) {
				// respawn during countdown
				// mark for respawning next frame because
				// respawning this frame doesn't work

				player.dueToSpawn = true;
			}
		}
	}
	else if( match.roundState == RoundState_Countdown ) {
		disableMovementFor( @client );
	}

	if( ent.isGhosting() ) {
		ent.svflags &= ~SVF_FORCETEAM;
		return;
	}

	player.giveInventory();

	ent.svflags |= SVF_FORCETEAM;

	if( match.getState() == MATCH_STATE_WARMUP ) {
		ent.respawnEffect();
	}
}

// Thinking function. Called each frame
void GT_ThinkRules() {
	// XXX: old bomb would let the current round finish before doing this
	if( match.timeLimitHit() ) {
		match.launchState( match.getState() + 1 );
		G_ClearCenterPrint( null );
	}

	for( int t = 0; t < GS_MAX_TEAMS; t++ ) {
		Team @team = @G_GetTeam( t );

		for( int i = 0; @team.ent( i ) != null; i++ ) {
			Client @client = @team.ent( i ).client;
			cPlayer @player = @playerFromClient( @client );

			// this should only happen when match state is playtime
			// but i put this up here since i'm calling playerFromClient
			if( player.dueToSpawn ) {
				client.respawn( false );

				player.dueToSpawn = false;

				continue;
			}
		}
	}

	if( match.getState() < MATCH_STATE_PLAYTIME ) {
		return;
	}

	roundThink();

	uint aliveAlpha = playersAliveOnTeam( TEAM_ALPHA );
	uint aliveBeta  = playersAliveOnTeam( TEAM_BETA );

	match.alphaPlayersAlive = aliveAlpha;
	match.betaPlayersAlive = aliveBeta;

	for( int i = 0; i < maxClients; i++ ) {
		Client @client = @G_GetClient( i );

		if( client.state() != CS_SPAWNED ) {
			continue; // don't bother if they're not ingame
		}

		client.canChangeLoadout = !client.getEnt().isGhosting() && match.roundState == RoundState_Countdown;
		client.carryingBomb = false;
		client.canPlant = false;
	}

	if( bombState == BombState_Planted ) {
		uint aliveOff = TEAM_ALPHA == attackingTeam ? aliveAlpha : aliveBeta;

		if( aliveOff == 0 ) {
			Team @team = @G_GetTeam( attackingTeam );

			for( int i = 0; @team.ent( i ) != null; i++ ) {
				bombLookAt( @team.ent( i ) );
			}
		}
	}

	if( bombState == BombState_Carried ) {
		bombCarrier.client.carryingBomb = true;

		// seems like physics only gets run on alternating frames
		bombCarrier.client.canPlant = bombCarrierCanPlantTime >= levelTime - 50;

		bombCarrierLastPos = bombCarrier.origin;
		bombCarrierLastVel = bombCarrier.velocity;
	}

	GENERIC_UpdateMatchScore();
}

// The game has detected the end of the match state, but it
// doesn't advance it before calling this function.
// This function must give permission to move into the next
// state by returning true.
bool GT_MatchStateFinished( int incomingMatchState ) {
	if( match.getState() <= MATCH_STATE_WARMUP && incomingMatchState > MATCH_STATE_WARMUP && incomingMatchState < MATCH_STATE_POSTMATCH ) {
		match.startAutorecord();
	}

	if( match.getState() == MATCH_STATE_POSTMATCH ) {
		match.stopAutorecord();
	}

	return true;
}

// the match state has just moved into a new state. Here is the
void GT_MatchStateStarted() {
	switch( match.getState() ) {
		case MATCH_STATE_WARMUP:
			GENERIC_SetUpWarmup();

			attackingTeam = INITIAL_ATTACKERS;
			defendingTeam = INITIAL_DEFENDERS;

			for( int t = TEAM_PLAYERS; t < GS_MAX_TEAMS; t++ ) {
				gametype.setTeamSpawnsystem( t, SPAWNSYSTEM_INSTANT, 0, 0, false );
			}

			break;

		case MATCH_STATE_COUNTDOWN:
			// XXX: old bomb had its own function to do pretty much
			//      exactly the same thing
			//      the only difference i can see is that bomb's
			//      didn't respawn all items, but that doesn't
			//      matter cause there aren't any
			GENERIC_SetUpCountdown();

			break;

		case MATCH_STATE_PLAYTIME:
			newGame();

			break;

		case MATCH_STATE_POSTMATCH:
			endGame();

			break;

		default:
			// do nothing

			break;
	}
}

// the gametype is shutting down cause of a match restart or map change
void GT_Shutdown() {
}

// The map entities have just been spawned. The level is initialized for
// playing, but nothing has yet started.
void GT_SpawnGametype() {
	bombInit();

	playersInit();
}

// Important: This function is called before any entity is spawned, and
// spawning entities from it is forbidden. ifyou want to make any entity
// spawning at initialization do it in GT_SpawnGametype, which is called
// right after the map entities spawning.
void GT_InitGametype() {
	gametype.isTeamBased = true;
	gametype.isRace = false;
	gametype.hasChallengersQueue = false;

	gametype.readyAnnouncementEnabled = false;
	gametype.scoreAnnouncementEnabled = true;
	gametype.countdownEnabled = false;
	gametype.matchAbortDisabled = false;
	gametype.shootingDisabled = false;
	gametype.removeInactivePlayers = true;

	gametype.spawnpointRadius = 256;

	// set spawnsystem type to instant while players join
	for( int t = TEAM_PLAYERS; t < GS_MAX_TEAMS; t++ ) {
		gametype.setTeamSpawnsystem( t, SPAWNSYSTEM_INSTANT, 0, 0, false );
	}

	G_RegisterCommand( "drop" );

	G_RegisterCommand( "gametypemenu" );
	G_RegisterCommand( "weapselect" );

	mediaInit();
}
