/*
Copyright (C) 1997-2001 Id Software, Inc.

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

#include "game/g_local.h"

static void G_Obituary( edict_t * victim, edict_t * attacker, int topAssistEntNo, DamageType mod, bool wallbang ) {
	TempAllocator temp = svs.frame_arena.temp();
	PF_GameCmd( NULL, temp( "obry {} {} {} {} {} {}", ENTNUM( victim ), ENTNUM( attacker ), topAssistEntNo, mod.encoded, wallbang ? 1 : 0, Random64( &svs.rng ) ) );
}

static void ClientObituary( edict_t * self, edict_t * inflictor, edict_t * attacker, int topAssistEntNo, DamageType damage_type ) {
	bool wallbang = ( damageFlagsOfDeath & DAMAGE_WALLBANG ) != 0;

	if( attacker && attacker->r.client ) {
		if( attacker != self ) { // regular death message
			self->enemy = attacker;
			if( is_dedicated_server ) {
				Com_GGPrint( "\"{}\" \"{}\" {} {}", self->r.client->netname, attacker->r.client->netname, damage_type.encoded, wallbang ? 1 : 0 );
			}
		}
		else {      // suicide
			self->enemy = NULL;
			if( is_dedicated_server ) {
				Com_GGPrint( "\"{}\" suicide {}", self->r.client->netname, damage_type.encoded );
			}

			G_PositionedSound( self->s.origin, "sounds/trombone/sad" );
		}

		G_Obituary( self, attacker, topAssistEntNo, damage_type, wallbang );
	}
	else {      // wrong place, suicide, etc.
		self->enemy = NULL;
		if( is_dedicated_server ) {
			Com_GGPrint( "\"{}\" suicide {}", self->r.client->netname, damage_type.encoded );
		}

		G_Obituary( self, attacker == self ? self : world, topAssistEntNo, damage_type, wallbang );
	}
}


//=======================================================
// DEAD BODIES
//=======================================================

static edict_t *CreateCorpse( edict_t *ent, edict_t *attacker, DamageType damage_type, int damage ) {
	assert( ent->s.type == ET_PLAYER );

	edict_t * body = G_Spawn();

	body->classname = "body";
	body->s.type = ET_CORPSE;
	body->health = ent->health;
	body->mass = ent->mass;
	body->r.owner = ent->r.owner;
	body->s.team = ent->s.team;
	body->s.scale = ent->s.scale;
	body->s.svflags = SVF_CORPSE | SVF_BROADCAST;
	body->activator = ent;
	if( g_deadbody_followkiller->integer ) {
		body->enemy = attacker;
	}

	//use flat yaw
	body->s.angles.y = ent->s.angles.y;

	//copy player position and box size
	body->s.origin = ent->s.origin;
	body->olds.origin = ent->s.origin;
	body->r.mins = ent->r.mins;
	body->r.maxs = ent->r.maxs;
	body->r.absmin = ent->r.absmin;
	body->r.absmax = ent->r.absmax;
	body->r.size = ent->r.size;
	body->velocity = ent->velocity;
	body->r.maxs.z = body->r.mins.z + 8;

	body->r.solid = SOLID_NOT;
	body->takedamage = DAMAGE_NO;
	body->movetype = MOVETYPE_TOSS;

	body->s.teleported = true;
	body->s.ownerNum = ent->s.number;

	bool gib = damage_type == Weapon_Railgun || damage_type == WorldDamage_Trigger || damage_type == WorldDamage_Telefrag
		|| damage_type == WorldDamage_Explosion || damage_type == WorldDamage_Spike ||
		( ( damage_type == Weapon_RocketLauncher || damage_type == Weapon_GrenadeLauncher ) && damage >= 20 );

	if( gib ) {
		ThrowSmallPileOfGibs( body, knockbackOfDeath, damage );

		body->nextThink = level.time + 3000 + RandomFloat01( &svs.rng ) * 3000;
		body->deadflag = DEAD_DEAD;
	}

	u64 parm = Random64( &svs.rng ) << 1;
	if( damage_type == WorldDamage_Void ) {
		parm |= 1;
	}

	edict_t * event = G_SpawnEvent( EV_DIE, parm, NULL );
	event->s.svflags |= SVF_BROADCAST;
	event->s.ownerNum = body->s.number;

	ent->s.ownerNum = body->s.number;

	// bit of a hack, if we're not in warmup, leave the body with no think. think self destructs
	// after a timeout, but if we leave, next bomb round will call G_ResetLevel() cleaning up
	if( server_gs.gameState.match_state != MatchState_Playing ) {
		body->nextThink = level.time + 3500;
		body->think = G_FreeEdict; // body self destruction countdown
	}

	GClip_LinkEntity( body );
	return body;
}

void player_die( edict_t *ent, edict_t *inflictor, edict_t *attacker, int topAssistorEntNo, DamageType damage_type, int damage ) {
	snap_edict_t snap_backup = ent->snap;
	client_snapreset_t resp_snap_backup = ent->r.client->resp.snap;

	ent->avelocity = Vec3( 0.0f );

	ent->s.angles.x = 0;
	ent->s.angles.z = 0;
	ent->s.sound = EMPTY_HASH;

	ent->r.solid = SOLID_NOT;

	// player death
	ClientObituary( ent, inflictor, attacker, topAssistorEntNo, damage_type );

	// create a corpse
	CreateCorpse( ent, attacker, damage_type, damage );
	ent->enemy = NULL;

	ent->s.angles.y = ent->r.client->ps.viewangles.y = LookAtKillerYAW( ent, inflictor, attacker );

	// go ghost (also resets snap)
	G_GhostClient( ent );

	ent->deathTimeStamp = level.time;

	ent->velocity = Vec3( 0.0f );
	ent->avelocity = Vec3( 0.0f );
	ent->snap = snap_backup;
	ent->r.client->resp.snap = resp_snap_backup;
	ent->r.client->resp.snap.buttons = 0;
	GClip_LinkEntity( ent );
}

void G_Client_UpdateActivity( gclient_t *client ) {
	if( !client ) {
		return;
	}
	client->level.last_activity = level.time;
}

void G_Client_InactivityRemove( gclient_t *client ) {
	if( !client ) {
		return;
	}

	if( PF_GetClientState( client - game.clients ) < CS_SPAWNED ) {
		return;
	}

	if( client->ps.pmove.pm_type != PM_NORMAL ) {
		return;
	}

	if( g_inactivity_maxtime->modified ) {
		if( g_inactivity_maxtime->number <= 0.0f ) {
			Cvar_ForceSet( "g_inactivity_maxtime", "0.0" );
		} else if( g_inactivity_maxtime->number < 15.0f ) {
			Cvar_ForceSet( "g_inactivity_maxtime", "15.0" );
		}

		g_inactivity_maxtime->modified = false;
	}

	if( g_inactivity_maxtime->number == 0.0f ) {
		return;
	}

	if( ( server_gs.gameState.match_state != MatchState_Playing ) || !level.gametype.removeInactivePlayers ) {
		return;
	}

	// inactive for too long
	if( client->level.last_activity && client->level.last_activity + ( g_inactivity_maxtime->number * 1000 ) < level.time ) {
		if( client->team >= TEAM_PLAYERS && client->team < GS_MAX_TEAMS ) {
			edict_t *ent = &game.edicts[ client - game.clients + 1 ];

			G_Teams_SetTeam( ent, TEAM_SPECTATOR );

			G_PrintMsg( NULL, "%s has been moved to spectator after %.1f seconds of inactivity\n", client->netname, g_inactivity_maxtime->number );
		}
	}
}

score_stats_t * G_ClientGetStats( edict_t * ent ) {
	return &ent->r.client->level.stats;
}

void G_ClientClearStats( edict_t * ent ) {
	if( !ent || !ent->r.client ) {
		return;
	}

	memset( G_ClientGetStats( ent ), 0, sizeof( score_stats_t ) );
}

void G_GhostClient( edict_t *ent ) {
	ent->movetype = MOVETYPE_NONE;
	ent->r.solid = SOLID_NOT;

	memset( &ent->snap, 0, sizeof( ent->snap ) );
	memset( &ent->r.client->resp.snap, 0, sizeof( ent->r.client->resp.snap ) );
	memset( &ent->r.client->resp.chase, 0, sizeof( ent->r.client->resp.chase ) );
	ent->r.client->resp.old_waterlevel = 0;
	ent->r.client->resp.old_watertype = 0;

	ent->s.type = ET_GHOST;
	ent->s.effects = 0;
	ent->s.sound = EMPTY_HASH;
	ent->viewheight = 0;
	ent->takedamage = DAMAGE_NO;

	ClearInventory( &ent->r.client->ps );

	GClip_LinkEntity( ent );
}

void G_ClientRespawn( edict_t *self, bool ghost ) {
	GT_CallPlayerRespawning( self );

	self->s.svflags &= ~SVF_NOCLIENT;

	//if invalid be spectator
	if( self->r.client->team < 0 || self->r.client->team >= GS_MAX_TEAMS ) {
		self->r.client->team = TEAM_SPECTATOR;
	}

	// force ghost always to true when in spectator team
	if( self->r.client->team == TEAM_SPECTATOR ) {
		ghost = true;
	}

	int old_team = self->s.team;

	GClip_UnlinkEntity( self );

	gclient_t * client = self->r.client;

	memset( &client->resp, 0, sizeof( client->resp ) );
	memset( &client->ps, 0, sizeof( client->ps ) );
	client->resp.timeStamp = level.time;
	client->ps.playerNum = PLAYERNUM( self );

	int old_svflags = self->s.svflags;
	G_InitEdict( self );
	self->s.svflags = old_svflags;

	// relink client struct
	self->r.client = &game.clients[PLAYERNUM( self )];

	// update team
	self->s.team = client->team;

	self->groundentity = NULL;
	self->takedamage = DAMAGE_AIM;
	self->die = player_die;
	self->viewheight = playerbox_stand_viewheight;
	self->r.inuse = true;
	self->mass = PLAYER_MASS;
	self->r.clipmask = MASK_PLAYERSOLID;
	self->waterlevel = 0;
	self->watertype = 0;
	self->s.svflags &= ~SVF_CORPSE;
	self->enemy = NULL;
	self->r.owner = NULL;
	self->max_health = 100;
	self->health = self->max_health;


	if( self->s.svflags & SVF_FAKECLIENT ) {
		self->classname = "fakeclient";
	} else {
		self->classname = "player";
	}

	self->r.mins = playerbox_stand_mins;
	self->r.maxs = playerbox_stand_maxs;
	self->velocity = Vec3( 0.0f );
	self->avelocity = Vec3( 0.0f );

	client->ps.POVnum = ENTNUM( self );

	// set movement info
	client->ps.pmove.max_speed = -1;

	if( ghost ) {
		G_GhostClient( self );
		self->s.svflags &= ~SVF_FORCETEAM;
		self->movetype = MOVETYPE_NOCLIP;
	}
	else {
		self->s.type = ET_PLAYER;
		self->s.svflags |= SVF_FORCETEAM;
		self->r.solid = SOLID_YES;
		self->movetype = MOVETYPE_PLAYER;
		client->ps.pmove.features = PMFEAT_DEFAULT;
	}

	ClientUserinfoChanged( self, client->userinfo );

	if( old_team != self->s.team ) {
		G_Teams_UpdateMembersList();
	}

	if( !ghost ) {
		const edict_t * spawnpoint;
		Vec3 spawn_origin, spawn_angles;
		SelectSpawnPoint( self, &spawnpoint, &spawn_origin, &spawn_angles );
		client->ps.pmove.origin = spawn_origin;
		self->s.origin = spawn_origin;

		// set angles
		self->s.angles.x = 0.0f;
		self->s.angles.y = AngleNormalize360( spawn_angles.y );
		self->s.angles.z = 0.0f;
		client->ps.viewangles = Vec3( self->s.angles );

		KillBox( self, WorldDamage_Telefrag, Vec3( 0.0f ) );
	}
	else {
		G_ChasePlayer( self );
	}

	// set the delta angle
	client->ps.pmove.delta_angles[ 0 ] = ANGLE2SHORT( client->ps.viewangles.x ) - client->ucmd.angles[ 0 ];
	client->ps.pmove.delta_angles[ 1 ] = ANGLE2SHORT( client->ps.viewangles.y ) - client->ucmd.angles[ 1 ];
	client->ps.pmove.delta_angles[ 2 ] = ANGLE2SHORT( client->ps.viewangles.z ) - client->ucmd.angles[ 2 ];

	self->s.teleported = true;

	memset( self->recent_attackers, 0, sizeof( self->recent_attackers ) );

	GClip_LinkEntity( self );

	if( self->s.svflags & SVF_FAKECLIENT ) {
		AI_Respawn( self );
	}

	GT_CallPlayerRespawned( self, old_team, self->s.team );
}

bool G_PlayerCanTeleport( edict_t *player ) {
	if( !player->r.client ) {
		return false;
	}
	if( player->r.client->ps.pmove.pm_type > PM_SPECTATOR ) {
		return false;
	}
	return true;
}

void G_TeleportPlayer( edict_t *player, edict_t *dest ) {
	gclient_t *client = player->r.client;

	if( !dest ) {
		return;
	}
	if( !client ) {
		return;
	}

	// draw the teleport entering effect
	G_TeleportEffect( player, false );

	//
	// teleport the player
	//

	// from racesow - use old pmove velocity
	Vec3 velocity = client->old_pmove.velocity;

	velocity.z = 0; // ignore vertical velocity
	float speed = Length( velocity );

	mat3_t axis;
	AnglesToAxis( dest->s.angles, axis );
	client->ps.pmove.velocity = FromQFAxis( axis, AXIS_FORWARD ) * ( speed );

	client->ps.viewangles = dest->s.angles;
	client->ps.pmove.origin = dest->s.origin;

	// set the delta angle
	client->ps.pmove.delta_angles[ 0 ] = ANGLE2SHORT( client->ps.viewangles.x ) - client->ucmd.angles[ 0 ];
	client->ps.pmove.delta_angles[ 1 ] = ANGLE2SHORT( client->ps.viewangles.y ) - client->ucmd.angles[ 1 ];
	client->ps.pmove.delta_angles[ 2 ] = ANGLE2SHORT( client->ps.viewangles.z ) - client->ucmd.angles[ 2 ];

	client->ps.pmove.pm_flags |= PMF_TIME_TELEPORT;
	client->ps.pmove.pm_time = 1; // force the minimum no control delay
	player->s.teleported = true;

	// update the entity from the pmove
	player->s.angles = client->ps.viewangles;
	player->s.origin = client->ps.pmove.origin;
	player->olds.origin = client->ps.pmove.origin;
	player->velocity = client->ps.pmove.velocity;

	// unlink to make sure it can't possibly interfere with KillBox
	GClip_UnlinkEntity( player );

	// kill anything at the destination
	KillBox( player, WorldDamage_Telefrag, Vec3( 0.0f ) );

	GClip_LinkEntity( player );

	// add the teleport effect at the destination
	G_TeleportEffect( player, true );
}

//==============================================================

/*
* ClientBegin
* called when a client has finished connecting, and is ready
* to be placed into the game.  This will happen every level load.
*/
void ClientBegin( edict_t *ent ) {
	gclient_t *client = ent->r.client;
	ent->r.inuse = true;

	memset( &client->ucmd, 0, sizeof( client->ucmd ) );
	memset( &client->level, 0, sizeof( client->level ) );
	client->level.timeStamp = level.time;
	G_Client_UpdateActivity( client ); // activity detected

	if( !G_Teams_JoinAnyTeam( ent, true ) ) {
		G_Teams_JoinTeam( ent, TEAM_SPECTATOR );
	}

	G_PrintMsg( NULL, "%s entered the game\n", client->netname );

	GT_CallPlayerConnected( ent );

	client->connecting = false;

	G_ClientEndSnapFrame( ent ); // make sure all view stuff is valid
}

