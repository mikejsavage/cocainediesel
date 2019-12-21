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

const int DA_ROUNDSTATE_NONE = 0;
const int DA_ROUNDSTATE_PREROUND = 1;
const int DA_ROUNDSTATE_ROUND = 2;
const int DA_ROUNDSTATE_ROUNDFINISHED = 3;
const int DA_ROUNDSTATE_POSTROUND = 4;

const int CountdownSeconds = 4;
const int CountdownNumSwitches = 20;
const float CountdownInitialSwitchDelay = 0.1;

const int MAX_HEALTH = 166;

int[] endMatchSounds;

int crownModel;

int max( int a, int b ) {
	return a > b ? a : b;
}

class cDARound
{
	int state;
	int numRounds;
	int64 roundStateStartTime;
	int64 roundStateEndTime;
	int countDown;
	int[] daChallengersQueue;
	Entity @alphaSpawn;
	Entity @betaSpawn;
	Client @roundWinner;
	Client @roundChallenger;
	Client@[] roundChallengers;
	Client@[] roundLosers;

	cDARound()
	{
		this.state = DA_ROUNDSTATE_NONE;
		this.numRounds = 0;
		this.roundStateStartTime = 0;
		this.countDown = 0;
		@this.alphaSpawn = null;
		@this.betaSpawn = null;
		@this.roundWinner = null;
		@this.roundChallenger = null;
	}

	~cDARound() {}

	void init()
	{
		this.clearChallengersQueue();
	}

	void clearChallengersQueue()
	{
		if ( this.daChallengersQueue.length() != uint( maxClients ) )
			this.daChallengersQueue.resize( maxClients );

		for ( int i = 0; i < maxClients; i++ )
			this.daChallengersQueue[i] = -1;
	}

	void challengersQueueAddPlayer( Client @client )
	{
		if ( @client == null )
			return;

		// check for already added
		for ( int i = 0; i < maxClients; i++ )
		{
			if ( this.daChallengersQueue[i] == client.playerNum )
				return;
		}

		for ( int i = 0; i < maxClients; i++ )
		{
			if ( this.daChallengersQueue[i] < 0 || this.daChallengersQueue[i] >= maxClients )
			{
				this.daChallengersQueue[i] = client.playerNum;
				break;
			}
		}
	}

	bool challengersQueueRemovePlayer( Client @client )
	{
		if ( @client == null )
			return false;

		for ( int i = 0; i < maxClients; i++ )
		{
			if ( this.daChallengersQueue[i] == client.playerNum )
			{
				int j;
				for ( j = i + 1; j < maxClients; j++ )
				{
					this.daChallengersQueue[j - 1] = this.daChallengersQueue[j];
					if ( daChallengersQueue[j] == -1 )
						break;
				}

				this.daChallengersQueue[j] = -1;
				return true;
			}
		}

		return false;
	}

	Client @challengersQueueGetNextPlayer()
	{
		Client @client = @G_GetClient( this.daChallengersQueue[0] );

		if ( @client != null )
		{
			this.challengersQueueRemovePlayer( client );
		}

		return client;
	}

	bool isChallenger( Client@ client )
	{
		for ( uint i = 0; i < this.roundChallengers.size(); i++ )
		{
			if ( @client == @this.roundChallengers[i] )
				return true;
		}
		return false;
	}

	void removeChallenger( Client@ client )
	{
		int index = -1;
		for ( uint i = 0; i < this.roundChallengers.size(); i++ )
		{
			if ( @client == @this.roundChallengers[i] )
			{
				index = i;
				break;
			}
		}

		if ( index != -1 )
			this.roundChallengers.erase(index);
	}

	bool isLoser( Client@ client )
	{
		for ( uint i = 0; i < this.roundLosers.size(); i++ )
		{
			if ( @client == @this.roundLosers[i] )
				return true;
		}
		return false;
	}


	void addLoser( Client@ client )
	{
		this.removeChallenger( client );
		this.roundLosers.push_back(client);
	}

