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

#define PLAYER_MASS 200

/*
* ClientObituary
*/
static void ClientObituary( edict_t *self, edict_t *inflictor, edict_t *attacker ) {
	char message[64];
	char message2[64];

	int mod = meansOfDeath;

	GS_Obituary( self, attacker, mod, message, message2 );

	// duplicate message at server console for logging
	if( attacker && attacker->r.client ) {
		if( attacker != self ) { // regular death message
			self->enemy = attacker;
			if( GAME_IMPORT.is_dedicated_server ) {
				Com_Printf( "%s %s %s%s\n", self->r.client->netname, message,
						  attacker->r.client->netname, message2 );
			}
		} else {      // suicide
			self->enemy = NULL;
			if( GAME_IMPORT.is_dedicated_server ) {
				Com_Printf( "%s %s\n", self->r.client->netname, message );
			}

			G_PositionedSound( self->s.origin, CHAN_AUTO, "sounds/trombone/sad" );
		}

		G_Obituary( self, attacker, mod );
	} else {      // wrong place, suicide, etc.
		self->enemy = NULL;
		if( GAME_IMPORT.is_dedicated_server ) {
			Com_Printf( "%s %s\n", self->r.client->netname, message );
		}

		G_PositionedSound( self->s.origin, CHAN_AUTO, "sounds/trombone/sad" );

		G_Obituary( self, attacker == self ? self : world, mod );
	}
}


//=======================================================
// DEAD BODIES
//=======================================================

static edict_t *CreateCorpse( edict_t *ent, edict_t *attacker, int damage ) {
	assert( ent->s.type == ET_PLAYER );

	int contents = G_PointContents( ent->s.origin );
	if( contents & CONTENTS_NODROP ) {
		return NULL;
	}

	edict_t * body = G_Spawn();

	body->classname = "body";
	body->s.type = ET_CORPSE;
	body->health = ent->health;
	body->mass = ent->mass;
	body->r.owner = ent->r.owner;
	body->s.team = ent->s.team;
	body->r.svflags = SVF_CORPSE | SVF_BROADCAST;
	body->activator = ent;
	if( g_deadbody_followkiller->integer ) {
		body->enemy = attacker;
	}

	//use flat yaw
	body->s.angles[YAW] = ent->s.angles[YAW];

	//copy player position and box size
	VectorCopy( ent->s.origin, body->s.origin );
	VectorCopy( ent->s.origin, body->olds.origin );
	VectorCopy( ent->r.mins, body->r.mins );
	VectorCopy( ent->r.maxs, body->r.maxs );
	VectorCopy( ent->r.absmin, body->r.absmin );
	VectorCopy( ent->r.absmax, body->r.absmax );
	VectorCopy( ent->r.size, body->r.size );
	VectorCopy( ent->velocity, body->velocity );
	body->r.maxs[2] = body->r.mins[2] + 8;

	body->r.solid = SOLID_NOT;
	body->takedamage = DAMAGE_NO;
	body->movetype = MOVETYPE_TOSS;
	body->think = G_FreeEdict; // body self destruction countdown

	body->s.teleported = true;
	body->s.ownerNum = ent->s.number;

	int mod = meansOfDeath;
	bool gib = mod == MOD_ELECTROBOLT || mod == MOD_TRIGGER_HURT || mod == MOD_TELEFRAG
		|| mod == MOD_EXPLOSIVE || mod == MOD_SPIKES ||
		( ( mod == MOD_ROCKET || mod == MOD_GRENADE ) && damage >= 20 );

	if( gib ) {
		ThrowSmallPileOfGibs( body, knockbackOfDeath, damage );

		// reset gib impulse
		VectorClear( body->velocity );

		body->nextThink = level.time + 3000 + random_float01( &svs.rng ) * 3000;
		body->deadflag = DEAD_DEAD;
	}

	edict_t * event = G_SpawnEvent( EV_DIE, random_u64( &svs.rng ), NULL );
	event->r.svflags |= SVF_BROADCAST;
	event->s.ownerNum = body->s.number;

	ent->s.ownerNum = body->s.number;

	// bit of a hack, if we're not in warmup, leave the body with no think. think self destructs
	// after a timeout, but if we leave, next bomb round will call G_ResetLevel() cleaning up
	if( GS_MatchState( &server_gs ) == MATCH_STATE_WARMUP ) {
		body->nextThink = level.time + 3500;
	}
	else {
		body->think = NULL;
	}

	GClip_LinkEntity( body );
	return body;
}

