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
int randNum;
int deadIcon;
int aliveIcon;
int[] endMatchSounds;
const String[] WEAPON_NAMES = {"none", "^7GB", "^9MG", "^8RG", "^4GL", "^1RL", "^2PG", "^3LG", "^5EB"};

Cvar gt_debug = Cvar("gt_debug", "0", 0);

class cDARound
{
	int state;
	int numRounds;
	uint roundStateStartTime;
	uint roundStateEndTime;
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
		G_DPrint("adding loser\n");
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

				/*if ( @client == @this.roundWinner )
				  {
				  @this.roundWinner = null;
				  this.newRoundState( DA_ROUNDSTATE_ROUNDFINISHED );
				  }

				  if ( @client == @this.roundChallenger )
				  {
				  @this.roundChallenger = null;
				  this.newRoundState( DA_ROUNDSTATE_ROUNDFINISHED );
				  }*/
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
				GT_Stats_GetPlayer( this.roundWinner ).stats.add("matchwins", 1);
				G_DPrint("MATCHWIN " + this.roundWinner.name + "\n");


				Entity @ent;
				Team @team;

				@team = @G_GetTeam( TEAM_PLAYERS );

				for ( int j = 0; @team.ent( j ) != null; j++ )
				{
					@ent = @team.ent( j );
					if ( @ent.client == @this.roundWinner )
						continue;

					GT_Stats_GetPlayer( ent.client ).stats.add("matchlosses", 1);
				}
			}
		}

		GLADIATOR_SetUpEndMatch();
	}

	void GLADIATOR_SetUpEndMatch()
	{
		Client @client;

		gametype.shootingDisabled = true;
		gametype.readyAnnouncementEnabled = false;
		gametype.scoreAnnouncementEnabled = false;
		gametype.countdownEnabled = false;

		for ( int i = 0; i < maxClients; i++ )
		{
			@client = @G_GetClient( i );

			if ( client.state() >= CS_SPAWNED ) {
				client.respawn( true ); // ghost them all
				GENERIC_SetPostmatchQuickMenu( @client );
			}
		}

		GENERIC_UpdateMatchScore();

		// print scores to console
		if ( gametype.isTeamBased )
		{
			Team @team1 = @G_GetTeam( TEAM_ALPHA );
			Team @team2 = @G_GetTeam( TEAM_BETA );

			String sC1 = (team1.stats.score < team2.stats.score ? S_COLOR_RED : S_COLOR_GREEN);
			String sC2 = (team2.stats.score < team1.stats.score ? S_COLOR_RED : S_COLOR_GREEN);

			G_PrintMsg( null, S_COLOR_YELLOW + "Final score: " + S_COLOR_WHITE + team1.name + S_COLOR_WHITE + " vs " +
				team2.name + S_COLOR_WHITE + " - " + match.getScore() + "\n" );
		}

		int soundIndex = endMatchSounds[uint(brandom(0,endMatchSounds.size()-0.001))];
		G_AnnouncerSound( null, soundIndex, GS_MAX_TEAMS, true, null );
	}

	void newRound()
	{
		StopModifiers();

		randNum = int(brandom(0,7.999)); //randomize weapon
		G_RemoveDeadBodies();
		G_RemoveAllProjectiles();
		G_ResetLevel();

		this.newRoundState( DA_ROUNDSTATE_PREROUND );
		this.numRounds++;
		//int currentWeapon = randNum + 1;
		//this.roundAnnouncementPrint( S_COLOR_WHITE + "This round's weapon: " + S_COLOR_WHITE + WEAPON_NAMES[currentWeapon]);
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
					this.roundStateEndTime = levelTime + 7000;
					this.countDown = 5;

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

					// respawn all clients inside the playing teams
					for ( int j = 0; @team.ent( j ) != null; j++ )
					{
						@ent = @team.ent( j );
						ent.client.respawn( true );
						ent.client.chaseCam( null, false );
						ent.client.chaseActive = true;
					}

					selected_spawn = false;

					for ( int j = 0; @team.ent( j ) != null; j++ )
					{
						@ent = @team.ent( j );
						if ( this.isChallenger(ent.client) )
						{
							ent.client.respawn( false );
							GT_Stats_GetPlayer( ent.client ).stats.add("rounds", 1);
						}
					}


					DoSpinner();
					LoadRandomModifier();

					// generate vs string
					String vs_string = S_COLOR_WHITE + this.roundChallengers[0].name;
					for ( uint i = 1; i < this.roundChallengers.size(); i++ )
					{
						vs_string += S_COLOR_WHITE + " vs. " + this.roundChallengers[i].name;
					}

					this.roundAnnouncementPrint( vs_string );
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
					InitModifiers();
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

					// watch for one of the players removing from the game
					/*if ( @this.roundWinner == null || @this.roundChallenger == null )
					  {
					  if ( @this.roundWinner != null )
					  @winner = @this.roundWinner;
					  else if ( @this.roundChallenger != null )
					  @winner = @this.roundChallenger;
					  }
					  else if ( !this.roundWinner.getEnt().isGhosting() && this.roundChallenger.getEnt().isGhosting() )
					  {
					  @winner = @this.roundWinner;
					  @loser = @this.roundChallenger;
					  }
					  else if ( this.roundWinner.getEnt().isGhosting() && !this.roundChallenger.getEnt().isGhosting() )
					  {
					  @winner = @this.roundChallenger;
					  @loser = @this.roundWinner;
					  }*/

					// if we didn't find a winner, it was a draw round
					if ( @winner == null )
					{
						int soundIndex;
						this.roundAnnouncementPrint( S_COLOR_WHITE + "Wow your terrible" );

						//this.challengersQueueAddPlayer( this.roundWinner );
						//this.challengersQueueAddPlayer( this.roundChallenger );
						soundIndex = G_SoundIndex( "sounds/gladiator/wowyourterrible" );
						G_AnnouncerSound( null, soundIndex, GS_MAX_TEAMS, false, null );
					}
					else
					{
						int soundIndex;

						soundIndex = G_SoundIndex( "sounds/gladiator/score" );
						G_AnnouncerSound( winner, soundIndex, GS_MAX_TEAMS, true, loser );

						winner.stats.addScore( 1 );
						GT_Stats_GetPlayer( winner ).stats.add("round_wins", 1);
						G_DPrint( "ROUNDWIN " + winner.name + "\n" );
						//soundIndex = G_SoundIndex( "sounds/gladiator/urrekt" );
						//G_AnnouncerSound( loser, soundIndex, GS_MAX_TEAMS, true, null );
						//this.challengersQueueAddPlayer( loser );
						//this.roundAnnouncementPrint( S_COLOR_WHITE + loser.name + S_COLOR_WHITE + " wasn't man enough.." );
					}

					this.roundLosers.reverse();
					for ( uint i = 0; i < this.roundLosers.size(); i++ )
					{
						this.challengersQueueAddPlayer( this.roundLosers[i] );
						if ( @winner == null )
						{
							GT_Stats_GetPlayer( this.roundLosers[i] ).stats.add("round_draws", 1);
						}
					}

					@this.roundWinner = @winner;
					//@this.roundChallenger = null;
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
				// we can't use the authomatic countdown announces because their are based on the
				// matchstate timelimit, and prerounds don't use it. So, fire the announces "by hand".
				int remainingSeconds = int( ( this.roundStateEndTime - levelTime ) * 0.001f ) + 1;
				if ( remainingSeconds < 0 )
					remainingSeconds = 0;

				if ( remainingSeconds < this.countDown )
				{
					this.countDown = remainingSeconds;

					if ( this.countDown == 4 )
					{
						int soundIndex = G_SoundIndex( "sounds/gladiator/ready" );
						G_AnnouncerSound( null, soundIndex, GS_MAX_TEAMS, false, null );
					}
					else if ( this.countDown <= 3 )
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

			ThinkModifiers();

			if ( count < 2 )
				this.newRoundState( this.state + 1 );
		}
	}

	void playerKilled( Entity @target, Entity @attacker, Entity @inflictor, int mod )
	{
		if ( @target == null || @target.client == null )
			return;

		Stats_Player@ target_player = @GT_Stats_GetPlayer( target.client );

		if ( this.isInRound() )
		{
			target_player.stats.add("deaths", 1);
			target_player.stats.add("deathmod_"+String(mod), 1);
			target_player.stats.add("roundpos_"+this.roundChallengers.length(), 1);
			this.addLoser( target.client );

			Vec3 vel = target.velocity;
			if ( vel.z < -1600 && target.health < 666 )
			{
				G_GlobalSound( CHAN_AUTO, G_SoundIndex( "sounds/gladiator/smackdown" ) );
				target_player.stats.add("smackdowns", 1);
			}
		}

		if ( this.state == DA_ROUNDSTATE_PREROUND )
		{
			G_DPrint("PREROUND ");

			target.client.stats.addScore( -1 );
			G_LocalSound( target.client, CHAN_AUTO, G_SoundIndex( "sounds/gladiator/ouch" ) );
			target_player.stats.add("preround_deaths", 1);
		}

		if ( this.state == DA_ROUNDSTATE_ROUND || this.state == DA_ROUNDSTATE_ROUNDFINISHED )
		{
			G_DPrint("INROUND ");
			//return;
		}

		if ( @attacker == null || @attacker.client == null )
			return;

		Stats_Player@ attacker_player = @GT_Stats_GetPlayer( attacker.client );
		attacker_player.stats.add("kills", 1);

		target.client.printMessage( "You were fragged by " + attacker.client.name + " (health: " + rint( attacker.health ) + ", armor: " + rint( attacker.client.armor ) + ")\n" );

		for ( int i = 0; i < maxClients; i++ )
		{
			Client @client = @G_GetClient( i );
			if ( client.state() < CS_SPAWNED )
				continue;

			if ( @client == @this.roundWinner || @client == @this.roundChallenger )
				continue;

			client.printMessage( target.client.name + " was fragged by " + attacker.client.name + " (health: " + int( attacker.health ) + ", armor: " + int( attacker.client.armor ) + ")\n" );
		}

		// check for generic awards for the frag
		award_playerKilled( @target, @attacker, @inflictor );
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

void G_DPrint( String& msg )
{
	if ( gt_debug.boolean )
		G_Print(msg);
}

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
	gametype.shootingDisabled = true;
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
	if ( cmdString == "gametype" )
	{
		String response = "";
		Cvar fs_game( "fs_game", "", 0 );
		String manifest = gametype.manifest;

		response += "\n";
		response += "Gametype " + gametype.name + " : " + gametype.title + "\n";
		response += "----------------\n";
		response += "Version: " + gametype.version + "\n";
		response += "Author: " + gametype.author + "\n";
		response += "Mod: " + fs_game.string + (!manifest.empty() ? " (manifest: " + manifest + ")" : "") + "\n";
		response += "----------------\n";

		G_PrintMsg( client.getEnt(), response );
		return true;
	}
	else if( cmdString == "cvarinfo" )
	{
		GENERIC_CheatVarResponse( client, cmdString, argsString, argc );
		return true;
	}
	else if ( cmdString == "callvotevalidate" )
	{
		String votename = argsString.getToken( 0 );
		if ( votename == "mods")
		{
			String voteArg = argsString.getToken( 1 );
			if ( voteArg.len() < 1 )
			{
				client.printMessage( "Callvote " + votename + " requires at least one argument\n" );
				return false;
			}

			if ( ModStringToMode(voteArg) == uint(gt_mod_mode.integer) )
			{
				client.printMessage( "Modifiers already set at "+voteArg+"\n" );
				return false;
			}

			if ( isValidMode(voteArg) )
			{
				return true;
			}
			else
			{
				client.printMessage( voteArg + " Is not a valid modifier\n");
				return false;
			}
		}
	}
	else if ( cmdString == "callvotepassed" )
	{
		String votename = argsString.getToken( 0 );
		if ( votename == "mods")
		{
			gt_mod_mode.set(ModStringToMode(argsString.getToken( 1 )));
		}
	}
	else if ( cmdString == "!stats" )
	{
		Stats_Player@ player = @GT_Stats_GetPlayer( client );
		G_PrintMsg( client.getEnt(), player.stats.toString() );
	}

	return false;
}