	void removeLoser( Client@ client )
	{
		int index = -1;
		for ( uint i = 0; i < this.roundLosers.size(); i++ )
		{
			if ( @client == @this.roundLosers[i] )
			{
				index = i;
				break;
			}
		}

		if ( index != -1 )
			this.roundLosers.erase(index);
	}

	void playerTeamChanged( Client @client, int new_team )
	{
		if ( new_team != TEAM_PLAYERS )
		{
			this.challengersQueueRemovePlayer( client );

			if ( this.isChallenger( client ) )
				this.removeChallenger( client );

			if ( this.isLoser( client ) )
				this.removeLoser( client );

			if ( this.state != DA_ROUNDSTATE_NONE )
			{
				if ( this.roundChallengers.size() <= 1 )
					this.newRoundState( DA_ROUNDSTATE_ROUNDFINISHED );
			}
		}
		else if ( new_team == TEAM_PLAYERS )
		{
			this.challengersQueueAddPlayer( client );
		}
	}

	void roundAnnouncementPrint( String &string )
	{
		if ( string.len() <= 0 )
			return;

		for ( uint i = 0; i < this.roundChallengers.size(); i++ )
		{
			G_CenterPrintMsg( this.roundChallengers[i].getEnt(), string );
		}

		// also add it to spectators who are not in chasecam

		Team @team = @G_GetTeam( TEAM_SPECTATOR );
		Entity @ent;

		// respawn all clients inside the playing teams
		for ( int i = 0; @team.ent( i ) != null; i++ )
		{
			@ent = @team.ent( i );
			if ( !ent.isGhosting() )
				G_CenterPrintMsg( ent, string );

		}
	}

	void newGame()
	{
		gametype.readyAnnouncementEnabled = false;
		gametype.scoreAnnouncementEnabled = false;
		gametype.countdownEnabled = false;

		// set spawnsystem type to not respawn the players when they die
		for ( int team = TEAM_PLAYERS; team < GS_MAX_TEAMS; team++ )
			gametype.setTeamSpawnsystem( team, SPAWNSYSTEM_HOLD, 0, 0, true );

		// clear scores

		Entity @ent;
		Team @team;

		for ( int i = TEAM_PLAYERS; i < GS_MAX_TEAMS; i++ )
		{
			@team = @G_GetTeam( i );
			team.stats.clear();

			// respawn all clients inside the playing teams
			for ( int j = 0; @team.ent( j ) != null; j++ )
			{
				@ent = @team.ent( j );
				ent.client.stats.clear(); // clear player scores & stats
				ent.client.respawn( true );
			}
		}

		this.numRounds = 0;
		this.newRound();
	}

	void endGame()
	{
		this.newRoundState( DA_ROUNDSTATE_NONE );

		if ( @this.roundWinner != null )
		{
			Cvar scoreLimit( "g_scorelimit", "", 0 );
			if ( this.roundWinner.stats.score == scoreLimit.integer )
			{
				this.roundAnnouncementPrint( S_COLOR_WHITE + this.roundWinner.name + S_COLOR_WHITE + " is a true gladiator!" );
			}
		}

		GLADIATOR_SetUpEndMatch();
	}

	void GLADIATOR_SetUpEndMatch()
	{
		Client @client;

		gametype.readyAnnouncementEnabled = false;
		gametype.scoreAnnouncementEnabled = false;
		gametype.countdownEnabled = false;

		for ( int i = 0; i < maxClients; i++ )
		{
			@client = @G_GetClient( i );

			if ( client.state() >= CS_SPAWNED ) {
				client.respawn( true ); // ghost them all
			}
		}

		GENERIC_UpdateMatchScore();

		int soundIndex = endMatchSounds[random_uniform(0,endMatchSounds.size())];
		G_AnnouncerSound( null, soundIndex, GS_MAX_TEAMS, true, null );
	}

	void newRound()
	{
		G_RemoveDeadBodies();
		G_RemoveAllProjectiles();
		G_ResetLevel();

		this.newRoundState( DA_ROUNDSTATE_PREROUND );
		this.numRounds++;
	}