/*
* strip_highchars
* kill all chars with code >= 127
* (127 is not exactly a highchar, but we drop it, too)
*/
static void strip_highchars( char *in ) {
	char *out = in;
	for( ; *in; in++ )
		if( ( unsigned char )*in < 127 ) {
			*out++ = *in;
		}
	*out = 0;
}

static int G_SanitizeUserString( char *string, size_t size ) {
	// life is hard, UTF-8 will have to go
	strip_highchars( string );

	Q_trim( string );

	// require at least one non-whitespace ascii char in the string
	// (this will upset people who would like to have a name entirely in a non-latin
	// script, but it makes damn sure you can't get an empty name by exploiting some
	// utf-8 decoder quirk)
	int c_ascii = 0;
	for( int i = 0; string[i]; i++ ) {
		if( string[i] > 32 && string[i] < 127 ) {
			c_ascii++;
		}
	}

	return c_ascii;
}

static void G_SetName( edict_t * ent, const char * original_name ) {
	if( !ent->r.client ) {
		return;
	}

	if( original_name == NULL ) {
		original_name = "";
	}

	char name[ MAX_NAME_CHARS + 1 ];
	Q_strncpyz( name, original_name, sizeof( name ) );

	int c_ascii = G_SanitizeUserString( name, sizeof( name ) );
	if( !c_ascii ) {
		Q_strncpyz( name, "Player", sizeof( name ) );
	}

	Q_strncpyz( ent->r.client->netname, name, sizeof( ent->r.client->netname ) );

	int trynum = 0;
	while( trynum < MAX_CLIENTS ) {
		bool ok = true;

		for( int i = 0; i < server_gs.maxclients; i++ ) {
			const edict_t * other = game.edicts + 1 + i;
			if( !other->r.inuse || !other->r.client || other == ent )
				continue;

			if( !StrEqual( other->r.client->netname, ent->r.client->netname ) )
				continue;

			char suffix[ MAX_NAME_CHARS + 1 ];
			snprintf( suffix, sizeof( suffix ), "(%i)", trynum + 2 );

			Span< const char > prefix = MakeSpan( name );
			prefix.n = Min2( MAX_NAME_CHARS - strlen( suffix ), prefix.n );

			ggformat( ent->r.client->netname, sizeof( ent->r.client->netname ), "{}{}", prefix, suffix );

			trynum++;
			ok = false;
			break;
		}

		if( ok )
			break;
	}
}

