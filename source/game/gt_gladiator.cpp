#include "qcommon/base.h"
#include "qcommon/array.h"
#include "qcommon/fs.h"
#include "qcommon/string.h"

#include "game/g_local.h"

#include "qcommon/cmodel.h"

enum GladiatorRoundState {
	GladiatorRoundState_None,
	GladiatorRoundState_Pre,
	GladiatorRoundState_Round,
	GladiatorRoundState_Finished,
	GladiatorRoundState_Post,
};

static struct GladiatorState {
	GladiatorRoundState state;
	u64 round_state_start;
	u64 round_state_end;
	s32 countdown;
	NonRAIIDynamicArray< s32 > challengers_queue;
	NonRAIIDynamicArray< s32 > round_challengers;
	NonRAIIDynamicArray< s32 > round_losers;
	s32 round_winner;
	bool randomise;
	edict_t * last_spawn;
} gladiator_state;

static constexpr int countdown_seconds = 2;
static constexpr StringHash crown_model = "models/crown/crown";

static void NewRoundState( GladiatorRoundState newState );
static void EndGame();
static void NewRound();

static void ChallengersQueueClear() {
	for( u32 i = 0; i < gladiator_state.challengers_queue.size(); i++ ) {
		gladiator_state.challengers_queue[ i ] = -1;
	}
}

static void ChallengersQueueAddPlayer( s32 player_num ) {
	assert( player_num < MAX_CLIENTS );
	for( s32 & player : gladiator_state.challengers_queue ) {
		if( player == player_num ) {
			return;
		}
	}
	gladiator_state.challengers_queue.add( player_num );
}

static void ChallengersQueueRemovePlayer( s32 player_num ) {
	for( u32 i = 0; i < gladiator_state.challengers_queue.size(); i++ ) {
		if( gladiator_state.challengers_queue[ i ] == player_num ) {
			for( u32 j = i; j < gladiator_state.challengers_queue.size() - 1; j++ ) {
				gladiator_state.challengers_queue[ j ] = gladiator_state.challengers_queue[ j + 1 ];
			}
			gladiator_state.challengers_queue.resize( gladiator_state.challengers_queue.size() - 1 );
			return;
		}
	}
}

static s32 ChallengersQueueNextPlayer() {
	if( gladiator_state.challengers_queue.size() == 0 ) {
		return -1;
	}
	s32 player = gladiator_state.challengers_queue[ 0 ];
	if( player > -1 ) {
		ChallengersQueueRemovePlayer( player );
	}
	return player;
}

static bool IsChallenger( s32 player_num ) {
	for( s32 & player : gladiator_state.round_challengers ) {
		if( player == player_num ) {
			return true;
		}
	}
	return false;
}

static void RemoveChallenger( s32 player_num ) {
	for( s32 & player : gladiator_state.round_challengers ) {
		if( player == player_num ) {
			Swap2( &player, &gladiator_state.round_challengers[ gladiator_state.round_challengers.size() - 1 ] );
			gladiator_state.round_challengers.resize( gladiator_state.round_challengers.size() - 1 );
			return;
		}
	}
}

static void AddLoser( s32 player_num ) {
	RemoveChallenger( player_num );
	gladiator_state.round_losers.add( player_num );
}

static void RemoveLoser( s32 player_num ) {
	for( s32 & player : gladiator_state.round_losers ) {
		if( player == player_num ) {
			Swap2( &player, &gladiator_state.round_losers[ gladiator_state.round_losers.size() - 1 ] );
			gladiator_state.round_losers.resize( gladiator_state.round_losers.size() - 1 );
			return;
		}
	}
}

static void PlayerTeamChanged( s32 player_num, int new_team ) {
	if( new_team != TEAM_PLAYERS ) {
		ChallengersQueueRemovePlayer( player_num );
		RemoveChallenger( player_num );
		RemoveLoser( player_num );
		if( gladiator_state.state != GladiatorRoundState_None && gladiator_state.round_challengers.size() <= 1 ) {
			NewRoundState( GladiatorRoundState_Finished );
		}
	}
	else {
		ChallengersQueueAddPlayer( player_num );
	}
}

static void RoundAnnouncementPrint( const char * string ) {
	// dno what this was doing
	G_CenterPrintMsg( NULL, "%s", string );
}

