#include "game/g_local.h"
#include "gameshared/gs_public.h"

static const char * bot_names[] = {
	"vic",
	"crizis",
	"jal",

	"MWAGA",

	"Triangel",

	"Perrina",

	"__mute__",

	// twitch subs
	"bes1337",
	"catman1900",
	"hvaholic",
	"ne0ns0up",
	"notmsc",
	"prym88",
	"Quitehatty",
	"Sam!",
	"Scoot",
	"Slice*>",
	"touchpad timma",
};

static edict_t * ConnectFakeClient() {
	char userInfo[ MAX_INFO_STRING ] = "";
	Info_SetValueForKey( userInfo, "name", RandomElement( &svs.rng, bot_names ) );

	int entNum = SVC_FakeConnect( userInfo );
	if( entNum == -1 ) {
		Com_Printf( "AI: Can't spawn the fake client\n" );
		return NULL;
	}
	return game.edicts + entNum;
}

void AI_SpawnBot() {
	if( level.spawnedTimeStamp + 5000 > svs.realtime || !level.canSpawnEntities ) {
		return;
	}

	edict_t * ent = ConnectFakeClient();
	if( ent == NULL )
		return;

	ent->think = NULL;
	ent->nextThink = level.time;
	ent->classname = "bot";
	ent->die = player_die;

	AI_Respawn( ent );

	game.numBots++;
}

void AI_Respawn( edict_t * ent ) {
	ent->r.client->ps.pmove.delta_angles[ 0 ] = 0;
	ent->r.client->ps.pmove.delta_angles[ 1 ] = 0;
	ent->r.client->ps.pmove.delta_angles[ 2 ] = 0;
	ent->r.client->level.last_activity = level.time;
}

static void AI_SpecThink( edict_t * self ) {
	self->nextThink = level.time + 100;

	if( !level.canSpawnEntities )
		return;

	if( self->r.client->team == Team_None ) {
		// try to join a team
		G_Teams_JoinAnyTeam( self, false );

		if( self->r.client->team == Team_None ) { // couldn't join, delay the next think
			self->nextThink = level.time + 100;
		} else {
			self->nextThink = level.time + 1;
		}
		return;
	}

	UserCommand ucmd;
	memset( &ucmd, 0, sizeof( UserCommand ) );

	// set approximate ping and show values
	ucmd.serverTimeStamp = svs.gametime;
	ucmd.msec = (uint8_t)game.frametime;

	ClientThink( self, &ucmd, 0 );
}

static void AI_GameThink( edict_t * self ) {
	if( server_gs.gameState.match_state <= MatchState_Warmup ) {
		bool all_humans_ready = true;
		bool any_humans = false;

		for( int i = 0; i < server_gs.maxclients; i++ ) {
			const edict_t * player = PLAYERENT( i );
			if( !player->r.inuse || ( player->s.svflags & SVF_FAKECLIENT ) ) {
				continue;
			}

			any_humans = true;
			if( !level.ready[ PLAYERNUM( player ) ] ) {
				all_humans_ready = false;
				break;
			}
		}

		if( any_humans && all_humans_ready ) {
			G_Match_Ready( self );
		}
	}

	UserCommand ucmd;
	memset( &ucmd, 0, sizeof( UserCommand ) );

	// set up for pmove
	ucmd.angles[ 0 ] = (short)ANGLE2SHORT( self->s.angles.x ) - self->r.client->ps.pmove.delta_angles[ 0 ];
	ucmd.angles[ 1 ] = (short)ANGLE2SHORT( self->s.angles.y ) - self->r.client->ps.pmove.delta_angles[ 1 ];
	ucmd.angles[ 2 ] = (short)ANGLE2SHORT( self->s.angles.z ) - self->r.client->ps.pmove.delta_angles[ 2 ];

	self->r.client->ps.pmove.delta_angles[ 0 ] = 0;
	self->r.client->ps.pmove.delta_angles[ 1 ] = 0;
	self->r.client->ps.pmove.delta_angles[ 2 ] = 0;

	// set approximate ping and show values
	ucmd.msec = u8( game.frametime );
	ucmd.serverTimeStamp = svs.gametime;

	ClientThink( self, &ucmd, 0 );
	self->nextThink = level.time + 1;
}

void AI_Think( edict_t * self ) {
	if( G_ISGHOSTING( self ) ) {
		AI_SpecThink( self );
	}
	else {
		AI_GameThink( self );
	}
}
