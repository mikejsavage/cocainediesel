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
	ent->r.client->level.last_activity = level.time;
}

void AI_Think( edict_t * self ) {
	if( self->r.client->team == Team_None ) {
		G_Teams_JoinAnyTeam( self, false );
	}
	else if( server_gs.gameState.match_state == MatchState_Warmup && !level.ready[ PLAYERNUM( self ) ] ) {
		G_Match_Ready( self );
	}

	UserCommand ucmd = { };

	// set approximate ping and show values
	ucmd.angles = EulerDegrees2( self->s.angles.pitch, self->s.angles.yaw );
	ucmd.msec = u8( game.frametime );
	ucmd.serverTimeStamp = svs.gametime;

	ClientThink( self, &ucmd, 0 );

	self->nextThink = level.time + 1;
}
