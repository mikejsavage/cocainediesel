/*
   Copyright (C) 2009-2010 Chasseur de bots

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

   See the GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
   */

// TODO: organise this crap
//       maybe move to constants.as

const uint BOMB_MAX_PLANT_SPEED = 50;
const uint BOMB_MAX_PLANT_HEIGHT = 100; // doesn't detect site above that height above ground

const uint BOMB_DROP_RETAKE_DELAY = 1000; // time (ms) after dropping before you can retake it

// jit cries if i use COLOR_RGBA so readability can go suck it
//                                     RED            GREEN          BLUE          ALPHA
const int BOMB_LIGHT_INACTIVE = int( ( 255 << 0 ) | ( 255 << 8 ) | ( 0 << 16 ) | ( 128 << 24 ) ); // yellow
const int BOMB_LIGHT_ARMED    = int( ( 255 << 0 ) | (   0 << 8 ) | ( 0 << 16 ) | ( 128 << 24 ) ); // red

// min cos(ang) between ground and up to plant
// 0.90 gives ~26 degrees max slope
// setting this too low breaks the roofplant @ alley b
const float BOMB_MIN_DOT_GROUND = 0.90f;

const Vec3 VEC_UP( 0, 0, 1 ); // this must have length 1! don't change this unless +z is no longer up...

const float BOMB_ARM_DEFUSE_RADIUS = 32.0f;

const uint BOMB_SPRITE_RESIZE_TIME = 300; // time taken to expand/shrink sprite/decal

const float BOMB_BEEP_FRACTION = 1.0f / 12.0f; // fraction of time left between beeps
const uint BOMB_BEEP_MAX = 5000;               // max time (ms) between beeps
const uint BOMB_BEEP_MIN = 200;                // min time (ms) between beeps

const uint BOMB_HURRYUP_TIME = 12000;

const uint BOMB_AUTODROP_DISTANCE = 400; // distance from indicator to drop (only some maps)

const uint BOMB_THROW_SPEED = 300; // speed at which the bomb is thrown with drop

const uint BOMB_EXPLOSION_EFFECT_RADIUS = 256;

const uint BOMB_DEAD_CAMERA_DIST = 256;

const int INITIAL_ATTACKERS = TEAM_ALPHA;
const int INITIAL_DEFENDERS = TEAM_BETA;

const int SITE_EXPLOSION_POINTS   = 30;

const int SITE_EXPLOSION_MAX_DELAY = 1500; // XXX THIS MUST BE BIGGER THAN BOMB_SPRITE_RESIZE_TIME OR EVERYTHING DIES FIXME?

const float SITE_EXPLOSION_MAX_DIST = 512.0f;

// jit cries if i use const
Vec3 BOMB_MINS( -16, -16, -8 );
Vec3 BOMB_MAXS(  16,  16, 48 ); // same size as player i guess

// cvars
Cvar cvarRoundTime( "g_bomb_roundtime", "60", CVAR_ARCHIVE );
Cvar cvarExplodeTime( "g_bomb_bombtimer", "35", CVAR_ARCHIVE );
Cvar cvarArmTime( "g_bomb_armtime", "1", CVAR_ARCHIVE );
Cvar cvarDefuseTime( "g_bomb_defusetime", "5", CVAR_ARCHIVE );
Cvar cvarEnableCarriers( "g_bomb_carriers", "1", CVAR_ARCHIVE );
Cvar cvarSpawnProtection( "g_bomb_spawnprotection", "3", CVAR_ARCHIVE );

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
	for( int t = TEAM_ALPHA; t < GS_MAX_TEAMS; t++ ) {
		Team @team = @G_GetTeam( t );

		for( int i = 0; @team.ent( i ) != null; i++ ) {
			Entity @ent = @team.ent( i );

			if( ent.team != teamNum )
				continue;

			Client @client = @ent.client;

			if( ent.isGhosting() ) {
				client.setHUDStat( STAT_PROGRESS, 0 );
				client.setHUDStat( STAT_PROGRESS_TYPE, BombProgress_Nothing );
				continue;
			}

			client.setHUDStat( STAT_PROGRESS, percent );
			client.setHUDStat( STAT_PROGRESS_TYPE, type );
		}
	}
}

