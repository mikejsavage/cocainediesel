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
#include "qcommon/hashtable.h"

static u64 entity_id_seq;
static Hashtable< MAX_EDICTS * 2 > entity_id_hashtable;

EntityID NewEntity() {
	return { entity_id_seq++ };
}

void ResetEntityIDSequence() {
	entity_id_seq = 1;
	entity_id_hashtable.clear();
}

edict_t * GetEntity( EntityID id ) {
	u64 idx;
	return entity_id_hashtable.get( id.id, &idx ) ? &game.edicts[ idx ] : NULL;
}

edict_t * G_Find( edict_t * cursor, StringHash edict_t::* field, StringHash value ) {
	const edict_t * end = game.edicts + game.numentities;

	for( cursor = cursor == NULL ? world : cursor + 1; ( cursor < end ); cursor++ ) {
		if( cursor->r.inuse && cursor->*field == value ) {
			return cursor;
		}
	}

	return NULL;
}

edict_t * G_PickRandomEnt( StringHash edict_t::* field, StringHash value ) {
	size_t num_ents = 0;
	edict_t * cursor = NULL;

	while( ( cursor = G_Find( cursor, field, value ) ) != NULL ) {
		num_ents++;
	}

	if( num_ents == 0 ) { //no ents with this field and this value
		return NULL;
	}

	const size_t index = RandomUniform( &svs.rng, 0, num_ents );
	cursor = NULL;

	for( size_t i = 0; i <= index; i++ ) {
		cursor = G_Find( cursor, field, value );
	}

	return cursor;
}

edict_t * G_PickTarget( StringHash name ) {
	if( name == EMPTY_HASH ) {
		return NULL;
	}

	edict_t * cursor = NULL;

	edict_t * candidates[ MAX_EDICTS ];
	size_t num_candidates = 0;

	while( ( cursor = G_Find( cursor, &edict_t::name, name ) ) != NULL ) {
		candidates[ num_candidates ] = cursor;
		num_candidates++;
	}

	if( !num_candidates ) {
		Com_Printf( "G_PickTarget: target not found\n" );
		return NULL;
	}

	return candidates[ RandomUniform( &svs.rng, 0, num_candidates ) ];
}

static void Think_Delay( edict_t * ent ) {
	G_UseTargets( ent, ent->activator );
	G_FreeEdict( ent );
}

/*
* G_UseTargets
*
* the global "activator" should be set to the entity that initiated the firing.
*
* If self.delay is set, a DelayedUse entity will be created that will actually
* do the SUB_UseTargets after that many seconds have passed.
*
* Centerprints any self.message to the activator.
*
* Search for (string)targetname in all entities that
* match (string)self.target and call their .use function
*
*/
void G_UseTargets( edict_t * ent, edict_t * activator ) {
	//
	// check for a delay
	//
	if( ent->delay ) {
		// create a temp object to fire at a later time
		edict_t * t = G_Spawn();
		t->classname = "delayed_use";
		t->nextThink = level.time + ent->delay;
		t->think = Think_Delay;
		t->activator = activator;
		if( !activator ) {
			Com_Printf( "Think_Delay with no activator\n" );
		}
		t->message = ent->message;
		t->target = ent->target;
		t->killtarget = ent->killtarget;
		return;
	}

	//
	// print the message
	//
	if( ent->message ) {
		G_CenterPrintMsg( activator, "%s", ent->message );

		if( ent->sound != EMPTY_HASH ) {
			G_Sound( activator, ent->sound );
		}
	}

	//
	// kill killtargets
	//
	if( ent->killtarget != EMPTY_HASH ) {
		edict_t * t = NULL;
		while( ( t = G_Find( t, &edict_t::name, ent->killtarget ) ) ) {
			G_FreeEdict( t );
			if( !ent->r.inuse ) {
				Com_Printf( "entity was removed while using killtargets\n" );
				return;
			}
		}
	}

	//	Com_Printf ("TARGET: activating %s\n", ent->target);

	//
	// fire targets
	//
	if( ent->target != EMPTY_HASH ) {
		edict_t * t = NULL;
		while( ( t = G_Find( t, &edict_t::name, ent->target ) ) ) {
			if( t == ent ) {
				Com_Printf( "WARNING: Entity used itself.\n" );
			} else {
				G_CallUse( t, ent, activator );
			}
			if( !ent->r.inuse ) {
				Com_Printf( "entity was removed while using targets\n" );
				return;
			}
		}
	}
}