static void Think() {
	if( gladiator_state.state == GladiatorRoundState_None ) {
		return;
	}

	if( server_gs.gameState.match_state != MatchState_Playing ) {
		EndGame();
		return;
	}

	if( gladiator_state.round_state_end != 0 ) {
		if( gladiator_state.round_state_end < u64( level.time ) ) {
			NewRoundState( GladiatorRoundState( gladiator_state.state + 1 ) );
			return;
		}

		if( gladiator_state.countdown > 0 ) {
			int remainingCounts = int( ( gladiator_state.round_state_end - level.time ) * 0.002f ) + 1;
			remainingCounts = Max2( 0, remainingCounts );

			if( remainingCounts < gladiator_state.countdown ) {
				gladiator_state.countdown = remainingCounts;

				TempAllocator temp = svs.frame_arena.temp();
				if( gladiator_state.countdown <= 3 ) {
					StringHash sound = StringHash( temp( "sounds/gladiator/countdown_{}", gladiator_state.countdown ) );
					G_AnnouncerSound( NULL, sound, GS_MAX_TEAMS, false, NULL );
				}
				G_CenterPrintMsg( NULL, "%d", gladiator_state.countdown );
			}
		}
	}

	if( gladiator_state.state == GladiatorRoundState_Round ) {
		SyncTeamState * team = &server_gs.gameState.teams[ TEAM_PLAYERS ];
		int count = 0;
		for( int i = 0; i < team->num_players; i++ ) {
			if( server_gs.gameState.players[ team->player_indices[ i ] - 1 ].alive ) {
				count++;
			}
		}

		if( count < 2 ) {
			NewRoundState( GladiatorRoundState( gladiator_state.state + 1 ) );
		}
	}
}

static void SetUpWarmup() {
	for( int team = TEAM_PLAYERS; team < GS_MAX_TEAMS; team++ ) {
		G_SpawnQueue_SetTeamSpawnsystem( team, SPAWNSYSTEM_INSTANT, 0, 0, false );
	}
}

static void SetUpCountdown() {
	StringHash sound = "sounds/gladiator/let_the_games_begin";
	G_AnnouncerSound( NULL, sound, GS_MAX_TEAMS, false, NULL );
}

static void NewGame() {
	level.gametype.countdownEnabled = false;

	for( int team = TEAM_PLAYERS; team < GS_MAX_TEAMS; team++ ) {
		G_SpawnQueue_SetTeamSpawnsystem( team, SPAWNSYSTEM_HOLD, 0, 0, true );

		for( int i = 0; i < server_gs.gameState.teams[ team ].num_players; i++ ) {
			s32 player_num = server_gs.gameState.teams[ team ].player_indices[ i ] - 1;
			*G_ClientGetStats( PLAYERENT( player_num ) ) = { };
			G_ClientRespawn( PLAYERENT( player_num ), true );
		}
	}
	server_gs.gameState.round_num = 0;
	NewRound();
}

static void EndGame() {
	NewRoundState( GladiatorRoundState_None );

	if( gladiator_state.round_winner != -1 ) {
		edict_t * winner = PLAYERENT( gladiator_state.round_winner );
		if( G_ClientGetStats( winner )->score == g_scorelimit->integer ) {
			TempAllocator temp = svs.frame_arena.temp();
			RoundAnnouncementPrint( temp( "{} is a true gladiator!", winner->r.client->netname ) );
		}
	}

	level.gametype.countdownEnabled = false;
	for( int i = 0; i < MAX_CLIENTS; i++ ) {
		edict_t * ent = PLAYERENT( i );
		if( G_ISGHOSTING( ent ) || PF_GetClientState( i ) < CS_SPAWNED ) {
			continue;
		}
		G_ClientRespawn( ent, true );
	}

	// GENERIC_UpdateMatchScore();

	G_AnnouncerSound( NULL, "sounds/announcer/game_over", GS_MAX_TEAMS, true, NULL );
}