/*
* player_die
*/
void player_die( edict_t *ent, edict_t *inflictor, edict_t *attacker, int damage, const vec3_t point ) {
	snap_edict_t snap_backup = ent->snap;
	client_snapreset_t resp_snap_backup = ent->r.client->resp.snap;

	VectorClear( ent->avelocity );

	ent->s.angles[0] = 0;
	ent->s.angles[2] = 0;
	ent->s.sound = EMPTY_HASH;

	ent->r.solid = SOLID_NOT;

	// player death
	ClientObituary( ent, inflictor, attacker );

	// create a corpse
	CreateCorpse( ent, attacker, damage );
	ent->enemy = NULL;

	ent->s.angles[YAW] = ent->r.client->ps.viewangles[YAW] = LookAtKillerYAW( ent, inflictor, attacker );

	// go ghost (also resets snap)
	G_GhostClient( ent );

	ent->deathTimeStamp = level.time;

	VectorClear( ent->velocity );
	VectorClear( ent->avelocity );
	ent->snap = snap_backup;
	ent->r.client->resp.snap = resp_snap_backup;
	ent->r.client->resp.snap.buttons = 0;
	GClip_LinkEntity( ent );
}

/*
* G_Client_UpdateActivity
*/
void G_Client_UpdateActivity( gclient_t *client ) {
	if( !client ) {
		return;
	}
	client->level.last_activity = level.time;
}

/*
* G_Client_InactivityRemove
*/
void G_Client_InactivityRemove( gclient_t *client ) {
	if( !client ) {
		return;
	}

	if( trap_GetClientState( client - game.clients ) < CS_SPAWNED ) {
		return;
	}

	if( client->ps.pmove.pm_type != PM_NORMAL ) {
		return;
	}

	if( g_inactivity_maxtime->modified ) {
		if( g_inactivity_maxtime->value <= 0.0f ) {
			Cvar_ForceSet( "g_inactivity_maxtime", "0.0" );
		} else if( g_inactivity_maxtime->value < 15.0f ) {
			Cvar_ForceSet( "g_inactivity_maxtime", "15.0" );
		}

		g_inactivity_maxtime->modified = false;
	}

	if( g_inactivity_maxtime->value == 0.0f ) {
		return;
	}

	if( ( GS_MatchState( &server_gs ) != MATCH_STATE_PLAYTIME ) || !level.gametype.removeInactivePlayers ) {
		return;
	}

	// inactive for too long
	if( client->level.last_activity && client->level.last_activity + ( g_inactivity_maxtime->value * 1000 ) < level.time ) {
		if( client->team >= TEAM_PLAYERS && client->team < GS_MAX_TEAMS ) {
			edict_t *ent = &game.edicts[ client - game.clients + 1 ];

			// move to spectators and reset the queue time, effectively removing from the challengers queue
			G_Teams_SetTeam( ent, TEAM_SPECTATOR );
			client->queueTimeStamp = 0;

			G_PrintMsg( NULL, "%s has been moved to spectator after %.1f seconds of inactivity\n", client->netname, g_inactivity_maxtime->value );
		}
	}
}

/*
* G_ClientClearStats
*/
void G_ClientClearStats( edict_t *ent ) {
	if( !ent || !ent->r.client ) {
		return;
	}

	memset( &ent->r.client->level.stats, 0, sizeof( ent->r.client->level.stats ) );
}

/*
* G_GhostClient
*/
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
	ent->s.weapon = 0;
	ent->s.sound = EMPTY_HASH;
	ent->s.light = 0;
	ent->viewheight = 0;
	ent->takedamage = DAMAGE_NO;

	memset( ent->r.client->ps.weapons, 0, sizeof( ent->r.client->ps.weapons ) );
	memset( ent->r.client->ps.items, 0, sizeof( ent->r.client->ps.items ) );

	ent->r.client->ps.weapon = Weapon_None;
	ent->r.client->ps.pending_weapon = Weapon_None;
	ent->r.client->ps.weapon_state = WeaponState_Ready;
	ent->r.client->ps.weapon_time = 0;

	GClip_LinkEntity( ent );
}