// When this function is called the weights of items have been reset to their default values,
// this means, the weights *are set*, and what this function does is scaling them depending
// on the current bot status.
// Player, and non-item entities don't have any weight set. So they will be ignored by the bot
// unless a weight is assigned here.
bool GT_UpdateBotStatus( Entity @ent )
{
	Entity @goal;
	Bot @bot;

	@bot = @ent.client.getBot();
	if ( @bot == null )
		return false;

	float offensiveStatus = GENERIC_OffensiveStatus( ent );

	// loop all the goal entities
	for ( int i = AI::GetNextGoal( AI::GetRootGoal() ); i != AI::GetRootGoal(); i = AI::GetNextGoal( i ) )
	{
		@goal = @AI::GetGoalEntity( i );

		// by now, always full-ignore not solid entities
		if ( goal.solid == SOLID_NOT )
		{
			bot.setGoalWeight( i, 0 );
			continue;
		}

		if ( @goal.client != null )
		{
			bot.setGoalWeight( i, GENERIC_PlayerWeight( ent, goal ) * 2.5 * offensiveStatus );
			continue;
		}

		// ignore it
		bot.setGoalWeight( i, 0 );
	}

	return true; // handled by the script
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
		@room = @gladiator_rooms[uint(brandom(0,gladiator_rooms.size()-0.001))];

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

String @GT_ScoreboardMessage( uint maxlen )
{
	String scoreboardMessage = "";
	String entry;
	Team @team;
	Client @client;
	int i, playerID;

	@team = @G_GetTeam( TEAM_PLAYERS );

	// &t = team tab, team tag, team score (doesn't apply), team ping (doesn't apply)
	entry = "&t " + int( TEAM_PLAYERS ) + " " + team.stats.score + " 0 ";
	if ( scoreboardMessage.len() + entry.len() < maxlen )
		scoreboardMessage += entry;

	// first add the players in the duel
	for ( uint j = 0; j < daRound.roundChallengers.size(); j++ )
	{
		@client = @daRound.roundChallengers[j];
		if ( @client != null )
		{
			if ( match.getState() != MATCH_STATE_PLAYTIME )
				playerID = client.playerNum;
			else
				playerID = client.getEnt().isGhosting() ? -( client.playerNum + 1 ) : client.playerNum;

			entry = "&p " + aliveIcon + " "
				+ playerID + " "
				+ client.clanName + " "
				+ client.stats.score + " "
				+ client.ping + " "
				+ ( client.isReady() ? "1" : "0" ) + " ";

			if ( scoreboardMessage.len() + entry.len() < maxlen )
				scoreboardMessage += entry;
		}
	}

	// then add the round losers
	if ( daRound.state > DA_ROUNDSTATE_NONE && daRound.state < DA_ROUNDSTATE_POSTROUND && daRound.roundLosers.size() > 0 )
	{
		for ( int j = daRound.roundLosers.size()-1; j >= 0; j-- )
		{
			@client = @daRound.roundLosers[j];
			if ( @client != null )
			{
				if ( match.getState() != MATCH_STATE_PLAYTIME )
					playerID = client.playerNum;
				else
					playerID = client.getEnt().isGhosting() ? -( client.playerNum + 1 ) : client.playerNum;

				entry = "&p " + deadIcon + " "
					+ playerID + " "
					+ client.clanName + " "
					+ client.stats.score + " "
					+ client.ping + " "
					+ ( client.isReady() ? "1" : "0" ) + " ";

				if ( scoreboardMessage.len() + entry.len() < maxlen )
					scoreboardMessage += entry;
			}
		}
	}

	// then add all the players in the queue
	for ( i = 0; i < maxClients; i++ )
	{
		if ( daRound.daChallengersQueue[i] < 0 || daRound.daChallengersQueue[i] >= maxClients )
			break;

		@client = @G_GetClient( daRound.daChallengersQueue[i] );
		if ( @client == null )
			break;

		if ( match.getState() != MATCH_STATE_PLAYTIME )
			playerID = client.playerNum;
		else
			playerID = client.getEnt().isGhosting() ? -( client.playerNum + 1 ) : client.playerNum;

		entry = "&p " + "0 "
			+ playerID + " "
			+ client.clanName + " "
			+ client.stats.score + " "
			+ client.ping + " "
			+ ( client.isReady() ? "1" : "0" ) + " ";

		if ( scoreboardMessage.len() + entry.len() < maxlen )
			scoreboardMessage += entry;

	}

	return scoreboardMessage;
}

// Some game actions trigger score events. These are events not related to killing
// oponents, like capturing a flag
// Warning: client can be null
void GT_ScoreEvent( Client @client, const String &score_event, const String &args )
{
	if ( score_event == "dmg" )
	{
	}
	else if ( score_event == "kill" )
	{
		Entity @attacker = null;

		if ( @client != null )
			@attacker = @client.getEnt();

		int arg1 = args.getToken( 0 ).toInt();
		int arg2 = args.getToken( 1 ).toInt();
		int mod  = args.getToken( 3 ).toInt();

		if ( arg1 == arg2 )
			G_DPrint( "VOID ");
		// target, attacker, inflictor
		daRound.playerKilled( G_GetEntity( arg1 ), attacker, G_GetEntity( arg2 ), mod );
	}
	else if ( score_event == "award" )
	{
		if ( daRound.isInRound() )
		{
			Stats_Player@ player = @GT_Stats_GetPlayer( client );
			String cleanAward = "award_" + args.removeColorTokens().tolower();
			player.stats.add(cleanAward, 1);
			player.stats.add("awards", 1);
		}
	}
	else if ( score_event == "enterGame" )
	{
		if ( @client != null )
		{
			GT_Stats_GetPlayer( client ).load();
		}
	}
	else if ( score_event == "userinfochanged" )
	{
		if ( @client != null )
		{
			Stats_Player@ player = @GT_Stats_GetPlayer( client );
			if ( @player.stats == null )
				player.load();

			if ( client.name != player.stats["name"] )
				player.load();
		}
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

	Item @item;
	Item @ammoItem;

	if ( match.getState() == MATCH_STATE_PLAYTIME )
	{
		//ent.client.inventorySetCount( WEAP_GUNBLADE, 1 );
		ent.client.inventorySetCount( POWERUP_QUAD, 100);
		ent.client.armor = 666;
		ent.client.getEnt().health = 666;
		/*ent.client.inventoryGiveItem( WEAP_GUNBLADE );
		  ent.client.inventoryGiveItem( WEAP_NONE + 1 + randNum );
		  ent.client.inventorySetCount( AMMO_GUNBLADE + randNum, 66 );*/
	} else {
		for ( int i = WEAP_GUNBLADE ; i < WEAP_TOTAL; i++ )
		{
			if ( i == WEAP_INSTAGUN || i == WEAP_MACHINEGUN ) // dont add instagun...
				continue;

			ent.client.inventoryGiveItem( i );

			@item = @G_GetItem( i );

			@ammoItem = @G_GetItem( item.ammoTag );
			if ( @ammoItem != null )
				ent.client.inventorySetCount( ammoItem.tag, 66 );
		}
	}


	ent.setupModel( "models/players/bigvic", "fullbright" );
	// auto-select best weapon in the inventory
	ent.client.selectWeapon( -1 );

	// add a teleportation effect
	ent.respawnEffect();
}

// Thinking function. Called each frame
void GT_ThinkRules()
{
	if ( match.scoreLimitHit() || match.timeLimitHit() || match.suddenDeathFinished() )
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
	GT_Stats_Init();

	if ( gladiator_rooms.size() > 0 )
		return;

	G_DPrint("Rooms debug: \n");
	Entity@[] rooms = G_FindByClassname( "target_connectroom" );
	G_DPrint("found "+rooms.size()+" rooms\n");
	for ( uint i = 0; i < rooms.size(); i++ )
	{
		Entity@ ent = rooms[i];
		G_DPrint("Room #"+(i+1)+" "+ent.targetname+":\n");
		uint k = 0;
		do {
			Entity@[] spawnents = ent.findTargets();
			if ( spawnents.size() == 0 )
				break;
			for ( uint j = 0; j < spawnents.size(); j++ )
			{
				@ent = @spawnents[j];
				G_DPrint("spawn #"+(k+1)+" "+ent.targetname+" @ "+vec3ToString(ent.origin)+"\n");
			}
			k++;
		} while ( true );
		if ( k < 4 )
		{
			G_DPrint("WARN: invalid room\n");
		} else {
			gladiator_rooms.push_back(rooms[i]);
		}
	}
	G_DPrint("Verification complete, "+gladiator_rooms.size()+" valid rooms found.\n");
}

// Important: This function is called before any entity is spawned, and
// spawning entities from it is forbidden. If you want to make any entity
// spawning at initialization do it in GT_SpawnGametype, which is called
// right after the map entities spawning.

void GT_InitGametype()
{
	gametype.title = "Gladiator";
	gametype.version = "0.6";
	gametype.author = "Rick James";

	daRound.init();

	// if the gametype doesn't have a config file, create it
	if ( !G_FileExists( "configs/server/gametypes/" + gametype.name + ".cfg" ) )
	{
		String config;

		// the config file doesn't exist or it's empty, create it
		config = "// '" + gametype.title + "' gametype configuration file\n"
			+ "// This config will be executed each time the gametype is started\n"
			+ "\n\n// map rotation\n"
			+ "set g_maplist \"gladiator01\" // list of maps in automatic rotation\n"
			+ "set g_maprotation \"0\"   // 0 = same map, 1 = in order, 2 = random\n"
			+ "\n// game settings\n"
			+ "set g_scorelimit \"0\"\n"
			+ "set g_timelimit \"0\"\n"
			+ "set g_warmup_timelimit \"1\"\n"
			+ "set g_match_extendedtime \"0\"\n"
			+ "set g_allow_falldamage \"0\"\n"
			+ "set g_allow_selfdamage \"0\"\n"
			+ "set g_allow_teamdamage \"0\"\n"
			+ "set g_allow_stun \"0\"\n"
			+ "set g_teams_maxplayers \"0\"\n"
			+ "set g_teams_allow_uneven \"0\"\n"
			+ "set g_countdown_time \"5\"\n"
			+ "set g_maxtimeouts \"3\" // -1 = unlimited\n"
			+ "\necho \"" + gametype.name + ".cfg executed\"\n";

		G_WriteFile( "configs/server/gametypes/" + gametype.name + ".cfg", config );
		G_Print( "Created default config file for '" + gametype.name + "'\n" );
		G_CmdExecute( "exec configs/server/gametypes/" + gametype.name + ".cfg silent" );
	}

	gametype.spawnableItemsMask = ( IT_POWERUP );
	gametype.respawnableItemsMask = (IT_POWERUP );
	gametype.dropableItemsMask = 0;
	gametype.pickableItemsMask = ( IT_POWERUP );

	gametype.isTeamBased = false;
	gametype.isRace = false;
	gametype.hasChallengersQueue = false;
	gametype.maxPlayersPerTeam = 0;

	gametype.ammoRespawn = 0;
	gametype.armorRespawn = 0;
	gametype.weaponRespawn = 0;
	gametype.healthRespawn = 0;
	gametype.powerupRespawn = 15;
	gametype.megahealthRespawn = 0;
	gametype.ultrahealthRespawn = 0;

	gametype.readyAnnouncementEnabled = false;
	gametype.scoreAnnouncementEnabled = false;
	gametype.countdownEnabled = false;
	gametype.mathAbortDisabled = false;
	gametype.shootingDisabled = false;
	gametype.infiniteAmmo = true;
	gametype.canForceModels = true;
	gametype.canShowMinimap = false;
	gametype.teamOnlyMinimap = false;
	gametype.removeInactivePlayers = true;

	gametype.mmCompatible = true;

	gametype.spawnpointRadius = 0;

	if ( gametype.isInstagib )
		gametype.spawnpointRadius *= 2;

	// set spawnsystem type to instant while players join
	for ( int team = TEAM_PLAYERS; team < GS_MAX_TEAMS; team++ )
		gametype.setTeamSpawnsystem( team, SPAWNSYSTEM_INSTANT, 0, 0, false );

	// define the scoreboard layout
	G_ConfigString( CS_SCB_PLAYERTAB_LAYOUT, "%p l1 %n 112 %s 52 %i 52 %l 48 %r l1" );
	G_ConfigString( CS_SCB_PLAYERTAB_TITLES, "\u00A0 Name Clan Score Ping R" );

	// add commands
	G_RegisterCommand( "gametype" );
	G_RegisterCommand( "!stats" );

	// add callvotes
	G_RegisterCallvote( "mods", "<none|base|silly|verysilly>", "string", "Sets the type of possible modifiers" );

	// register gladiator media pure
	G_SoundIndex( "sounds/gladiator/fight", true );
	G_SoundIndex( "sounds/gladiator/score", true );
	G_SoundIndex( "sounds/gladiator/urrekt", true );
	G_SoundIndex( "sounds/gladiator/ready", true );
	G_SoundIndex( "sounds/gladiator/countdown_1", true );
	G_SoundIndex( "sounds/gladiator/countdown_2", true );
	G_SoundIndex( "sounds/gladiator/countdown_3", true );
	G_SoundIndex( "sounds/gladiator/let_the_games_begin", true );
	G_SoundIndex( "sounds/gladiator/you_both_suck", true );
	G_SoundIndex( "sounds/gladiator/oh_no", true );
	G_SoundIndex( "sounds/gladiator/its_a_tie", true );
	G_SoundIndex( "sounds/gladiator/the_king_is_back", true );
	G_SoundIndex( "sounds/gladiator/fall_scream", true );
	G_SoundIndex( "sounds/gladiator/wowyourterrible", true );
	G_SoundIndex( "sounds/gladiator/smackdown", true );

	endMatchSounds.push_back( G_SoundIndex( "sounds/gladiator/perrina_sucks_dicks", true ) );
	endMatchSounds.push_back( G_SoundIndex( "sounds/gladiator/hashtagpuffdarcrybaby", true ) );
	endMatchSounds.push_back( G_SoundIndex( "sounds/gladiator/callmemaybe", true ) );
	endMatchSounds.push_back( G_SoundIndex( "sounds/gladiator/mikecabbage", true ) );
	endMatchSounds.push_back( G_SoundIndex( "sounds/gladiator/rihanna", true ) );
	endMatchSounds.push_back( G_SoundIndex( "sounds/gladiator/zorg", true ) );
	endMatchSounds.push_back( G_SoundIndex( "sounds/gladiator/shazam", true ) );
	endMatchSounds.push_back( G_SoundIndex( "sounds/gladiator/sanic", true ) );
	endMatchSounds.push_back( G_SoundIndex( "sounds/gladiator/rlop", true ) );
	endMatchSounds.push_back( G_SoundIndex( "sounds/gladiator/puffdarquote", true ) );
	endMatchSounds.push_back( G_SoundIndex( "sounds/gladiator/magnets", true ) );
	endMatchSounds.push_back( G_SoundIndex( "sounds/gladiator/howdoyoufeel", true ) );
	endMatchSounds.push_back( G_SoundIndex( "sounds/gladiator/fuckoff", true ) );
	endMatchSounds.push_back( G_SoundIndex( "sounds/gladiator/fluffle", true ) );
	endMatchSounds.push_back( G_SoundIndex( "sounds/gladiator/drillbit", true ) );
	endMatchSounds.push_back( G_SoundIndex( "sounds/gladiator/demo", true ) );

	deadIcon = G_ImageIndex( "gfx/gladiator_icons/dead" );
	aliveIcon = G_ImageIndex( "gfx/gladiator_icons/alive" );

	LoadAllModifiers();
}