static void G_UpdatePlayerInfoString( int playerNum ) {
	const gclient_t * client = &game.clients[ playerNum ];
	PF_ConfigString( CS_PLAYERINFOS + playerNum, client->netname );
}

/*
* ClientUserinfoChanged
* called whenever the player updates a userinfo variable.
*
* The game can override any of the settings in place
* (forcing skins or names, etc) before copying it off.
*/
void ClientUserinfoChanged( edict_t *ent, char *userinfo ) {
	char oldname[MAX_INFO_VALUE];
	gclient_t *cl;

	assert( ent && ent->r.client );
	assert( userinfo && Info_Validate( userinfo ) );

	// check for malformed or illegal info strings
	if( !Info_Validate( userinfo ) ) {
		PF_DropClient( ent, "Error: Invalid userinfo" );
		return;
	}

	cl = ent->r.client;

	// set name, it's validated and possibly changed first
	Q_strncpyz( oldname, cl->netname, sizeof( oldname ) );
	G_SetName( ent, Info_ValueForKey( userinfo, "name" ) );
	if( oldname[0] && !StrCaseEqual( oldname, cl->netname ) && !CheckFlood( ent, false ) ) {
		G_PrintMsg( NULL, "%s is now known as %s\n", oldname, cl->netname );
	}
	if( !Info_SetValueForKey( userinfo, "name", cl->netname ) ) {
		PF_DropClient( ent, "Error: Couldn't set userinfo (name)" );
		return;
	}

	// save off the userinfo in case we want to check something later
	Q_strncpyz( cl->userinfo, userinfo, sizeof( cl->userinfo ) );

	G_UpdatePlayerInfoString( PLAYERNUM( ent ) );
}