	void newRoundState( int newState )
	{

		if ( newState > DA_ROUNDSTATE_POSTROUND )
		{
			this.newRound();
			return;
		}

		this.state = newState;
		this.roundStateStartTime = levelTime;

		switch ( this.state )
		{
			case DA_ROUNDSTATE_NONE:

				this.roundStateEndTime = 0;
				this.countDown = 0;
				break;

			case DA_ROUNDSTATE_PREROUND:
				{
					this.roundStateEndTime = levelTime + CountdownSeconds * 1000;
					this.countDown = 4;

					// respawn everyone and disable shooting
					gametype.shootingDisabled = true;
					gametype.removeInactivePlayers = false;

					Entity @ent;
					Team @team;

					@team = @G_GetTeam( TEAM_PLAYERS );

					this.roundLosers.resize(0);

					// pick 2 to 4 players from queue
					for ( int i = 0; this.roundChallengers.size() < 4 && this.roundChallengers.size() < uint(team.numPlayers); i++ )
					{
						this.roundChallengers.push_back( @this.challengersQueueGetNextPlayer() );
					}

					// find topscore
					int topscore = 0;
					for ( int j = 0; @team.ent( j ) != null; j++ ) {
						Client @client = @team.ent( j ).client;
						topscore = max( client.stats.score, topscore );
					}

					// respawn all clients inside the playing teams
					for ( int j = 0; @team.ent( j ) != null; j++ )
					{
						@ent = @team.ent( j );
						if( this.isChallenger( ent.client ) ) {
							ent.client.respawn( false );
							if( ent.client.stats.score == topscore ) {
								ent.modelindex2 = crownModel;
								ent.effects |= EF_HAT;
							}
							else {
								ent.modelindex2 = 0;
								ent.effects &= ~EF_HAT;
							}
						}
						else {
							ent.client.respawn( true );
							ent.client.chaseCam( null, false );
							ent.client.chaseActive = true;
						}
					}

					selected_spawn = false;

					DoSpinner();

					// generate vs string
					String vs_string = S_COLOR_WHITE + this.roundChallengers[0].name;
					for ( uint i = 1; i < this.roundChallengers.size(); i++ )
					{
						vs_string += S_COLOR_WHITE + " vs. " + this.roundChallengers[i].name;
					}

					this.roundAnnouncementPrint( vs_string );

					// check for match point
					int limit = Cvar( "g_scorelimit", "10", 0 ).integer;

					RoundType type = RoundType_Normal;
					for( int i = 0; @team.ent( i ) != null; i++ ) {
						Client @client = @team.ent( i ).client;
						if( client.stats.score == limit - 1 ) {
							type = RoundType_MatchPoint;
							break;
						}
					}

					for( int i = 0; @team.ent( i ) != null; i++ ) {
						Client @client = @team.ent( i ).client;
						client.setHUDStat( STAT_ROUND_TYPE, int( type ) );
					}
				}
				break;

			case DA_ROUNDSTATE_ROUND:
				{
					gametype.shootingDisabled = false;
					gametype.removeInactivePlayers = true;
					this.countDown = 0;
					this.roundStateEndTime = 0;
					int soundIndex = G_SoundIndex( "sounds/gladiator/fight" );
					G_AnnouncerSound( null, soundIndex, GS_MAX_TEAMS, false, null );
					G_CenterPrintMsg( null, 'FIGHT!');
				}
				break;

			case DA_ROUNDSTATE_ROUNDFINISHED:

				gametype.shootingDisabled = false;
				this.roundStateEndTime = levelTime + 1500;
				this.countDown = 0;
				break;

			case DA_ROUNDSTATE_POSTROUND:
				{
					this.roundStateEndTime = levelTime + 1000;

					// add score to round-winning player
					Client @winner = null;
					Client @loser = null;

					// get last remaining challenger if any remain
					if ( this.roundChallengers.size() > 0 )
						@winner = @this.roundChallengers[0];

					// if we didn't find a winner, it was a draw round
					if ( @winner == null )
					{
						int soundIndex;
						this.roundAnnouncementPrint( S_COLOR_WHITE + "Wow your terrible" );

						soundIndex = G_SoundIndex( "sounds/gladiator/wowyourterrible" );
						G_AnnouncerSound( null, soundIndex, GS_MAX_TEAMS, false, null );
					}
					else
					{
						int soundIndex;

						soundIndex = G_SoundIndex( "sounds/gladiator/score" );
						G_AnnouncerSound( winner, soundIndex, GS_MAX_TEAMS, true, loser );

						winner.stats.addScore( 1 );
					}

					this.roundLosers.reverse();
					for ( uint i = 0; i < this.roundLosers.size(); i++ )
					{
						this.challengersQueueAddPlayer( this.roundLosers[i] );
					}

					@this.roundWinner = @winner;
				}
				break;

			default:
				break;
		}
	}