/*
* G_ClientRespawn
*/
void G_ClientRespawn( edict_t *self, bool ghost ) {
	int i;
	edict_t *spawnpoint;
	vec3_t spawn_origin, spawn_angles;
	gclient_t *client;
	int old_team;

	G_SpawnQueue_RemoveClient( self );

	self->r.svflags &= ~SVF_NOCLIENT;

	//if invalid be spectator
	if( self->r.client->team < 0 || self->r.client->team >= GS_MAX_TEAMS ) {
		self->r.client->team = TEAM_SPECTATOR;
	}

	// force ghost always to true when in spectator team
	if( self->r.client->team == TEAM_SPECTATOR ) {
		ghost = true;
	}

	old_team = self->s.team;

	GClip_UnlinkEntity( self );

	client = self->r.client;

	memset( &client->resp, 0, sizeof( client->resp ) );
	memset( &client->ps, 0, sizeof( client->ps ) );
	client->resp.timeStamp = level.time;
	client->ps.playerNum = PLAYERNUM( self );

	// clear entity values
	memset( &self->snap, 0, sizeof( self->snap ) );
	memset( &self->s, 0, sizeof( self->s ) );
	memset( &self->olds, 0, sizeof( self->olds ) );

	self->s.number = self->olds.number = ENTNUM( self );

	// relink client struct
	self->r.client = &game.clients[PLAYERNUM( self )];

	// update team
	self->s.team = client->team;

	self->deadflag = DEAD_NO;
	self->groundentity = NULL;
	self->takedamage = DAMAGE_AIM;
	self->die = player_die;
	self->viewheight = playerbox_stand_viewheight;
	self->r.inuse = true;
	self->mass = PLAYER_MASS;
	self->r.clipmask = MASK_PLAYERSOLID;
	self->waterlevel = 0;
	self->watertype = 0;
	self->flags &= ~FL_NO_KNOCKBACK;
	self->r.svflags &= ~SVF_CORPSE;
	self->enemy = NULL;
	self->r.owner = NULL;
	self->max_health = 100;
	self->health = self->max_health;

	if( self->r.svflags & SVF_FAKECLIENT ) {
		self->classname = "fakeclient";
	} else {
		self->classname = "player";
	}

	VectorCopy( playerbox_stand_mins, self->r.mins );
	VectorCopy( playerbox_stand_maxs, self->r.maxs );
	VectorClear( self->velocity );
	VectorClear( self->avelocity );

	client->ps.POVnum = ENTNUM( self );

	// set movement info
	client->ps.pmove.max_speed = DEFAULT_PLAYERSPEED;
	client->ps.pmove.jump_speed = DEFAULT_JUMPSPEED;
	client->ps.pmove.dash_speed = DEFAULT_DASHSPEED;

	if( ghost ) {
		self->s.type = ET_GHOST;
		self->r.solid = SOLID_NOT;
		self->movetype = MOVETYPE_NOCLIP;
	} else {
		self->s.type = ET_PLAYER;
		self->r.solid = SOLID_YES;
		self->movetype = MOVETYPE_PLAYER;
		client->ps.pmove.features = PMFEAT_DEFAULT;
	}

	ClientUserinfoChanged( self, client->userinfo );

	if( old_team != self->s.team ) {
		G_Teams_UpdateMembersList();
	}

	SelectSpawnPoint( self, &spawnpoint, spawn_origin, spawn_angles );
	VectorCopy( spawn_origin, client->ps.pmove.origin );
	VectorCopy( spawn_origin, self->s.origin );

	// set angles
	self->s.angles[PITCH] = 0;
	self->s.angles[YAW] = AngleNormalize360( spawn_angles[YAW] );
	self->s.angles[ROLL] = 0;
	VectorCopy( self->s.angles, client->ps.viewangles );

	// set the delta angle
	for( i = 0; i < 3; i++ )
		client->ps.pmove.delta_angles[i] = ANGLE2SHORT( client->ps.viewangles[i] ) - client->ucmd.angles[i];

	// don't put spectators in the game
	if( !ghost ) {
		KillBox( self, MOD_TELEFRAG, vec3_origin );
	}

	self->s.teleported = true;

	// hold in place briefly
	client->ps.pmove.pm_flags = PMF_TIME_TELEPORT;
	client->ps.pmove.pm_time = 14;
	client->ps.pmove.no_control_time = CLIENT_RESPAWN_FREEZE_DELAY;

	G_UseTargets( spawnpoint, self );

	GClip_LinkEntity( self );

	// let the gametypes perform their changes
	GT_asCallPlayerRespawn( self, old_team, self->s.team );

	if( self->r.svflags & SVF_FAKECLIENT ) {
		AI_Respawn( self );
	}
}