void G_SetMovedir( EulerDegrees3 * angles, Vec3 * movedir ) {
	AngleVectors( *angles, movedir, NULL, NULL );
	*angles = EulerDegrees3( 0.0f, 0.0f, 0.0f );
}

void G_FreeEdict( edict_t * ed ) {
	if( ed == NULL || !ed->r.inuse )
		return;

	GClip_UnlinkEntity( ed );

	// bool ok = entity_id_hashtable.remove( ed->id.id );
	// Assert( ok );

	memset( ed, 0, sizeof( *ed ) );
	ed->s.number = ENTNUM( ed );
	ed->s.svflags = SVF_NOCLIENT;

	if( !ISEVENTENTITY( &ed->s ) && level.spawnedTimeStamp != svs.realtime ) {
		ed->freetime = svs.realtime; // ET_EVENT or ET_SOUND don't need to wait to be reused
	}
}

void G_InitEdict( edict_t * e ) {
	// if( e->r.inuse ) {
	// 	bool ok = entity_id_hashtable.remove( e->id.id );
	// 	Assert( ok );
	// }

	memset( e, 0, sizeof( *e ) );
	e->s.number = ENTNUM( e );
	e->s.id = NewEntity();
	e->r.inuse = true;

	// bool ok = entity_id_hashtable.add( e->id.id, e->s.number );
	// Assert( ok );

	e->s.scale = Vec3( 1.0f );
	e->gravity_scale = 1.0f;
	e->restitution = 1.0f;

	// mark all entities to not be sent by default
	e->s.svflags = SVF_NOCLIENT;
}

/*
* G_Spawn
*
* Either finds a free edict, or allocates a new one.
* Try to avoid reusing an entity that was recently freed, because it
* can cause the client to think the entity morphed into something else
* instead of being removed and recreated, which can cause interpolated
* angles and bad trails.
*/
edict_t * G_Spawn() {
	if( !level.canSpawnEntities ) {
		Com_Printf( "WARNING: Spawning entity before map entities have been spawned\n" );
	}

	size_t i;
	edict_t * freed = NULL;
	for( i = server_gs.maxclients + 1; i < game.numentities; i++ ) {
		edict_t * e = &game.edicts[ i ];
		if( e->r.inuse ) {
			continue;
		}

		// the first couple seconds of server time can involve a lot of
		// freeing and allocating, so relax the replacement policy
		if( e->freetime < level.spawnedTimeStamp + 2000 || svs.realtime > e->freetime + 500 ) {
			G_InitEdict( e );
			return e;
		}

		// this is going to be our second chance to spawn an entity in case all free
		// entities have been freed only recently
		if( freed == NULL ) {
			freed = e;
		}
	}

	if( i == ARRAY_COUNT( game.edicts ) ) {
		if( freed != NULL ) {
			G_InitEdict( freed );
			return freed;
		}
		Fatal( "G_Spawn: no free edicts" );
	}

	edict_t * e = &game.edicts[ game.numentities ];
	game.numentities++;

	SV_LocateEntities( game.edicts, game.numentities );

	G_InitEdict( e );

	return e;
}

void G_AddEvent( edict_t * ent, int event, u64 parm, bool highPriority ) {
	if( !ent || ent == world || !ent->r.inuse || !event ) {
		return;
	}

	int eventNum = ent->numEvents & 1;
	if( ent->eventPriority[eventNum] && !ent->eventPriority[( eventNum + 1 ) & 1] ) {
		eventNum = ( eventNum + 1 ) & 1; // prefer overwriting low priority events
	} else if( !highPriority && ent->eventPriority[eventNum] ) {
		return; // no low priority event to overwrite
	} else {
		ent->numEvents++; // numEvents is only used to vary the overwritten event
	}
	ent->s.events[eventNum].type = event;
	ent->s.events[eventNum].parm = parm;
	ent->eventPriority[eventNum] = highPriority;
}