	void think()
	{
		if ( this.state == DA_ROUNDSTATE_NONE )
			return;

		if ( match.getState() != MATCH_STATE_PLAYTIME )
		{
			this.endGame();
			return;
		}

		if ( this.roundStateEndTime != 0 )
		{
			if ( this.roundStateEndTime < levelTime )
			{
				this.newRoundState( this.state + 1 );
				return;
			}

			if ( this.countDown > 0 )
			{
				int remainingSeconds = int( ( this.roundStateEndTime - levelTime ) * 0.001f ) + 1;
				if ( remainingSeconds < 0 )
					remainingSeconds = 0;

				if ( remainingSeconds < this.countDown )
				{
					this.countDown = remainingSeconds;

					if ( this.countDown <= 3 )
					{
						int soundIndex = G_SoundIndex( "sounds/gladiator/countdown_" + this.countDown );
						G_AnnouncerSound( null, soundIndex, GS_MAX_TEAMS, false, null );
					}
					G_CenterPrintMsg( null, String( this.countDown ) );
				}
			}
		}

		// if one of the teams has no players alive move from DA_ROUNDSTATE_ROUND
		if ( this.state == DA_ROUNDSTATE_ROUND )
		{
			Entity @ent;
			Team @team;
			int count;

			@team = @G_GetTeam( TEAM_PLAYERS );
			count = 0;

			for ( int j = 0; @team.ent( j ) != null; j++ )
			{
				@ent = @team.ent( j );
				if ( !ent.isGhosting() )
					count++;

				// do not let the challengers be moved to specs due to inactivity
				if ( ent.client.chaseActive ) {
					ent.client.lastActivity = levelTime;
				}
			}

			if ( count < 2 )
				this.newRoundState( this.state + 1 );
		}
	}

	void playerKilled( Entity @target, Entity @attacker, Entity @inflictor, int mod )
	{
		if ( @target == null || @target.client == null )
			return;

		if ( this.isInRound() )
		{
			this.addLoser( target.client );

			Vec3 vel = target.velocity;
			if ( vel.z < -1600 && target.health < MAX_HEALTH )
			{
				G_GlobalSound( CHAN_AUTO, G_SoundIndex( "sounds/gladiator/smackdown" ) );
			}
		}

		if ( this.state == DA_ROUNDSTATE_PREROUND )
		{
			target.client.stats.addScore( -1 );
			G_LocalSound( target.client, CHAN_AUTO, G_SoundIndex( "sounds/gladiator/ouch" ) );
		}

		if ( @attacker == null || @attacker.client == null )
			return;

		for ( int i = 0; i < maxClients; i++ )
		{
			Client @client = @G_GetClient( i );
			if ( client.state() < CS_SPAWNED )
				continue;

			if ( @client == @this.roundWinner || @client == @this.roundChallenger )
				continue;
		}
	}

	bool isInRound()
	{
		return (this.state > DA_ROUNDSTATE_NONE && this.state < DA_ROUNDSTATE_POSTROUND);
	}
}

cDARound daRound;

///*****************************************************************
/// NEW MAP ENTITY DEFINITIONS
///*****************************************************************

void target_connectroom(Entity@ self)
{

}

///*****************************************************************
/// LOCAL FUNCTIONS
///*****************************************************************

void DA_SetUpWarmup()
{
	GENERIC_SetUpWarmup();

	// set spawnsystem type to instant while players join
	for ( int team = TEAM_PLAYERS; team < GS_MAX_TEAMS; team++ )
		gametype.setTeamSpawnsystem( team, SPAWNSYSTEM_INSTANT, 0, 0, false );

	gametype.readyAnnouncementEnabled = true;
}