/*
* ClientConnect
* Called when a player begins connecting to the server.
* The game can refuse entrance to a client by returning false.
* If the client is allowed, the connection process will continue
* and eventually get to ClientBegin()
* Changing levels will NOT cause this to be called again, but
* loadgames will.
*/
bool ClientConnect( edict_t *ent, char *userinfo, const NetAddress & address, bool fakeClient ) {
	assert( ent );
	assert( userinfo && Info_Validate( userinfo ) );

	// verify that server gave us valid data
	if( !Info_Validate( userinfo ) ) {
		Info_SetValueForKey( userinfo, "rejmsg", "Invalid userinfo" );
		return false;
	}

	// check for a password
	char * value = Info_ValueForKey( userinfo, "password" );
	if( !fakeClient && ( *sv_password->value && ( !value || !StrEqual( sv_password->value, value ) ) ) ) {
		if( value && value[0] ) {
			Info_SetValueForKey( userinfo, "rejmsg", "Incorrect password" );
		} else {
			Info_SetValueForKey( userinfo, "rejmsg", "Password required" );
		}
		return false;
	}

	// they can connect

	G_InitEdict( ent );
	ent->r.solid = SOLID_NOT;
	ent->r.client = game.clients + PLAYERNUM( ent );
	ent->s.svflags = ( SVF_NOCLIENT | ( fakeClient ? SVF_FAKECLIENT : 0 ) );
	memset( ent->r.client, 0, sizeof( gclient_t ) );
	ent->r.client->ps.playerNum = PLAYERNUM( ent );
	ent->r.client->connecting = true;
	ent->r.client->team = TEAM_SPECTATOR;
	G_Client_UpdateActivity( ent->r.client ); // activity detected

	ClientUserinfoChanged( ent, userinfo );

	if( !fakeClient ) {
		char message[MAX_STRING_CHARS];

		snprintf( message, sizeof( message ), "%s connected", ent->r.client->netname );

		G_PrintMsg( NULL, "%s\n", message );

		Com_GGPrint( "{} connected from {}", ent->r.client->netname, address );
	}

	G_CallVotes_ResetClient( PLAYERNUM( ent ) );

	return true;
}