edict_t * G_SpawnEvent( int event, u64 parm, const Vec3 * origin ) {
	edict_t * ent = G_Spawn();
	ent->s.type = ET_EVENT;
	ent->s.solidity = Solid_NotSolid;
	ent->s.svflags &= ~SVF_NOCLIENT;
	if( origin ) {
		ent->s.origin = *origin;
	}
	G_AddEvent( ent, event, parm, true );

	GClip_LinkEntity( ent );

	return ent;
}

void G_InitMover( edict_t * ent ) {
	// ent->r.solid = SOLID_YES;
	ent->movetype = MOVETYPE_PUSH;
	ent->s.svflags &= ~SVF_NOCLIENT;
}

void G_CallThink( edict_t * ent ) {
	if( ent->think ) {
		ent->think( ent );
	}
}

void G_CallTouch( edict_t * self, edict_t * other, Vec3 normal, SolidBits solid_mask ) {
	if( self == other ) {
		return;
	}

	if( self->touch ) {
		self->touch( self, other, normal, solid_mask );
	}
}

void G_CallUse( edict_t * self, edict_t * other, edict_t * activator ) {
	if( self->use ) {
		self->use( self, other, activator );
	}
}

void G_CallStop( edict_t * self ) {
	if( self->stop ) {
		self->stop( self );
	}
}

void G_CallPain( edict_t * ent, edict_t * attacker, float kick, float damage ) {
	if( ent->pain ) {
		ent->pain( ent, attacker, kick, damage );
	}
}

void G_CallDie( edict_t * ent, edict_t * inflictor, edict_t * attacker, int assistorNo, DamageType damage_type, int damage ) {
	if( ent->die ) {
		ent->die( ent, inflictor, attacker, assistorNo, damage_type, damage );
	}
}

/*
* G_PrintMsg
*
* NULL sends to all the message to all clients
*/
void G_PrintMsg( edict_t * ent, const char * format, ... ) {
	char msg[MAX_STRING_CHARS];
	va_list argptr;

	va_start( argptr, format );
	vsnprintf( msg, sizeof( msg ), format, argptr );
	va_end( argptr );

	// double quotes are bad
	char * p = msg;
	while( ( p = strchr( p, '\"' ) ) != NULL ) {
		*p = '\'';
	}

	char cmd[MAX_STRING_CHARS];
	snprintf( cmd, sizeof( cmd ), "pr \"%s\"", msg );

	if( !ent ) {
		// mirror at server console
		if( is_dedicated_server ) {
			Com_Printf( "%s", msg );
		}
		PF_GameCmd( NULL, cmd );
	}
	else {
		if( ent->r.inuse && ent->r.client ) {
			PF_GameCmd( ent, cmd );
		}
	}
}

/*
* G_ChatMsg
*
* NULL sends the message to all clients
*/
void G_ChatMsg( edict_t * ent, const edict_t * who, bool teamonly, Span< const char > msg_bad_quotes ) {
	TempAllocator temp = svs.frame_arena.temp();

	// double quotes are bad
	Span< char > msg = CloneSpan( &temp, msg_bad_quotes );
	for( char & c : msg ) {
		if( c == '\"' ) {
			c = '\'';
		}
	}

	char cmd[ MAX_STRING_CHARS ];
	ggformat( cmd, sizeof( cmd ), "{} {} \"{}\"", who && teamonly ? "tch" : "ch", who ? ENTNUM( who ) : 0, msg );

	if( !ent ) {
		// mirror at server console
		if( is_dedicated_server ) {
			if( !who ) {
				Com_GGPrint( "Console: {}", msg );     // admin console
			} else if( !who->r.client ) {
				;   // wtf?
			} else if( teamonly ) {
				const char * channel = who->r.client->ps.team == Team_None ? "SPEC" : "TEAM";
				Com_GGPrint( "[{}] {} {}", channel, who->r.client->name, msg );
			} else {
				Com_GGPrint( "{}: {}", who->r.client->name, msg );
			}
		}

		if( who && teamonly ) {
			for( int i = 0; i < server_gs.maxclients; i++ ) {
				ent = game.edicts + 1 + i;

				if( ent->r.inuse && ent->r.client && PF_GetClientState( i ) >= CS_CONNECTED ) {
					if( ent->s.team == who->s.team ) {
						PF_GameCmd( ent, cmd );
					}
				}
			}
		} else {
			PF_GameCmd( NULL, cmd );
		}
	} else {
		if( ent->r.inuse && ent->r.client && PF_GetClientState( PLAYERNUM( ent ) ) >= CS_CONNECTED ) {
			if( !who || !teamonly || ent->s.team == who->s.team ) {
				PF_GameCmd( ent, cmd );
			}
		}
	}
}