void DA_SetUpCountdown()
{
	gametype.readyAnnouncementEnabled = false;
	gametype.scoreAnnouncementEnabled = false;
	gametype.countdownEnabled = false;
	G_RemoveAllProjectiles();

	// lock teams
	bool anyone = false;
	if ( gametype.isTeamBased )
	{
		for ( int team = TEAM_ALPHA; team < GS_MAX_TEAMS; team++ )
		{
			if ( G_GetTeam( team ).lock() )
				anyone = true;
		}
	}
	else
	{
		if ( G_GetTeam( TEAM_PLAYERS ).lock() )
			anyone = true;
	}

	if ( anyone )
		G_PrintMsg( null, "Teams locked.\n" );

	// Countdowns should be made entirely client side, because we now can

	int soundIndex = G_SoundIndex( "sounds/gladiator/let_the_games_begin" );
	G_AnnouncerSound( null, soundIndex, GS_MAX_TEAMS, false, null );
}

///*****************************************************************
/// MODULE SCRIPT CALLS
///*****************************************************************

bool GT_Command( Client @client, const String &cmdString, const String &argsString, int argc )
{
	return false;
}

// select a spawning point for a player
Entity @last_spawnposition;
bool selected_spawn = false;
Entity@[] gladiator_rooms;
Entity @GT_SelectSpawnPoint( Entity @self )
{
	/*if ( match.getState() != MATCH_STATE_PLAYTIME )
	  return GENERIC_SelectBestRandomSpawnPoint( self, "info_player_deathmatch" );*/

	if ( self.isGhosting() )
		return null;

	Team @enemyTeam;
	Entity @enemy;
	Entity @room;
	Entity@[] spawnents;
	Entity@[] rooms;

	if ( !selected_spawn || @last_spawnposition == null )
	{
		selected_spawn = true;

		//get random room
		@room = @gladiator_rooms[random_uniform(0, gladiator_rooms.size())];

		//get first spawnpoint from room
		spawnents = room.findTargets();
		if ( spawnents.size() == 0 )
			return null; // hmm? bad/old map
		@last_spawnposition = @spawnents[0];
	} else {

		spawnents = last_spawnposition.findTargets();
		if ( spawnents.size() > 0 )
		{
			@last_spawnposition = @spawnents[0];
		} else {
			selected_spawn = false;
			return GT_SelectSpawnPoint(self); // reached the end of spawnpoint chain
		}
	}

	return @last_spawnposition;
}

String @playerScoreboardMessage( Client @client ) {
	int playerID = client.getEnt().isGhosting() ? -client.playerNum - 1 : client.playerNum;
	int state = match.getState() == MATCH_STATE_WARMUP ? ( client.isReady() ? 1 : 0 ) : 0;

	return " " + playerID
		+ " " + client.ping
		+ " " + client.stats.score
		+ " " + client.stats.frags
		+ " " + state;
}

String @GT_ScoreboardMessage() {
	int challengers = daRound.roundChallengers.size();
	int loosers = daRound.roundLosers.size();

	Team @team = @G_GetTeam( TEAM_PLAYERS );
	String scoreboard = daRound.numRounds + " " + team.numPlayers;

	// first add the players in the duel
	for( int i = 0; i < challengers; i++ ) {
		Client @client = @daRound.roundChallengers[i];
		if( @client != null ) {
			scoreboard += playerScoreboardMessage( client );
		}
	}

	// then add the round losers
	if( daRound.state > DA_ROUNDSTATE_NONE && daRound.state < DA_ROUNDSTATE_POSTROUND && loosers > 0 ) {
		for( int i = loosers - 1; i >= 0; i-- ) {
			Client @client = @daRound.roundLosers[i];
			if( @client != null ) {
				scoreboard += playerScoreboardMessage( client );
			}
		}
	}

	// then add all the players in the queue
	for( int i = 0; i < maxClients; i++ ) {
		if ( daRound.daChallengersQueue[i] < 0 || daRound.daChallengersQueue[i] >= maxClients )
			break;

		Client @client = @G_GetClient( daRound.daChallengersQueue[i] );
		if ( @client == null )
			break;

		scoreboard += playerScoreboardMessage( client );
	}

	return scoreboard;
}