bool GT_Command( Client @client, const String &cmdString, const String &argsString, int argc ) {
	if( cmdString == "drop" ) {
		if( @client.getEnt() == @bombCarrier && bombState == BombState_Carried ) {
			bombDrop( BombDrop_Normal );
		}

		return true;
	}

	if( cmdString == "carrier" ) {
		if( !cvarEnableCarriers.boolean ) {
			G_PrintMsg( @client.getEnt(), "Bomb carriers are disabled.\n" );

			return true;
		}

		cPlayer @player = @playerFromClient( @client );

		String token = argsString.getToken( 0 );

		if( token.len() != 0 ) {
			if( token.toInt() == 1 ) {
				G_PrintMsg( @client.getEnt(), "You are now a bomb carrier!\n" );
			}
			else {
				G_PrintMsg( @client.getEnt(), "You are no longer a bomb carrier.\n" );
			}
		}
		else {
			if( @client.getEnt() == @bombCarrier ) {
				G_PrintMsg( @client.getEnt(), "You are now a bomb carrier!\n" );
			}
			else {
				G_PrintMsg( @client.getEnt(), "You are no longer a bomb carrier.\n" );
			}
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

	if( cmdString == "cvarinfo" ) {
		GENERIC_CheatVarResponse( @client, cmdString, argsString, argc );
		return true;
	}

	return false;
}

Entity @GT_SelectSpawnPoint( Entity @self ) {
	if( self.team == attackingTeam ) {
		Entity @spawn = GENERIC_SelectBestRandomSpawnPoint( @self, "spawn_offense" );
		if( @spawn != null )
			return spawn;
		return GENERIC_SelectBestRandomSpawnPoint( @self, "team_CTF_betaspawn" );
	}

	Entity @spawn = GENERIC_SelectBestRandomSpawnPoint( @self, "spawn_defense" );
	if( @spawn != null )
		return spawn;
	return GENERIC_SelectBestRandomSpawnPoint( @self, "team_CTF_alphaspawn" );
}

String @teamScoreboardMessage( int t ) {
	Team @team = @G_GetTeam( t );

	String players = "";

	for( int i = 0; @team.ent( i ) != null; i++ ) {
		Entity @ent = @team.ent( i );
		Client @client = @ent.client;

		cPlayer @player = @playerFromClient( @client );

		bool warmup = match.getState() == MATCH_STATE_WARMUP;
		int state = warmup ? ( client.isReady() ? 1 : 0 ) : ( @ent == @bombCarrier ? 1 : 0 );
		int playerId = ent.isGhosting() ? -( ent.playerNum + 1 ) : ent.playerNum;

		players += " " + playerId
			+ " " + client.ping
			+ " " + client.stats.score
			+ " " + client.stats.frags
			+ " " + state;
	}

	return team.stats.score + " " + team.numPlayers + players;
}

String @GT_ScoreboardMessage() {
	return roundCount + " " + teamScoreboardMessage( TEAM_ALPHA ) + " " + teamScoreboardMessage( TEAM_BETA );
}

void GT_updateScore( Client @client ) {
	cPlayer @player = @playerFromClient( @client );
	Stats @stats = @client.stats;

	client.stats.setScore( int( stats.frags * 0.5 + stats.totalDamageGiven * 0.01 ) );
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
		client.setHUDStat( STAT_CAN_CHANGE_LOADOUT, new_team >= TEAM_ALPHA ? 1 : 0 );
	}

	if( new_team != old_team ) {
		if( @ent == @bombCarrier ) {
			bombDrop( BombDrop_Team );
		}

		if( matchState == MATCH_STATE_PLAYTIME ) {
			if( roundState == RoundState_Round ) {
				if( old_team != TEAM_SPECTATOR ) {
					checkPlayersAlive( old_team );
				}
			}
			else if( roundState == RoundState_Pre && new_team != TEAM_SPECTATOR ) {
				// respawn during countdown
				// mark for respawning next frame because
				// respawning this frame doesn't work

				player.dueToSpawn = true;
			}
		}
	}
	else if( roundState == RoundState_Pre ) {
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

	G_ConfigString( MSG_ALIVE_ALPHA, "" + aliveAlpha );
	G_ConfigString( MSG_TOTAL_ALPHA, "" + alphaAliveAtStart );
	G_ConfigString( MSG_ALIVE_BETA,  "" + aliveBeta );
	G_ConfigString( MSG_TOTAL_BETA,  "" + betaAliveAtStart );

	for( int i = 0; i < maxClients; i++ ) {
		Client @client = @G_GetClient( i );

		if( client.state() != CS_SPAWNED ) {
			continue; // don't bother if they're not ingame
		}

		bool can_change_loadout = !client.getEnt().isGhosting() && roundState == RoundState_Pre;

		client.setHUDStat( STAT_CARRYING_BOMB, 0 );
		client.setHUDStat( STAT_CAN_PLANT_BOMB, 0 );
		client.setHUDStat( STAT_CAN_CHANGE_LOADOUT, can_change_loadout ? 1 : 0 );
		client.setHUDStat( STAT_ALPHA_PLAYERS_ALIVE, MSG_ALIVE_ALPHA );
		client.setHUDStat( STAT_ALPHA_PLAYERS_TOTAL, MSG_TOTAL_ALPHA );
		client.setHUDStat( STAT_BETA_PLAYERS_ALIVE, MSG_ALIVE_BETA );
		client.setHUDStat( STAT_BETA_PLAYERS_TOTAL, MSG_TOTAL_BETA );
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
		bombCarrier.client.setHUDStat( STAT_CARRYING_BOMB, 1 );

		// seems like physics only gets run on alternating frames
		int can_plant = bombCarrierCanPlantTime >= levelTime - 50 ? 1 : 0;
		bombCarrier.client.setHUDStat( STAT_CAN_PLANT_BOMB, can_plant );

		bombCarrierLastPos = bombCarrier.origin;
		bombCarrierLastVel = bombCarrier.velocity;
	}

	GENERIC_Think();
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
	gametype.maxPlayersPerTeam = 0;

	gametype.readyAnnouncementEnabled = false;
	gametype.scoreAnnouncementEnabled = true;
	gametype.countdownEnabled = false;
	gametype.matchAbortDisabled = false;
	gametype.shootingDisabled = false;
	gametype.infiniteAmmo = false;
	gametype.canForceModels = true;
	gametype.removeInactivePlayers = true;

	gametype.spawnpointRadius = 256;

	// set spawnsystem type to instant while players join
	for( int t = TEAM_PLAYERS; t < GS_MAX_TEAMS; t++ ) {
		gametype.setTeamSpawnsystem( t, SPAWNSYSTEM_INSTANT, 0, 0, false );
	}

	// add commands
	G_RegisterCommand( "drop" );
	G_RegisterCommand( "carrier" );

	G_RegisterCommand( "gametype" );

	G_RegisterCommand( "gametypemenu" );
	G_RegisterCommand( "gametypemenu2" );
	G_RegisterCommand( "weapselect" );

	mediaInit();

	G_CmdExecute( "exec configs/server/gametypes/bomb.cfg silent" ); // TODO XXX FIXME
}