/*
* G_CenterPrintMsg
*
* NULL sends to all the message to all clients
*/
void G_CenterPrintMsg( edict_t * ent, const char * format, ... ) {
	char msg[1024];
	char cmd[MAX_STRING_CHARS];
	va_list argptr;

	va_start( argptr, format );
	vsnprintf( msg, sizeof( msg ), format, argptr );
	va_end( argptr );

	// double quotes are bad
	char * p = msg;
	while( ( p = strchr( p, '\"' ) ) != NULL )
		*p = '\'';

	snprintf( cmd, sizeof( cmd ), "cp \"%s\"", msg );
	PF_GameCmd( ent, cmd );

	if( ent != NULL ) {
		// add it to every player who's chasing this player
		for( edict_t * other = game.edicts + 1; PLAYERNUM( other ) < server_gs.maxclients; other++ ) {
			if( !other->r.client || !other->r.inuse || !other->r.client->resp.chase.active ) {
				continue;
			}

			if( other->r.client->resp.chase.target == ENTNUM( ent ) ) {
				PF_GameCmd( other, cmd );
			}
		}
	}
}

void G_ClearCenterPrint( edict_t * ent ) {
	G_CenterPrintMsg( ent, "%s", "" );
}

void G_DebugPrint( const char * format, ... ) {
	char msg[128];
	va_list argptr;

	va_start( argptr, format );
	vsnprintf( msg, sizeof( msg ), format, argptr );
	va_end( argptr );

	// double quotes are bad
	char * p = msg;
	while( ( p = strchr( p, '\"' ) ) != NULL )
		*p = '\'';

	char cmd[MAX_STRING_CHARS];
	snprintf( cmd, sizeof( cmd ), "debug \"%s\"", msg );
	PF_GameCmd( NULL, cmd );

	Com_Printf( "Debug: %s\n", msg );
}

//==================================================
// SOUNDS
//==================================================

static edict_t * _G_SpawnSound( StringHash sound ) {
	edict_t * ent = G_Spawn();
	ent->s.svflags &= ~SVF_NOCLIENT;
	ent->s.svflags |= SVF_SOUNDCULL;
	ent->s.type = ET_SOUNDEVENT;
	ent->s.sound = sound;

	return ent;
}

edict_t * G_Sound( edict_t * owner, StringHash sound ) {
	if( sound == EMPTY_HASH ) {
		return NULL;
	}

	if( ISEVENTENTITY( &owner->s ) ) {
		return NULL; // event entities can't be owner of sound entities
	}

	edict_t * ent = _G_SpawnSound( sound );
	ent->s.ownerNum = owner->s.number;
	MinMax3 bounds = EntityBounds( ServerCollisionModelStorage(), &owner->s );
	ent->s.origin = owner->s.origin + Center( bounds );

	GClip_LinkEntity( ent );
	return ent;
}

edict_t * G_PositionedSound( Vec3 origin, StringHash sound ) {
	if( sound == EMPTY_HASH ) {
		return NULL;
	}

	edict_t * ent = _G_SpawnSound( sound );
	ent->s.positioned_sound = true;
	ent->s.origin = origin;

	GClip_LinkEntity( ent );
	return ent;
}