/*
* ClientDisconnect
* Called when a player drops from the server.
* Will not be called between levels.
*/
void ClientDisconnect( edict_t *ent, const char *reason ) {
	if( !ent->r.client || !ent->r.inuse ) {
		return;
	}

	if( !reason ) {
		G_PrintMsg( NULL, "%s disconnected\n", ent->r.client->netname );
	} else {
		G_PrintMsg( NULL, "%s disconnected (%s)\n", ent->r.client->netname, reason );
	}

	if( ent->s.type == ET_PLAYER ) {
		CreateCorpse( ent, NULL, WorldDamage_Suicide, 0 ); //create a corpse
	}
	
	ent->r.inuse = false;
	ent->s.svflags = SVF_NOCLIENT;

	memset( ent->r.client, 0, sizeof( *ent->r.client ) );
	ent->r.client->ps.playerNum = PLAYERNUM( ent );

	PF_ConfigString( CS_PLAYERINFOS + PLAYERNUM( ent ), "" );
	GClip_UnlinkEntity( ent );

	G_Match_CheckReadys();
}

//==============================================================

void G_PredictedEvent( int entNum, int ev, u64 parm ) {
	assert( ev != EV_FIREWEAPON );

	edict_t *ent = &game.edicts[entNum];

	switch( ev ) {
		case EV_SMOOTHREFIREWEAPON: // update the firing
			G_FireWeapon( ent, parm );
			break; // don't send the event

		case EV_SUICIDE_BOMB_EXPLODE: {
			ent->health = 0;

			edict_t stupid = { };
			stupid.s.origin = ent->s.origin;
			stupid.projectileInfo.maxDamage = 100;
			stupid.projectileInfo.minDamage = 25;
			stupid.projectileInfo.maxKnockback = 150;
			stupid.projectileInfo.minKnockback = 75;
			stupid.projectileInfo.radius = 150;

			G_RadiusDamage( &stupid, ent, NULL, ent, Gadget_SuicideBomb );

			G_Killed( ent, ent, ent, 0, Gadget_SuicideBomb, 10000 );
			G_AddEvent( ent, ev, parm, true );
		} break;

		default:
			G_AddEvent( ent, ev, parm, true );
			break;
	}
}