void G_Aasdf(); // TODO
static void PickRandomArena() {
	if( gladiator_state.randomise ) {
		TempAllocator temp = svs.frame_arena.temp();

		const char * maps_dir = temp( "{}/base/maps/gladiator", RootDirPath() );
		DynamicArray< const char * > maps( &temp );

		ListDirHandle scan = BeginListDir( sys_allocator, maps_dir );

		const char * name;
		bool dir;
		while( ListDirNext( &scan, &name, &dir ) ) {
			// skip ., .., .git, etc
			if( name[ 0 ] == '.' || dir )
				continue;

			if( FileExtension( name ) != ".bsp" && FileExtension( name ) != ".bsp.zst" )
				continue;

			maps.add( temp( "gladiator/{}", StripExtension( name ) ) );
		}

		G_LoadMap( RandomElement( &svs.rng, maps.begin(), maps.size() ) );
		G_Aasdf();
	}
}

static void DoSpinner() {
	// TODO
	Span< const char > loadout = G_GetWorldspawnKey( "loadout" );
	if( loadout != "" ) {
		// GiveFixedLoadout( loadout );
		return;
	}

	WeaponType weap1 = RandomUniform( &svs.rng, Weapon_None + 1, Weapon_Count );
	WeaponType weap2 = RandomUniform( &svs.rng, Weapon_None + 1, Weapon_Count - 1 );

	if( weap2 >= weap1 ) {
		weap2++;
	}

	for( int i = 0; i < MAX_CLIENTS; i++ ) {
		edict_t * ent = PLAYERENT( i );
		if( G_ISGHOSTING( ent ) ) {
			continue;
		}

		G_GiveWeapon( ent, weap1 );
		G_GiveWeapon( ent, weap2 );
		G_SelectWeapon( ent, 0 );
	}
}

static void NewRound() {
	gladiator_state.last_spawn = NULL;
	PickRandomArena();
	server_gs.gameState.round_num++;
	NewRoundState( GladiatorRoundState_Pre );
}