// Some game actions trigger score events. These are events not related to killing
// oponents, like capturing a flag
// Warning: client can be null
void GT_ScoreEvent( Client @client, const String &score_event, const String &args )
{
	if ( score_event == "kill" )
	{
		Entity @attacker = null;

		if ( @client != null )
			@attacker = @client.getEnt();

		int arg1 = args.getToken( 0 ).toInt();
		int arg2 = args.getToken( 1 ).toInt();
		int mod  = args.getToken( 3 ).toInt();

		// target, attacker, inflictor
		daRound.playerKilled( G_GetEntity( arg1 ), attacker, G_GetEntity( arg2 ), mod );
	}
}

// a player is being respawned. This can happen from several ways, as dying, changing team,
// being moved to ghost state, be placed in respawn queue, being spawned from spawn queue, etc
void GT_PlayerRespawn( Entity @ent, int old_team, int new_team )
{
	if ( old_team != new_team )
	{
		daRound.playerTeamChanged( ent.client, new_team );
	}

	if ( ent.isGhosting() )
		return;

	const Item @item;
	const Item @ammoItem;

	if ( match.getState() == MATCH_STATE_PLAYTIME )
	{
		ent.client.getEnt().health = MAX_HEALTH;
	} else {
		for ( int i = WEAP_GUNBLADE ; i < WEAP_TOTAL; i++ )
		{
			ent.client.inventoryGiveItem( i );

			@item = @G_GetItem( i );

			@ammoItem = @G_GetItem( item.ammoTag );
			if ( @ammoItem != null )
				ent.client.inventorySetCount( ammoItem.tag, 66 );
		}
	}


	// auto-select best weapon in the inventory
	ent.client.selectWeapon( -1 );

	if( match.getState() == MATCH_STATE_WARMUP ) {
		ent.respawnEffect();
	}
}

// Thinking function. Called each frame
void GT_ThinkRules()
{
	if ( match.scoreLimitHit() || match.timeLimitHit() )
		match.launchState( match.getState() + 1 );

	if ( match.getState() >= MATCH_STATE_POSTMATCH )
		return;

	GENERIC_Think();

	daRound.think();
}

// The game has detected the end of the match state, but it
// doesn't advance it before calling this function.
// This function must give permission to move into the next
// state by returning true.
bool GT_MatchStateFinished( int incomingMatchState )
{
	// ** MISSING EXTEND PLAYTIME CHECK **

	if ( match.getState() <= MATCH_STATE_WARMUP && incomingMatchState > MATCH_STATE_WARMUP
	    && incomingMatchState < MATCH_STATE_POSTMATCH )
		match.startAutorecord();

	if ( match.getState() == MATCH_STATE_POSTMATCH )
		match.stopAutorecord();

	return true;
}

// the match state has just moved into a new state. Here is the
// place to set up the new state rules
void GT_MatchStateStarted()
{
	switch ( match.getState() )
	{
		case MATCH_STATE_WARMUP:
			DA_SetUpWarmup();
			break;

		case MATCH_STATE_COUNTDOWN:
			DA_SetUpCountdown();
			break;

		case MATCH_STATE_PLAYTIME:
			daRound.newGame();
			break;

		case MATCH_STATE_POSTMATCH:
			daRound.endGame();
			break;

		default:
			break;
	}
}

// the gametype is shutting down cause of a match restart or map change
void GT_Shutdown()
{
}

// The map entities have just been spawned. The level is initialized for
// playing, but nothing has yet started.
void GT_SpawnGametype()
{
	if ( gladiator_rooms.size() > 0 )
		return;

	Entity@[] rooms = G_FindByClassname( "target_connectroom" );
	for ( uint i = 0; i < rooms.size(); i++ )
	{
		Entity@ ent = rooms[i];
		uint k = 0;
		do {
			Entity@[] spawnents = ent.findTargets();
			if ( spawnents.size() == 0 )
				break;
			for ( uint j = 0; j < spawnents.size(); j++ )
			{
				@ent = @spawnents[j];
			}
			k++;
		} while ( true );
		if ( k < 4 )
		{
		} else {
			gladiator_rooms.push_back(rooms[i]);
		}
	}
}