/*
* G_PlayerCanTeleport
*
* Checks if the player can be teleported.
*/
bool G_PlayerCanTeleport( edict_t *player ) {
	if( !player->r.client ) {
		return false;
	}
	if( player->r.client->ps.pmove.pm_type > PM_SPECTATOR ) {
		return false;
	}
	return true;
}

/*
* G_TeleportPlayer
*/
void G_TeleportPlayer( edict_t *player, edict_t *dest ) {
	int i;
	vec3_t velocity;
	mat3_t axis;
	float speed;
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
	VectorCopy( client->old_pmove.velocity, velocity );

	velocity[2] = 0; // ignore vertical velocity
	speed = VectorLength( velocity );

	AnglesToAxis( dest->s.angles, axis );
	VectorScale( &axis[AXIS_FORWARD], speed, client->ps.pmove.velocity );

	VectorCopy( dest->s.angles, client->ps.viewangles );
	VectorCopy( dest->s.origin, client->ps.pmove.origin );

	// set the delta angle
	for( i = 0; i < 3; i++ )
		client->ps.pmove.delta_angles[i] = ANGLE2SHORT( client->ps.viewangles[i] ) - client->ucmd.angles[i];

	client->ps.pmove.pm_flags |= PMF_TIME_TELEPORT;
	client->ps.pmove.pm_time = 1; // force the minimum no control delay
	player->s.teleported = true;

	// update the entity from the pmove
	VectorCopy( client->ps.viewangles, player->s.angles );
	VectorCopy( client->ps.pmove.origin, player->s.origin );
	VectorCopy( client->ps.pmove.origin, player->olds.origin );
	VectorCopy( client->ps.pmove.velocity, player->velocity );

	// unlink to make sure it can't possibly interfere with KillBox
	GClip_UnlinkEntity( player );

	// kill anything at the destination
	KillBox( player, MOD_TELEFRAG, vec3_origin );

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

	memset( &client->ucmd, 0, sizeof( client->ucmd ) );
	memset( &client->level, 0, sizeof( client->level ) );
	client->level.timeStamp = level.time;
	G_Client_UpdateActivity( client ); // activity detected

	client->team = TEAM_SPECTATOR;
	if( g_teams_autojoin->integer ) {
		G_Teams_JoinAnyTeam( ent, true ); // auto respawn in a team
	} else {
		G_ClientRespawn( ent, true ); // respawn as ghost
	}
	ent->movetype = MOVETYPE_NOCLIP; // allow freefly

	G_PrintMsg( NULL, "%s entered the game\n", client->netname );

	client->connecting = false;

	// schedule the next scoreboard update
	client->level.scoreboard_time = svs.realtime + scoreboardInterval - ( svs.realtime % scoreboardInterval );

	G_ClientEndSnapFrame( ent ); // make sure all view stuff is valid

	// let the gametype scripts now this client just entered the level
	G_Gametype_ScoreEvent( client, "enterGame", NULL );
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

/*
* G_SanitizeUserString
*/
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

/*
* G_SetName
*/
static void G_SetName( edict_t *ent, const char *original_name ) {
	const char *invalid_prefixes[] = { "console", "[team]", "[spec]", "[bot]", NULL };
	edict_t *other;
	char name[MAX_NAME_CHARS + 1];
	int i, trynum, trylen;
	int c_ascii;

	if( !ent->r.client ) {
		return;
	}

	// we allow NULL to be passed for name
	if( !original_name ) {
		original_name = "";
	}

	Q_strncpyz( name, original_name, sizeof( name ) );

	c_ascii = G_SanitizeUserString( name, sizeof( name ) );
	if( !c_ascii ) {
		Q_strncpyz( name, "Player", sizeof( name ) );
	}

	if( !( ent->r.svflags & SVF_FAKECLIENT ) ) {
		for( i = 0; invalid_prefixes[i] != NULL; i++ ) {
			if( !Q_strnicmp( name, invalid_prefixes[i], strlen( invalid_prefixes[i] ) ) ) {
				Q_strncpyz( name, "Player", sizeof( name ) );
				break;
			}
		}
	}

	trynum = 1;
	do {
		for( i = 0; i < server_gs.maxclients; i++ ) {
			other = game.edicts + 1 + i;
			if( !other->r.inuse || !other->r.client || other == ent ) {
				continue;
			}

			// if nick is already in use, try with (number) appended
			if( !Q_stricmp( name, other->r.client->netname ) ) {
				if( trynum != 1 ) { // remove last try
					name[strlen( name ) - strlen( va( "(%i)", trynum - 1 ) )] = 0;
				}

				// make sure there is enough space for the postfix
				trylen = strlen( va( "(%i)", trynum ) );
				if( (int)strlen( name ) + trylen > MAX_NAME_CHARS ) {
					name[ MAX_NAME_CHARS - trylen - 1 ] = '\0';
				}

				// add the postfix
				Q_strncatz( name, va( "(%i)", trynum ), sizeof( name ) );

				// go trough all clients again
				trynum++;
				break;
			}
		}
	} while( i != server_gs.maxclients && trynum <= MAX_CLIENTS );

	Q_strncpyz( ent->r.client->netname, name, sizeof( ent->r.client->netname ) );
}

/*
* G_UpdatePlayerInfoString
*/
static void G_UpdatePlayerInfoString( int playerNum ) {
	char playerString[MAX_INFO_STRING];
	gclient_t *client;

	assert( playerNum >= 0 && playerNum < server_gs.maxclients );
	client = &game.clients[playerNum];

	// update client information in cgame
	playerString[0] = 0;

	Info_SetValueForKey( playerString, "name", client->netname );
	Info_SetValueForKey( playerString, "hand", va( "%i", client->hand ) );

	playerString[MAX_CONFIGSTRING_CHARS - 1] = 0;
	trap_ConfigString( CS_PLAYERINFOS + playerNum, playerString );
}

/*
* ClientUserinfoChanged
* called whenever the player updates a userinfo variable.
*
* The game can override any of the settings in place
* (forcing skins or names, etc) before copying it off.
*/
void ClientUserinfoChanged( edict_t *ent, char *userinfo ) {
	char *s;
	char oldname[MAX_INFO_VALUE];
	gclient_t *cl;

	int i;

	assert( ent && ent->r.client );
	assert( userinfo && Info_Validate( userinfo ) );

	// check for malformed or illegal info strings
	if( !Info_Validate( userinfo ) ) {
		trap_DropClient( ent, DROP_TYPE_GENERAL, "Error: Invalid userinfo" );
		return;
	}

	cl = ent->r.client;

	// ip
	s = Info_ValueForKey( userinfo, "ip" );
	if( !s ) {
		trap_DropClient( ent, DROP_TYPE_GENERAL, "Error: Server didn't provide client IP" );
		return;
	}

	Q_strncpyz( cl->ip, s, sizeof( cl->ip ) );

	// socket
	s = Info_ValueForKey( userinfo, "socket" );
	if( !s ) {
		trap_DropClient( ent, DROP_TYPE_GENERAL, "Error: Server didn't provide client socket" );
		return;
	}

	Q_strncpyz( cl->socket, s, sizeof( cl->socket ) );

	// set name, it's validated and possibly changed first
	Q_strncpyz( oldname, cl->netname, sizeof( oldname ) );
	G_SetName( ent, Info_ValueForKey( userinfo, "name" ) );
	if( oldname[0] && Q_stricmp( oldname, cl->netname ) && !CheckFlood( ent, false ) ) {
		G_PrintMsg( NULL, "%s is now known as %s\n", oldname, cl->netname );
	}
	if( !Info_SetValueForKey( userinfo, "name", cl->netname ) ) {
		trap_DropClient( ent, DROP_TYPE_GENERAL, "Error: Couldn't set userinfo (name)" );
		return;
	}

	// handedness
	s = Info_ValueForKey( userinfo, "hand" );
	if( !s ) {
		cl->hand = 2;
	} else {
		cl->hand = bound( atoi( s ), 0, 2 );
	}

	// handicap
	s = Info_ValueForKey( userinfo, "handicap" );
	if( s ) {
		i = atoi( s );

		if( i > 90 || i < 0 ) {
			G_PrintMsg( ent, "Handicap must be defined in the [0-90] range.\n" );
			cl->handicap = 0;
		} else {
			cl->handicap = i;
		}
	}

	// save off the userinfo in case we want to check something later
	Q_strncpyz( cl->userinfo, userinfo, sizeof( cl->userinfo ) );

	G_UpdatePlayerInfoString( PLAYERNUM( ent ) );

	G_Gametype_ScoreEvent( cl, "userinfochanged", oldname );
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
bool ClientConnect( edict_t *ent, char *userinfo, bool fakeClient ) {
	char *value;

	assert( ent );
	assert( userinfo && Info_Validate( userinfo ) );
	assert( Info_ValueForKey( userinfo, "ip" ) && Info_ValueForKey( userinfo, "socket" ) );

	// verify that server gave us valid data
	if( !Info_Validate( userinfo ) ) {
		Info_SetValueForKey( userinfo, "rejtype", va( "%i", DROP_TYPE_GENERAL ) );
		Info_SetValueForKey( userinfo, "rejflag", va( "%i", 0 ) );
		Info_SetValueForKey( userinfo, "rejmsg", "Invalid userinfo" );
		return false;
	}

	if( !Info_ValueForKey( userinfo, "ip" ) ) {
		Info_SetValueForKey( userinfo, "rejtype", va( "%i", DROP_TYPE_GENERAL ) );
		Info_SetValueForKey( userinfo, "rejflag", va( "%i", 0 ) );
		Info_SetValueForKey( userinfo, "rejmsg", "Error: Server didn't provide client IP" );
		return false;
	}

	if( !Info_ValueForKey( userinfo, "ip" ) ) {
		Info_SetValueForKey( userinfo, "rejtype", va( "%i", DROP_TYPE_GENERAL ) );
		Info_SetValueForKey( userinfo, "rejflag", va( "%i", 0 ) );
		Info_SetValueForKey( userinfo, "rejmsg", "Error: Server didn't provide client socket" );
		return false;
	}

	// check to see if they are on the banned IP list
	value = Info_ValueForKey( userinfo, "ip" );
	if( SV_FilterPacket( value ) ) {
		Info_SetValueForKey( userinfo, "rejtype", va( "%i", DROP_TYPE_GENERAL ) );
		Info_SetValueForKey( userinfo, "rejflag", va( "%i", 0 ) );
		Info_SetValueForKey( userinfo, "rejmsg", "You're banned from this server" );
		return false;
	}

	// check for a password
	value = Info_ValueForKey( userinfo, "password" );
	if( !fakeClient && ( *password->string && ( !value || strcmp( password->string, value ) ) ) ) {
		Info_SetValueForKey( userinfo, "rejtype", va( "%i", DROP_TYPE_PASSWORD ) );
		Info_SetValueForKey( userinfo, "rejflag", va( "%i", 0 ) );
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
	ent->r.svflags = ( SVF_NOCLIENT | ( fakeClient ? SVF_FAKECLIENT : 0 ) );
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

		Com_Printf( "%s connected from %s\n", ent->r.client->netname, ent->r.client->ip );
	}

	// let the gametype scripts know this client just connected
	G_Gametype_ScoreEvent( ent->r.client, "connect", NULL );

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

	// send effect
	if( ent->s.team > TEAM_SPECTATOR ) {
		G_TeleportEffect( ent, false );
	}

	ent->r.client->team = TEAM_SPECTATOR;
	G_ClientRespawn( ent, true ); // respawn as ghost
	ent->movetype = MOVETYPE_NOCLIP; // allow freefly

	// let the gametype scripts know this client just disconnected
	G_Gametype_ScoreEvent( ent->r.client, "disconnect", NULL );

	ent->r.inuse = false;
	ent->r.svflags = SVF_NOCLIENT;

	memset( ent->r.client, 0, sizeof( *ent->r.client ) );
	ent->r.client->ps.playerNum = PLAYERNUM( ent );

	trap_ConfigString( CS_PLAYERINFOS + PLAYERNUM( ent ), "" );
	GClip_UnlinkEntity( ent );

	G_Match_CheckReadys();
}

//==============================================================

/*
* G_PredictedEvent
*/
void G_PredictedEvent( int entNum, int ev, u64 parm ) {
	edict_t *ent = &game.edicts[entNum];
	switch( ev ) {
		case EV_SMOOTHREFIREWEAPON: // update the firing
			G_FireWeapon( ent, parm );
			break; // don't send the event

		case EV_WEAPONACTIVATE:
			ent->s.weapon = parm;
			G_AddEvent( ent, ev, parm, true );
			break;

		default:
			G_AddEvent( ent, ev, parm, true );
			break;
	}
}

void G_PredictedFireWeapon( int entNum, WeaponType weapon ) {
	edict_t * ent = &game.edicts[ entNum ];
	G_FireWeapon( ent, weapon );

	vec3_t start;
	VectorCopy( ent->s.origin, start );
	start[ 2 ] += ent->r.client->ps.viewheight;

	edict_t * event = G_SpawnEvent( EV_FIREWEAPON, weapon, start );
	event->s.ownerNum = entNum;
	VectorCopy( ent->r.client->ps.viewangles, event->s.origin2 ); // DirToByte is too inaccurate
	event->s.team = ent->s.team;
}

/*
* ClientMakePlrkeys
*/
static void ClientMakePlrkeys( gclient_t *client, usercmd_t *ucmd ) {
	client_snapreset_t *clsnap;

	if( !client ) {
		return;
	}

	clsnap = &client->resp.snap;
	clsnap->plrkeys = 0; // clear it first

	if( ucmd->forwardmove > 0 ) {
		clsnap->plrkeys |= ( 1 << KEYICON_FORWARD );
	}
	if( ucmd->forwardmove < 0 ) {
		clsnap->plrkeys |= ( 1 << KEYICON_BACKWARD );
	}
	if( ucmd->sidemove > 0 ) {
		clsnap->plrkeys |= ( 1 << KEYICON_RIGHT );
	}
	if( ucmd->sidemove < 0 ) {
		clsnap->plrkeys |= ( 1 << KEYICON_LEFT );
	}
	if( ucmd->upmove > 0 ) {
		clsnap->plrkeys |= ( 1 << KEYICON_JUMP );
	}
	if( ucmd->upmove < 0 ) {
		clsnap->plrkeys |= ( 1 << KEYICON_CROUCH );
	}
	if( ucmd->buttons & BUTTON_ATTACK ) {
		clsnap->plrkeys |= ( 1 << KEYICON_FIRE );
	}
	if( ucmd->buttons & BUTTON_SPECIAL ) {
		clsnap->plrkeys |= ( 1 << KEYICON_SPECIAL );
	}
}

/*
* ClientMultiviewChanged
* This will be called when client tries to change multiview mode
* Mode change can be disallowed by returning false
*/
bool ClientMultiviewChanged( edict_t *ent, bool multiview ) {
	ent->r.client->multiview = multiview == true;

	return true;
}

/*
* ClientThink
*/
void ClientThink( edict_t *ent, usercmd_t *ucmd, int timeDelta ) {
	gclient_t *client;
	int i, j;
	static pmove_t pm;
	int delta, count;

	client = ent->r.client;

	client->ps.POVnum = ENTNUM( ent );
	client->ps.playerNum = PLAYERNUM( ent );

	// anti-lag
	if( ent->r.svflags & SVF_FAKECLIENT ) {
		client->timeDelta = 0;
	} else {
		int fixedNudge = ( game.snapFrameTime ) * 0.5; // fixme: find where this nudge comes from.

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
	if( ucmd->forwardmove != 0 || ucmd->sidemove != 0 || ucmd->upmove != 0 ||
		client->ucmd.angles[PITCH] != ucmd->angles[PITCH] || client->ucmd.angles[YAW] != ucmd->angles[YAW] ) {
		G_Client_UpdateActivity( client );
	}

	client->ucmd = *ucmd;

	// (is this really needed?:only if not cared enough about ps in the rest of the code)
	// refresh player state position from the entity
	VectorCopy( ent->s.origin, client->ps.pmove.origin );
	VectorCopy( ent->velocity, client->ps.pmove.velocity );
	VectorCopy( ent->s.angles, client->ps.viewangles );

	client->ps.pmove.gravity = level.gravity;

	if( GS_MatchState( &server_gs ) >= MATCH_STATE_POSTMATCH || GS_MatchPaused( &server_gs )
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

	// perform a pmove
	Pmove( &server_gs, &pm );

	// save results of pmove
	client->old_pmove = client->ps.pmove;

	// update the entity with the new position
	VectorCopy( client->ps.pmove.origin, ent->s.origin );
	VectorCopy( client->ps.pmove.velocity, ent->velocity );
	VectorCopy( client->ps.viewangles, ent->s.angles );
	ent->viewheight = client->ps.viewheight;
	VectorCopy( pm.mins, ent->r.mins );
	VectorCopy( pm.maxs, ent->r.maxs );

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

		if( ent->s.origin[ 2 ] <= -1024 ) {
			G_Damage( ent, world, world, vec3_origin, vec3_origin, ent->s.origin, 1337, 0, 0, MOD_VOID );
		}
	}

	ent->s.weapon = GS_ThinkPlayerWeapon( &server_gs, &client->ps, ucmd, client->timeDelta );

	if( G_IsDead( ent ) ) {
		if( ent->deathTimeStamp + g_respawn_delay_min->integer <= level.time ) {
			client->resp.snap.buttons |= ucmd->buttons;
		}
	} else if( client->ps.pmove.no_control_time <= 0 ) {
		client->resp.snap.buttons |= ucmd->buttons;
	}

	// generating plrkeys (optimized for net communication)
	ClientMakePlrkeys( client, ucmd );
}

/*
* G_ClientThink
* Client frame think, and call to execute its usercommands thinking
*/
void G_ClientThink( edict_t *ent ) {
	if( !ent || !ent->r.client ) {
		return;
	}

	if( trap_GetClientState( PLAYERNUM( ent ) ) < CS_SPAWNED ) {
		return;
	}

	ent->r.client->ps.POVnum = ENTNUM( ent ); // set self

	// run bots thinking with the rest of clients
	if( ent->r.svflags & SVF_FAKECLIENT ) {
		AI_Think( ent );
	}

	trap_ExecuteClientThinks( PLAYERNUM( ent ) );
}

/*
* G_CheckClientRespawnClick
*/
void G_CheckClientRespawnClick( edict_t *ent ) {
	if( !ent->r.inuse || !ent->r.client || !G_IsDead( ent ) ) {
		return;
	}

	if( GS_MatchState( &server_gs ) >= MATCH_STATE_POSTMATCH ) {
		return;
	}

	if( trap_GetClientState( PLAYERNUM( ent ) ) >= CS_SPAWNED ) {
		// if the spawnsystem doesn't require to click
		if( G_SpawnQueue_GetSystem( ent->s.team ) != SPAWNSYSTEM_INSTANT ) {
			if( level.time >= ent->deathTimeStamp + 3000 ) {
				G_SpawnQueue_AddClient( ent );
			}
		}
		// clicked
		else if( ent->r.client->resp.snap.buttons & BUTTON_ATTACK ) {
			if( level.time > ent->deathTimeStamp + g_respawn_delay_min->integer ) {
				G_SpawnQueue_AddClient( ent );
			}
		}
		// didn't click, but too much time passed
		else if( g_respawn_delay_max->integer && level.time > ent->deathTimeStamp + g_respawn_delay_max->integer ) {
			G_SpawnQueue_AddClient( ent );
		}
	}
}

#undef PLAYER_MASS