static void NewRoundState( GladiatorRoundState newState ) {
	if( newState > GladiatorRoundState_Post ) {
		NewRound();
		return;
	}

	gladiator_state.state = newState;
	gladiator_state.round_state_start = level.time;

	switch( newState ) {
		case GladiatorRoundState_None: {
			gladiator_state.round_state_end = 0;
			gladiator_state.countdown = 0;
		} break;

		case GladiatorRoundState_Pre: {
			gladiator_state.round_state_end = level.time + countdown_seconds * 1000;
			gladiator_state.countdown = 4;
			level.gametype.removeInactivePlayers = false;

			gladiator_state.round_losers.resize( 0 );

			// pick 2 to 4 players from the queue
			size_t num_players = server_gs.gameState.teams[ TEAM_PLAYERS ].num_players;
			// TODO: this is ugly but cba to figure out
			for( int i = 0; gladiator_state.round_challengers.size() < 4 && gladiator_state.round_challengers.size() < num_players; i++ ) {
				gladiator_state.round_challengers.add( ChallengersQueueNextPlayer() );
			}

			// find topscore
			int topscore = 0;
			for( size_t i = 0; i < num_players; i++ ) {
				s32 player_num = server_gs.gameState.teams[ TEAM_PLAYERS ].player_indices[ i ] - 1;
				int score = G_ClientGetStats( PLAYERENT( player_num ) )->score;
				topscore = Max2( score, topscore );
			}

			// respawn all clients
			for( int i = 0; i < MAX_CLIENTS; i++ ) {
				edict_t * ent = PLAYERENT( i );
				if( PF_GetClientState( i ) >= CS_SPAWNED ) {
					GClip_UnlinkEntity( ent );
				}
			}

			for( size_t i = 0; i < num_players; i++ ) {
				s32 player_num = server_gs.gameState.teams[ TEAM_PLAYERS ].player_indices[ i ] - 1;
				edict_t * ent = PLAYERENT( player_num );
				if( IsChallenger( player_num ) ) {
					G_ClientRespawn( ent, false );
					ent->r.client->ps.pmove.no_shooting_time = countdown_seconds * 1000;
					if( G_ClientGetStats( ent )->score == topscore ) {
						ent->s.model2 = crown_model;
						ent->s.effects |= EF_HAT;
					}
					else {
						ent->s.model2 = EMPTY_HASH;
						ent->s.effects &= ~EF_HAT;
					}
				}
				else {
					G_ClientRespawn( ent, true );
					G_ChasePlayer( ent, NULL, false, 0 );
					ent->r.client->resp.chase.active = true;
				}
			}

			DoSpinner();

			// generate vs string
			TempAllocator temp = svs.frame_arena.temp();
			DynamicString vs_string( &temp, "{}", PLAYERENT( gladiator_state.round_challengers[ 0 ] )->r.client->netname );
			for( int i = 1; i < gladiator_state.round_challengers.size(); i++ ) {
				s32 player = gladiator_state.round_challengers[ i ];
				vs_string.append( temp( " vs. {}", PLAYERENT( player )->r.client->netname ) );
			}

			RoundAnnouncementPrint( vs_string.c_str() );

			// check for match point
			int limit = g_scorelimit->integer;
			RoundType type = RoundType_Normal;
			for( int i = 0; i < num_players; i++ ) {
				s32 player_num = server_gs.gameState.teams[ TEAM_PLAYERS ].player_indices[ i ] - 1;
				int score = G_ClientGetStats( PLAYERENT( player_num ) )->score;
				if( score == limit - 1 ) {
					type = RoundType_MatchPoint;
					break;
				}
			}
			server_gs.gameState.round_type = type;
		} break;

		case GladiatorRoundState_Round: {
			level.gametype.removeInactivePlayers = true;
			gladiator_state.countdown = 0;
			gladiator_state.round_state_end = 0;
			G_AnnouncerSound( NULL, "sounds/gladiator/fight", GS_MAX_TEAMS, false, NULL );
			G_CenterPrintMsg( NULL, "FIGHT!" );
		} break;

		case GladiatorRoundState_Finished: {
			gladiator_state.round_state_end = level.time + 500;
			gladiator_state.countdown = 0;
		} break;

		case GladiatorRoundState_Post: {
			gladiator_state.round_state_end = level.time + 1000;

			// add score to round-winning player
			if( gladiator_state.round_challengers.size() == 0 ) {
				// draw
				gladiator_state.round_winner = -1;
				RoundAnnouncementPrint( "Wow your terrible" );
				G_AnnouncerSound( NULL, "sounds/gladiator/wowyourterrible", GS_MAX_TEAMS, false, NULL );
			}
			else {
				s32 winner = gladiator_state.round_challengers[ 0 ];
				gladiator_state.round_winner = winner;
				edict_t * ent = PLAYERENT( winner );
				G_AnnouncerSound( ent, "sounds/gladiator/score", GS_MAX_TEAMS, true, NULL );
				G_ClientGetStats( ent )->score++;
			}

			for( size_t i = 0; i < gladiator_state.round_losers.size(); i++ ) {
				size_t index = gladiator_state.round_losers.size() - i - 1;
				ChallengersQueueAddPlayer( gladiator_state.round_losers[ index ] );
			}
			gladiator_state.round_losers.clear();
		} break;
	}
}

// --------------

static edict_t * GT_Gladiator_SelectSpawnPoint( edict_t * ent ) {
	if( PF_GetClientState( PLAYERNUM( ent ) ) < CS_SPAWNED ) {
		return NULL;
	}
	edict_t * spawn = G_Find( gladiator_state.last_spawn, &edict_t::classname, "spawn_gladiator" );
	if( spawn == NULL ) {
		spawn = G_Find( NULL, &edict_t::classname, "spawn_gladiator" );
	}
	gladiator_state.last_spawn = spawn;
	if( spawn == NULL ) {
		Com_GGPrint( "null spawn btw" );
	}
	return spawn;
}

static void GT_Gladiator_PlayerRespawned( edict_t *ent, int old_team, int new_team ) {
	if( old_team != new_team ) {
		PlayerTeamChanged( PLAYERNUM( ent ), new_team );
	}

	if( G_ISGHOSTING( ent ) ) {
		return;
	}

	if( server_gs.gameState.match_state != MatchState_Playing ) {
		WeaponType weap1 = RandomUniform( &svs.rng, Weapon_None + 1, Weapon_Count );
		WeaponType weap2 = RandomUniform( &svs.rng, Weapon_None + 1, Weapon_Count - 1 );

		if( weap2 >= weap1 ) {
			weap2++;
		}

		G_GiveWeapon( ent, weap1 );
		G_GiveWeapon( ent, weap2 );
		G_SelectWeapon( ent, 0 );
		G_RespawnEffect( ent );
	}
}