void G_PredictedFireWeapon( int entNum, u64 parm ) {
	edict_t * ent = &game.edicts[ entNum ];
	G_FireWeapon( ent, parm );

	Vec3 start = ent->s.origin;
	start.z += ent->r.client->ps.viewheight;

	edict_t * event = G_SpawnEvent( EV_FIREWEAPON, parm, &start );
	event->s.ownerNum = entNum;
	event->s.origin2 = ent->r.client->ps.viewangles;
	event->s.team = ent->s.team;
}

void G_PredictedUseGadget( int entNum, GadgetType gadget, u64 parm ) {
	edict_t * ent = &game.edicts[ entNum ];
	G_UseGadget( ent, gadget, parm );

	Vec3 start = ent->s.origin;
	start.z += ent->r.client->ps.viewheight;

	edict_t * event = G_SpawnEvent( EV_USEGADGET, ( parm << 8 ) | gadget, &start );
	event->s.ownerNum = entNum;
	event->s.origin2 = ent->r.client->ps.viewangles;
	event->s.team = ent->s.team;
}

void G_GiveWeapon( edict_t * ent, WeaponType weapon ) {
	SyncPlayerState * ps = &ent->r.client->ps;

	for( size_t i = 0; i < ARRAY_COUNT( ps->weapons ); i++ ) {
		if( ps->weapons[ i ].weapon == weapon || ps->weapons[ i ].weapon == Weapon_None ) {
			ps->weapons[ i ].weapon = weapon;
			ps->weapons[ i ].ammo = GS_GetWeaponDef( weapon )->clip_size;
			break;
		}
	}
}