// Important: This function is called before any entity is spawned, and
// spawning entities from it is forbidden. If you want to make any entity
// spawning at initialization do it in GT_SpawnGametype, which is called
// right after the map entities spawning.

void GT_InitGametype()
{
	daRound.init();

	gametype.isTeamBased = false;
	gametype.isRace = false;
	gametype.hasChallengersQueue = false;
	gametype.maxPlayersPerTeam = 0;

	gametype.readyAnnouncementEnabled = false;
	gametype.scoreAnnouncementEnabled = false;
	gametype.countdownEnabled = false;
	gametype.matchAbortDisabled = false;
	gametype.shootingDisabled = false;
	gametype.infiniteAmmo = true;
	gametype.canForceModels = true;
	gametype.removeInactivePlayers = true;
	gametype.selfDamage = false;

	gametype.spawnpointRadius = 0;

	// set spawnsystem type to instant while players join
	for ( int team = TEAM_PLAYERS; team < GS_MAX_TEAMS; team++ )
		gametype.setTeamSpawnsystem( team, SPAWNSYSTEM_INSTANT, 0, 0, false );

	// register gladiator media pure
	G_SoundIndex( "sounds/gladiator/fight" );
	G_SoundIndex( "sounds/gladiator/score" );
	G_SoundIndex( "sounds/gladiator/urrekt" );
	G_SoundIndex( "sounds/gladiator/ready" );
	G_SoundIndex( "sounds/gladiator/countdown_1" );
	G_SoundIndex( "sounds/gladiator/countdown_2" );
	G_SoundIndex( "sounds/gladiator/countdown_3" );
	G_SoundIndex( "sounds/gladiator/let_the_games_begin" );
	G_SoundIndex( "sounds/gladiator/you_both_suck" );
	G_SoundIndex( "sounds/gladiator/oh_no" );
	G_SoundIndex( "sounds/gladiator/its_a_tie" );
	G_SoundIndex( "sounds/gladiator/the_king_is_back" );
	G_SoundIndex( "sounds/gladiator/fall_scream" );
	G_SoundIndex( "sounds/gladiator/wowyourterrible" );
	G_SoundIndex( "sounds/gladiator/smackdown" );

	endMatchSounds.push_back( G_SoundIndex( "sounds/gladiator/perrina_sucks_dicks" ) );
	endMatchSounds.push_back( G_SoundIndex( "sounds/gladiator/hashtagpuffdarcrybaby" ) );
	endMatchSounds.push_back( G_SoundIndex( "sounds/gladiator/callmemaybe" ) );
	endMatchSounds.push_back( G_SoundIndex( "sounds/gladiator/mikecabbage" ) );
	endMatchSounds.push_back( G_SoundIndex( "sounds/gladiator/rihanna" ) );
	endMatchSounds.push_back( G_SoundIndex( "sounds/gladiator/zorg" ) );
	endMatchSounds.push_back( G_SoundIndex( "sounds/gladiator/shazam" ) );
	endMatchSounds.push_back( G_SoundIndex( "sounds/gladiator/sanic" ) );
	endMatchSounds.push_back( G_SoundIndex( "sounds/gladiator/rlop" ) );
	endMatchSounds.push_back( G_SoundIndex( "sounds/gladiator/puffdarquote" ) );
	endMatchSounds.push_back( G_SoundIndex( "sounds/gladiator/magnets" ) );
	endMatchSounds.push_back( G_SoundIndex( "sounds/gladiator/howdoyoufeel" ) );
	endMatchSounds.push_back( G_SoundIndex( "sounds/gladiator/fuckoff" ) );
	endMatchSounds.push_back( G_SoundIndex( "sounds/gladiator/fluffle" ) );
	endMatchSounds.push_back( G_SoundIndex( "sounds/gladiator/drillbit" ) );
	endMatchSounds.push_back( G_SoundIndex( "sounds/gladiator/demo" ) );

	crownModel = G_ModelIndex( "models/objects/crown" );
}