static void GT_Gladiator_PlayerKilled( edict_t * victim, edict_t * attacker, edict_t * inflictor ) {
	if( gladiator_state.state > GladiatorRoundState_None && gladiator_state.state < GladiatorRoundState_Post ) {
		AddLoser( PLAYERNUM( victim ) );
		if( victim->velocity.z < -1600.0f && victim->health < 100 ) {
			G_GlobalSound( CHAN_AUTO, "sounds/gladiator/smackdown" );
		}
	}

	if( gladiator_state.state == GladiatorRoundState_Pre ) {
		G_ClientGetStats( victim )->score--;
		G_LocalSound( victim, CHAN_AUTO, "sounds/gladiator/ouch" );
	}
}

static void GT_Gladiator_ThinkRules() {
	if( G_Match_ScorelimitHit() || G_Match_TimelimitHit() ) {
		G_Match_LaunchState( MatchState( server_gs.gameState.match_state + 1 ) );
		G_ClearCenterPrint( NULL );
	}

	if( server_gs.gameState.match_state >= MatchState_PostMatch ) {
		return;
	}

	Think();
}

static bool GT_Gladiator_MatchStateFinished( MatchState incomingMatchState ) {
	if( server_gs.gameState.match_state <= MatchState_Warmup && incomingMatchState > MatchState_Warmup && incomingMatchState < MatchState_PostMatch ) {
		G_Match_Autorecord_Start();
	}

	if( server_gs.gameState.match_state == MatchState_PostMatch ) {
		G_Match_Autorecord_Stop();
	}

	return true;
}

static void GT_Gladiator_MatchStateStarted() {
	switch( server_gs.gameState.match_state ) {
		case MatchState_Warmup:
			SetUpWarmup();
			break;
		case MatchState_Countdown:
			SetUpCountdown();
			break;
		case MatchState_Playing:
			NewGame();
			break;
		case MatchState_PostMatch:
			EndGame();
			break;
		case MatchState_WaitExit:
			break;
	}
}

static void GT_Gladiator_InitGametype() {
	gladiator_state = { };

	gladiator_state.challengers_queue.init( sys_allocator, MAX_CLIENTS );
	gladiator_state.round_challengers.init( sys_allocator, MAX_CLIENTS );
	gladiator_state.round_losers.init( sys_allocator, MAX_CLIENTS );
	ChallengersQueueClear();

	level.gametype.isTeamBased = false;
	level.gametype.countdownEnabled = false;
	level.gametype.removeInactivePlayers = true;
	level.gametype.selfDamage = false;

	for( int team = TEAM_PLAYERS; team < GS_MAX_TEAMS; team++ ) {
		G_SpawnQueue_SetTeamSpawnsystem( team, SPAWNSYSTEM_INSTANT, 0, 0, false );
	}
	gladiator_state.last_spawn = NULL;

	gladiator_state.randomise = G_GetWorldspawnKey( "randomise_arena" ) != "";

	PickRandomArena();
}

static void GT_Gladiator_Shutdown() {
	gladiator_state.challengers_queue.shutdown();
	gladiator_state.round_challengers.shutdown();
	gladiator_state.round_losers.shutdown();
}

static bool GT_Gladiator_SpawnEntity( StringHash classname, edict_t * ent ) {
	if( classname == "spawn_gladiator" ) {
		DropSpawnToFloor( ent );
		return true;
	}

	return false;
}

Gametype GetGladiatorGametype() {
	Gametype gt = { };

	gt.Init = GT_Gladiator_InitGametype;
	gt.MatchStateStarted = GT_Gladiator_MatchStateStarted;
	gt.MatchStateFinished = GT_Gladiator_MatchStateFinished;
	gt.Think = GT_Gladiator_ThinkRules;
	gt.PlayerRespawned = GT_Gladiator_PlayerRespawned;
	gt.PlayerKilled = GT_Gladiator_PlayerKilled;
	gt.SelectSpawnPoint = GT_Gladiator_SelectSpawnPoint;
	gt.Shutdown = GT_Gladiator_Shutdown;
	gt.SpawnEntity = GT_Gladiator_SpawnEntity;

	gt.isTeamBased = false;
	gt.countdownEnabled = false;
	gt.removeInactivePlayers = true;
	gt.selfDamage = false;

	return gt;
}