void G_SelectWeapon( edict_t * ent, int index ) {
	SyncPlayerState * ps = &ent->r.client->ps;

	if( ps->weapons[ index ].weapon == Weapon_None ) {
		return;
	}

	ps->weapon = Weapon_None;
	ps->pending_weapon = ps->weapons[ index ].weapon;
	ps->weapon_state = WeaponState_DispatchQuiet;
	ps->weapon_state_time = 0;
}

void ClientThink( edict_t *ent, UserCommand *ucmd, int timeDelta ) {
	TracyZoneScoped;

	gclient_t *client;
	int i, j;
	static pmove_t pm;
	int delta, count;

	client = ent->r.client;

	client->ps.POVnum = ENTNUM( ent );
	client->ps.playerNum = PLAYERNUM( ent );

	// anti-lag
	if( ent->s.svflags & SVF_FAKECLIENT ) {
		client->timeDelta = 0;
	} else {
		int fixedNudge = game.snapFrameTime * 0.5; // fixme: find where this nudge comes from.

		// add smoothing to timeDelta between the last few ucmds and a small fine-tuning nudge.
		int nudge = fixedNudge + g_antilag_timenudge->integer;
		timeDelta = Clamp( -g_antilag_maxtimedelta->integer, timeDelta + nudge, 0 );

		// smooth using last valid deltas
		i = client->timeDeltasHead - 6;
		if( i < 0 ) {
			i = 0;
		}
		for( count = 0, delta = 0; i < client->timeDeltasHead; i++ ) {
			if( client->timeDeltas[i & G_MAX_TIME_DELTAS_MASK] < 0 ) {
				delta += client->timeDeltas[i & G_MAX_TIME_DELTAS_MASK];
				count++;
			}
		}

		if( !count ) {
			client->timeDelta = timeDelta;
		} else {
			delta /= count;
			client->timeDelta = ( delta + timeDelta ) * 0.5;
		}

		client->timeDeltas[client->timeDeltasHead & G_MAX_TIME_DELTAS_MASK] = timeDelta;
		client->timeDeltasHead++;
	}

	client->timeDelta = Clamp( -g_antilag_maxtimedelta->integer, client->timeDelta, 0 );

	// update activity if he touched any controls
	if( ucmd->forwardmove != 0 || ucmd->sidemove != 0 || ucmd->buttons != 0 ||
		client->ucmd.angles[PITCH] != ucmd->angles[PITCH] || client->ucmd.angles[YAW] != ucmd->angles[YAW] ) {
		G_Client_UpdateActivity( client );
	}

	client->ucmd = *ucmd;

	// (is this really needed?:only if not cared enough about ps in the rest of the code)
	// refresh player state position from the entity
	client->ps.pmove.origin = ent->s.origin;
	client->ps.pmove.velocity = ent->velocity;
	client->ps.viewangles = ent->s.angles;

	if( server_gs.gameState.match_state >= MatchState_PostMatch || GS_MatchPaused( &server_gs )
		|| ( ent->movetype != MOVETYPE_PLAYER && ent->movetype != MOVETYPE_NOCLIP ) ) {
		client->ps.pmove.pm_type = PM_FREEZE;
	} else if( ent->movetype == MOVETYPE_NOCLIP ) {
		client->ps.pmove.pm_type = PM_SPECTATOR;
	} else {
		client->ps.pmove.pm_type = PM_NORMAL;
	}

	// set up for pmove
	memset( &pm, 0, sizeof( pmove_t ) );
	pm.playerState = &client->ps;
	pm.cmd = *ucmd;
	pm.scale = ent->s.scale;

	// perform a pmove
	Pmove( &server_gs, &pm );

	// save results of pmove
	client->old_pmove = client->ps.pmove;

	// update the entity with the new position
	ent->s.origin = client->ps.pmove.origin;
	ent->velocity = client->ps.pmove.velocity;
	ent->s.angles = client->ps.viewangles;
	ent->viewheight = client->ps.viewheight;
	ent->r.mins = pm.mins;
	ent->r.maxs = pm.maxs;

	ent->waterlevel = pm.waterlevel;
	ent->watertype = pm.watertype;
	if( pm.groundentity == -1 ) {
		ent->groundentity = NULL;
	} else {
		ent->groundentity = &game.edicts[pm.groundentity];
		ent->groundentity_linkcount = ent->groundentity->linkcount;
	}

	GClip_LinkEntity( ent );

	// fire touch functions
	if( ent->movetype != MOVETYPE_NOCLIP ) {
		edict_t *other;

		// touch other objects
		for( i = 0; i < pm.numtouch; i++ ) {
			other = &game.edicts[pm.touchents[i]];
			for( j = 0; j < i; j++ ) {
				if( &game.edicts[pm.touchents[j]] == other ) {
					break;
				}
			}
			if( j != i ) {
				continue; // duplicated

			}
			// player can't touch projectiles, only projectiles can touch the player
			G_CallTouch( other, ent, NULL, 0 );
		}
	}

	if( ent->movetype == MOVETYPE_PLAYER ) {
		if( ent->s.origin.z <= -1024 ) {
			G_Damage( ent, world, world, Vec3( 0.0f ), Vec3( 0.0f ), ent->s.origin, 1337, 0, 0, WorldDamage_Void );
		}
	}

	UpdateWeapons( &server_gs, &client->ps, *ucmd, client->timeDelta );
	ent->s.weapon = client->ps.weapon;

	client->resp.snap.buttons |= ucmd->buttons;
}