void G_GlobalSound( StringHash sound ) {
	if( sound == EMPTY_HASH )
		return;

	edict_t * ent = _G_SpawnSound( sound );
	ent->s.svflags |= SVF_BROADCAST;

	GClip_LinkEntity( ent );
}

void G_LocalSound( edict_t * owner, StringHash sound ) {
	if( sound == EMPTY_HASH )
		return;

	if( ISEVENTENTITY( &owner->s ) ) {
		return; // event entities can't be owner of sound entities
	}

	edict_t * ent = _G_SpawnSound( sound );
	ent->s.ownerNum = ENTNUM( owner );
	ent->s.svflags |= EntityFlags( SVF_ONLYOWNER | SVF_BROADCAST );

	GClip_LinkEntity( ent );
}

//==============================================================================
//
//Kill box
//
//==============================================================================

/*
* KillBox
*
* Kills all entities that would touch the proposed new positioning
* of ent.  Ent should be unlinked before calling this!
*/
void KillBox( edict_t * ent, DamageType damage_type, Vec3 knockback ) {
	while( true ) {
		MinMax3 bounds = EntityBounds( ServerCollisionModelStorage(), &ent->s );
		trace_t trace = G_Trace( ent->s.origin, bounds, ent->s.origin, world, SolidMask_AnySolid );
		if( trace.HitNothing() ) {
			break;
		}

		if( trace.ent == ENTNUM( world ) ) {
			break; // found the world (but a player could be in there too). suicide?
		}

		// nail it
		G_Damage( &game.edicts[trace.ent], ent, ent, knockback, Vec3( 0.0f ), ent->s.origin, 200, Length( knockback ), 0, damage_type );

		// if we didn't kill it, fail
		if( EntitySolidity( ServerCollisionModelStorage(), &game.edicts[ trace.ent ].s ) != Solid_NotSolid ) {
			break;
		}
	}
}

float LookAtKillerYAW( edict_t * self, edict_t * inflictor, edict_t * attacker ) {
	Vec3 dir;

	if( attacker && attacker != world && attacker != self ) {
		dir = attacker->s.origin - self->s.origin;
	} else if( inflictor && inflictor != world && inflictor != self ) {
		dir = inflictor->s.origin - self->s.origin;
	} else {
		return self->s.angles.yaw;
	}

	return VecToAngles( dir ).yaw;
}

//==============================================================================
//
//		Warsow: more miscelanea tools
//
//==============================================================================

static void G_SpawnTeleportEffect( edict_t * ent, bool respawn, bool in ) {
	constexpr StringHash tele_in = "sounds/world/tele_in";
	constexpr StringHash tele_out = "sounds/world/tele_in";

	if( !ent || !ent->r.client ) {
		return;
	}

	if( PF_GetClientState( PLAYERNUM( ent ) ) < CS_SPAWNED || EntitySolidity( ServerCollisionModelStorage(), &ent->s ) == Solid_NotSolid ) {
		return;
	}

	edict_t * event = G_SpawnEvent( EV_SOUND_ORIGIN, in ? tele_in.hash : tele_out.hash, &ent->s.origin );
	event->s.ownerNum = ENTNUM( ent );
}

void G_TeleportEffect( edict_t * ent, bool in ) {
	G_SpawnTeleportEffect( ent, false, in );
}

void G_RespawnEffect( edict_t * ent ) {
	G_SpawnTeleportEffect( ent, true, false );
}

void G_CheckGround( edict_t * ent ) {
	float up_speed_limit = ent->r.client == NULL ? 1.0f : 180.0f;

	Vec3 ground_point = ent->s.origin - Vec3( 0.0f, 0.0f, 0.25f );
	MinMax3 bounds = EntityBounds( ServerCollisionModelStorage(), &ent->s );
	trace_t trace = G_Trace( ent->s.origin, bounds, ground_point, ent, EntitySolidity( ServerCollisionModelStorage(), &ent->s ) );

	if( ent->velocity.z > up_speed_limit || !ISWALKABLEPLANE( trace.normal ) ) {
		ent->groundentity = NULL;
		return;
	}

	ent->groundentity = &game.edicts[trace.ent];
	if( ent->velocity.z < 0.0f ) {
		ent->velocity.z = 0.0f;
	}
}

