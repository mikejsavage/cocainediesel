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

void GENERIC_SetUpWarmup()
{
	gametype.countdownEnabled = false;
}

void GENERIC_SetUpCountdown()
{
	gametype.countdownEnabled = true;

	// Countdowns should be made entirely client side, because we now can

	uint64 sound = Hash64( "sounds/announcer/get_ready_to_fight" );
	G_AnnouncerSound( null, sound, GS_MAX_TEAMS, false, null );
}

void GENERIC_SetUpMatch()
{
	G_RemoveAllProjectiles();
	gametype.countdownEnabled = true;

	// clear player stats and scores, team scores and respawn clients in team lists

	G_GetTeam( TEAM_ALPHA ).setScore( 0 );
	G_GetTeam( TEAM_BETA ).setScore( 0 );

	for ( int i = TEAM_PLAYERS; i < GS_MAX_TEAMS; i++ )
	{
		Team @team = @G_GetTeam( i );

		// respawn all clients inside the playing teams
		for ( int j = 0; @team.ent( j ) != null; j++ )
		{
			Entity @ent = @team.ent( j );
			ent.client.stats.clear(); // clear player scores & stats
			ent.client.respawn( false );
		}
	}

	G_RemoveDeadBodies();

	// Countdowns should be made entirely client side, because we now can
	uint64 sound = Hash64( "sounds/announcer/fight" );
	G_AnnouncerSound( null, sound, GS_MAX_TEAMS, false, null );
	G_CenterPrintMsg( null, "FIGHT!" );
}

void GENERIC_SetUpEndMatch()
{
	Client @client;

	gametype.countdownEnabled = false;

	for ( int i = 0; i < maxClients; i++ )
	{
		@client = @G_GetClient( i );

		if ( client.state() >= CS_SPAWNED ) {
			client.respawn( true ); // ghost them all
		}
	}

	GENERIC_UpdateMatchScore();

	// print scores to console
	if ( gametype.isTeamBased )
	{
		G_PrintMsg( null, S_COLOR_YELLOW + "Final score: " + match.getScore() + "\n" );
	}

	uint64 sound = Hash64( "sounds/announcer/game_over" );
	G_AnnouncerSound( null, sound, GS_MAX_TEAMS, true, null );
}

Entity @RandomEntity( String &className )
{
	array<Entity @> @spawnents = G_FindByClassname( className );
	if( spawnents.size() == 0 )
		return null;
	return spawnents[ RandomUniform( 0, spawnents.size() ) ];
}

void GENERIC_UpdateMatchScore()
{
	if( gametype.isTeamBased ) {
		match.setScore( G_GetTeam( TEAM_ALPHA ).score + " : " + G_GetTeam( TEAM_BETA ).score );
		return;
	}

	match.setScore( "" );
}