void G_ClientThink( edict_t *ent ) {
	if( PF_GetClientState( PLAYERNUM( ent ) ) < CS_SPAWNED ) {
		return;
	}

	ent->r.client->ps.POVnum = ENTNUM( ent ); // set self

	// run bots thinking with the rest of clients
	if( ent->s.svflags & SVF_FAKECLIENT ) {
		AI_Think( ent );
	}

	SV_ExecuteClientThinks( PLAYERNUM( ent ) );
}

void G_CheckClientRespawnClick( edict_t *ent ) {
	if( !ent->r.inuse || !ent->r.client || !G_IsDead( ent ) ) {
		return;
	}

	if( server_gs.gameState.match_state >= MatchState_PostMatch ) {
		return;
	}

	if( level.gametype.autoRespawn ) {
		constexpr int min_delay = 600;
		constexpr int max_delay = 6000;

		bool clicked = level.time > ent->deathTimeStamp + min_delay && ( ent->r.client->resp.snap.buttons & BUTTON_ATTACK1 );
		bool timeout = level.time > ent->deathTimeStamp + max_delay;
		if( clicked || timeout ) {
			G_ClientRespawn( ent, false );
		}
	}
	else {
		if( level.time >= ent->deathTimeStamp + 3000 ) {
			G_ClientRespawn( ent, true );
		}
	}
}


void G_GivePerk( edict_t * ent, PerkType perk ) {
	const PerkDef * def = GetPerkDef( perk );

	ent->r.client->ps.perk = perk;
	ent->r.client->ps.pmove.stamina = 1.0f; //max stamina
	ent->r.client->ps.pmove.stamina_stored = 0.0f;
	ent->r.client->ps.pmove.stamina_state = Stamina_Normal;

	float old_max_health = ent->max_health;
	ent->s.scale = def->scale;
	ent->max_health = def->health;

	if( old_max_health == 0.0f ) {
		ent->health = ent->max_health;
	} else {
		ent->health = ent->health * ent->max_health / old_max_health;
	}

	ent->mass = PLAYER_MASS * def->weight;
}