void G_ReleaseClientPSEvent( gclient_t * client ) {
	for( int i = 0; i < 2; i++ ) {
		if( client->resp.eventsCurrent < client->resp.eventsHead ) {
			client->ps.events[ i ] = client->resp.events[ client->resp.eventsCurrent % ARRAY_COUNT( client->resp.events ) ];
			client->resp.eventsCurrent++;
		} else {
			client->ps.events[ i ] = { };
		}
	}
}

/*
* G_AddPlayerStateEvent
* This event is only sent to this client inside its SyncPlayerState.
*/
void G_AddPlayerStateEvent( gclient_t * client, int ev, u64 parm ) {
	Assert( ev >= 0 && ev < PSEV_MAX_EVENTS );
	if( client == NULL )
		return;

	SyncEvent * event = &client->resp.events[ client->resp.eventsHead % ARRAY_COUNT( client->resp.events ) ];
	client->resp.eventsHead++;
	event->type = ev;
	event->parm = parm;
}

void G_ClearPlayerStateEvents( gclient_t *client ) {
	if( client ) {
		memset( client->resp.events, PSEV_NONE, sizeof( client->resp.events ) );
		client->resp.eventsCurrent = client->resp.eventsHead = 0;
	}
}

/*
* G_PlayerForText
* Returns player matching given text. It can be either number of the player or player's name.
*/
edict_t * G_PlayerForText( Span< const char > text ) {
	u64 num;
	if( TrySpanToU64( text, &num ) && num < u64( server_gs.maxclients ) && game.edicts[ num + 1 ].r.inuse ) {
		return &game.edicts[ num + 1 ];
	}

	for( int i = 0; i < server_gs.maxclients; i++ ) {
		edict_t * e = &game.edicts[ i + 1 ];
		if( StrCaseEqual( e->r.client->name, text ) ) {
			return e;
		}
	}

	return NULL;
}

void G_AnnouncerSound( edict_t * targ, StringHash sound, Team team, bool queued, edict_t * ignore ) {
	int psev = queued ? PSEV_ANNOUNCER_QUEUED : PSEV_ANNOUNCER;
	Team playerTeam;

	if( targ ) { // only for a given player
		if( !targ->r.client || PF_GetClientState( PLAYERNUM( targ ) ) < CS_SPAWNED ) {
			return;
		}

		if( targ == ignore ) {
			return;
		}

		G_AddPlayerStateEvent( targ->r.client, psev, sound.hash );
	} else {   // add it to all players
		for( edict_t * ent = game.edicts + 1; PLAYERNUM( ent ) < server_gs.maxclients; ent++ ) {
			if( !ent->r.inuse || PF_GetClientState( PLAYERNUM( ent ) ) < CS_SPAWNED ) {
				continue;
			}

			if( ent == ignore ) {
				continue;
			}

			// team filter
			if( team >= Team_None && team < Team_Count ) {
				playerTeam = ent->s.team;

				// if in chasecam, assume the player is in the chased player team
				if( playerTeam == Team_None && ent->r.client->resp.chase.active
					&& ent->r.client->resp.chase.target > 0 ) {
					playerTeam = game.edicts[ent->r.client->resp.chase.target].s.team;
				}

				if( playerTeam != team ) {
					continue;
				}
			}

			G_AddPlayerStateEvent( ent->r.client, psev, sound.hash );
		}
	}
}

void G_SunCycle( u64 time ) {
	float yaw = 3.420f + 24.0f * RandomUniformFloat( &svs.rng, 0.0f, 15.0f ); // idk, some random angle that doesn't hit 90° etc
	server_gs.gameState.sun_angles_from = server_gs.gameState.sun_angles_to;
	server_gs.gameState.sun_angles_to = EulerDegrees3( 53.31f, yaw, 0.0f );
	server_gs.gameState.sun_moved_from = svs.gametime;
	server_gs.gameState.sun_moved_to = svs.gametime + time;
}
