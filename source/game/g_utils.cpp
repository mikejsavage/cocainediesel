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

static void Think_Delay( edict_t *ent ) {
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
void G_UseTargets( edict_t *ent, edict_t *activator ) {
	edict_t *t;

	//
	// check for a delay
	//
	if( ent->delay ) {
		// create a temp object to fire at a later time
		t = G_Spawn();
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
		t = NULL;
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
		t = NULL;
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

void G_SetMovedir( Vec3 * angles, Vec3 * movedir ) {
	AngleVectors( *angles, movedir, NULL, NULL );
	*angles = Vec3( 0.0f );
}

void G_FreeEdict( edict_t *ed ) {
	if( ed == NULL || !ed->r.inuse )
		return;

	GClip_UnlinkEntity( ed );

	bool ok = entity_id_hashtable.remove( ed->id.id );
	assert( ok );

	memset( ed, 0, sizeof( *ed ) );
	ed->s.number = ENTNUM( ed );
	ed->s.svflags = SVF_NOCLIENT;

	if( !ISEVENTENTITY( &ed->s ) && level.spawnedTimeStamp != svs.realtime ) {
		ed->freetime = svs.realtime; // ET_EVENT or ET_SOUND don't need to wait to be reused
	}
}

void G_InitEdict( edict_t *e ) {
	if( e->r.inuse ) {
		bool ok = entity_id_hashtable.remove( e->id.id );
		assert( ok );
	}

	memset( e, 0, sizeof( *e ) );
	e->s.number = ENTNUM( e );
	e->id = NewEntity();
	e->r.inuse = true;

	bool ok = entity_id_hashtable.add( e->id.id, e->s.number );
	assert( ok );

	e->s.scale = Vec3( 1.0f );

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
edict_t *G_Spawn() {
	if( !level.canSpawnEntities ) {
		Com_Printf( "WARNING: Spawning entity before map entities have been spawned\n" );
	}

	int i;
	edict_t * freed = NULL;
	edict_t * e = &game.edicts[server_gs.maxclients + 1];
	for( i = server_gs.maxclients + 1; i < game.numentities; i++, e++ ) {
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
		if( !freed ) {
			freed = e;
		}
	}

	if( i == game.maxentities ) {
		if( freed ) {
			G_InitEdict( freed );
			return freed;
		}
		Fatal( "G_Spawn: no free edicts" );
	}

	game.numentities++;

	SV_LocateEntities( game.edicts, game.numentities, game.maxentities );

	G_InitEdict( e );

	return e;
}

void G_AddEvent( edict_t *ent, int event, u64 parm, bool highPriority ) {
	if( !ent || ent == world || !ent->r.inuse ) {
		return;
	}
	if( !event ) {
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

edict_t *G_SpawnEvent( int event, u64 parm, const Vec3 * origin ) {
	edict_t * ent = G_Spawn();
	ent->s.type = ET_EVENT;
	ent->r.solid = SOLID_NOT;
	ent->s.svflags &= ~SVF_NOCLIENT;
	if( origin ) {
		ent->s.origin = *origin;
	}
	G_AddEvent( ent, event, parm, true );

	GClip_LinkEntity( ent );

	return ent;
}

void G_MorphEntityIntoEvent( edict_t *ent, int event, u64 parm ) {
	ent->s.type = ET_EVENT;
	ent->r.solid = SOLID_NOT;
	ent->s.svflags &= ~SVF_PROJECTILE; // FIXME: Medar: should be remove all or remove this one elsewhere?
	ent->s.linearMovement = false;
	G_AddEvent( ent, event, parm, true );

	GClip_LinkEntity( ent );
}

void G_InitMover( edict_t *ent ) {
	ent->r.solid = SOLID_YES;
	ent->movetype = MOVETYPE_PUSH;
	ent->s.svflags &= ~SVF_NOCLIENT;

	GClip_SetBrushModel( ent );
}

void G_CallThink( edict_t *ent ) {
	if( ent->think ) {
		ent->think( ent );
	}
}

void G_CallTouch( edict_t *self, edict_t *other, Plane *plane, int surfFlags ) {
	if( self == other ) {
		return;
	}

	if( self->touch ) {
		self->touch( self, other, plane, surfFlags );
	}
}

void G_CallUse( edict_t *self, edict_t *other, edict_t *activator ) {
	if( self->use ) {
		self->use( self, other, activator );
	}
}

void G_CallStop( edict_t *self ) {
	if( self->stop ) {
		self->stop( self );
	}
}

void G_CallPain( edict_t *ent, edict_t *attacker, float kick, float damage ) {
	if( ent->pain ) {
		ent->pain( ent, attacker, kick, damage );
	}
}

void G_CallDie( edict_t *ent, edict_t *inflictor, edict_t *attacker, int assistorNo, DamageType damage_type, int damage ) {
	if( ent->die ) {
		ent->die( ent, inflictor, attacker, assistorNo, damage_type, damage );
	}
}


/*
* G_PrintMsg
*
* NULL sends to all the message to all clients
*/
void G_PrintMsg( edict_t *ent, const char *format, ... ) {
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
void G_ChatMsg( edict_t *ent, edict_t *who, bool teamonly, const char *format, ... ) {
	char msg[MAX_STRING_CHARS];
	va_list argptr;

	va_start( argptr, format );
	vsnprintf( msg, sizeof( msg ), format, argptr );
	va_end( argptr );

	// double quotes are bad
	char * p = msg;
	while( ( p = strchr( p, '\"' ) ) != NULL )
		*p = '\'';

	char cmd[ MAX_STRING_CHARS ];
	snprintf( cmd, sizeof( cmd ), "%s %d \"%s\"", ( who && teamonly ? "tch" : "ch" ), ( who ? ENTNUM( who ) : 0 ), msg );

	if( !ent ) {
		// mirror at server console
		if( is_dedicated_server ) {
			if( !who ) {
				Com_Printf( "Console: %s\n", msg );     // admin console
			} else if( !who->r.client ) {
				;   // wtf?
			} else if( teamonly ) {
				Com_Printf( "[%s] %s %s\n",
						  who->r.client->ps.team == Team_None ? "SPEC" : "TEAM", who->r.client->netname, msg );
			} else {
				Com_Printf( "%s: %s\n", who->r.client->netname, msg );
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
void G_CenterPrintMsg( edict_t *ent, const char *format, ... ) {
	char msg[1024];
	char cmd[MAX_STRING_CHARS];
	va_list argptr;
	char *p;
	edict_t *other;

	va_start( argptr, format );
	vsnprintf( msg, sizeof( msg ), format, argptr );
	va_end( argptr );

	// double quotes are bad
	p = msg;
	while( ( p = strchr( p, '\"' ) ) != NULL )
		*p = '\'';

	snprintf( cmd, sizeof( cmd ), "cp \"%s\"", msg );
	PF_GameCmd( ent, cmd );

	if( ent != NULL ) {
		// add it to every player who's chasing this player
		for( other = game.edicts + 1; PLAYERNUM( other ) < server_gs.maxclients; other++ ) {
			if( !other->r.client || !other->r.inuse || !other->r.client->resp.chase.active ) {
				continue;
			}

			if( other->r.client->resp.chase.target == ENTNUM( ent ) ) {
				PF_GameCmd( other, cmd );
			}
		}
	}
}

void G_ClearCenterPrint( edict_t *ent ) {
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
	ent->s.origin = ( owner->r.absmin + owner->r.absmax ) * 0.5f;

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
	ent->s.svflags |= SVF_ONLYOWNER | SVF_BROADCAST;

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
bool KillBox( edict_t *ent, DamageType damage_type, Vec3 knockback ) {
	trace_t tr;
	bool telefragged = false;

	while( true ) {
		G_Trace( &tr, ent->s.origin, ent->r.mins, ent->r.maxs, ent->s.origin, world, MASK_PLAYERSOLID );
		if( ( tr.fraction == 1.0f && !tr.startsolid ) || tr.ent < 0 ) {
			return telefragged;
		}

		if( tr.ent == ENTNUM( world ) ) {
			return telefragged; // found the world (but a player could be in there too). suicide?
		}

		// nail it
		G_Damage( &game.edicts[tr.ent], ent, ent, knockback, Vec3( 0.0f ), ent->s.origin, 200, Length( knockback ), 0, damage_type );
		telefragged = true;

		// if we didn't kill it, fail
		if( game.edicts[tr.ent].r.solid ) {
			return telefragged;
		}
	}

	return telefragged; // all clear
}

float LookAtKillerYAW( edict_t *self, edict_t *inflictor, edict_t *attacker ) {
	Vec3 dir;

	if( attacker && attacker != world && attacker != self ) {
		dir = attacker->s.origin - self->s.origin;
	} else if( inflictor && inflictor != world && inflictor != self ) {
		dir = inflictor->s.origin - self->s.origin;
	} else {
		return self->s.angles.y;
	}

	return VecToAngles( dir ).y;
}

//==============================================================================
//
//		Warsow: more miscelanea tools
//
//==============================================================================

static void G_SpawnTeleportEffect( edict_t *ent, bool respawn, bool in ) {
	edict_t *event;

	if( !ent || !ent->r.client ) {
		return;
	}

	if( PF_GetClientState( PLAYERNUM( ent ) ) < CS_SPAWNED || ent->r.solid == SOLID_NOT ) {
		return;
	}

	// add a teleportation effect
	event = G_SpawnEvent( respawn ? EV_PLAYER_RESPAWN : ( in ? EV_PLAYER_TELEPORT_IN : EV_PLAYER_TELEPORT_OUT ), 0, &ent->s.origin );
	event->s.ownerNum = ENTNUM( ent );
}

void G_TeleportEffect( edict_t *ent, bool in ) {
	G_SpawnTeleportEffect( ent, false, in );
}

void G_RespawnEffect( edict_t *ent ) {
	G_SpawnTeleportEffect( ent, true, false );
}

int G_SolidMaskForEnt( edict_t *ent ) {
	return ent->r.clipmask ? ent->r.clipmask : MASK_SOLID;
}

void G_CheckGround( edict_t *ent ) {
	trace_t trace;

	if( ent->r.client && ent->velocity.z > 180 ) {
		ent->groundentity = NULL;
		ent->groundentity_linkcount = 0;
		return;
	}

	// if the hull point one-quarter unit down is solid the entity is on ground
	Vec3 point = ent->s.origin;
	point.z -= 0.25f;

	G_Trace( &trace, ent->s.origin, ent->r.mins, ent->r.maxs, point, ent, G_SolidMaskForEnt( ent ) );

	// check steepness
	if( !ISWALKABLEPLANE( &trace.plane ) && !trace.startsolid ) {
		ent->groundentity = NULL;
		ent->groundentity_linkcount = 0;
		return;
	}

	if( ent->velocity.z > 1.0f && !ent->r.client && !trace.startsolid ) {
		ent->groundentity = NULL;
		ent->groundentity_linkcount = 0;
		return;
	}

	if( !trace.startsolid && !trace.allsolid ) {
		//VectorCopy( trace.endpos, ent->s.origin );
		ent->groundentity = &game.edicts[trace.ent];
		ent->groundentity_linkcount = ent->groundentity->linkcount;
		if( ent->velocity.z < 0.0f ) {
			ent->velocity.z = 0.0f;
		}
	}
}

void G_SetBoundsForSpanEntity( edict_t *ent, float size ) {
	ClearBounds( &ent->r.absmin, &ent->r.absmax );
	AddPointToBounds( ent->s.origin, &ent->r.absmin, &ent->r.absmax );
	AddPointToBounds( ent->s.origin2, &ent->r.absmin, &ent->r.absmax );
	ent->r.absmin -= size;
	ent->r.absmax += size;
	ent->r.mins = ent->r.absmin - ent->s.origin;
	ent->r.maxs = ent->r.absmax - ent->s.origin;
}

void G_ReleaseClientPSEvent( gclient_t *client ) {
	for( int i = 0; i < 2; i++ ) {
		if( client->resp.eventsCurrent < client->resp.eventsHead ) {
			client->ps.events[ i ] = client->resp.events[client->resp.eventsCurrent & MAX_CLIENT_EVENTS_MASK];
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
void G_AddPlayerStateEvent( gclient_t *client, int ev, u64 parm ) {
	assert( ev >= 0 && ev < PSEV_MAX_EVENTS );
	if( client == NULL )
		return;

	SyncEvent * event = &client->resp.events[client->resp.eventsHead & MAX_CLIENT_EVENTS_MASK];
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
edict_t *G_PlayerForText( const char *text ) {
	if( !text || !text[0] ) {
		return NULL;
	}

	u64 num;
	if( TrySpanToU64( MakeSpan( text ), &num ) && num < u64( server_gs.maxclients ) && game.edicts[ num + 1 ].r.inuse ) {
		return &game.edicts[ num + 1 ];
	}

	for( int i = 0; i < server_gs.maxclients; i++ ) {
		edict_t * e = &game.edicts[ i + 1 ];
		if( StrCaseEqual( e->r.client->netname, text ) ) {
			return e;
		}
	}

	return NULL;
}

void G_AnnouncerSound( edict_t *targ, StringHash sound, Team team, bool queued, edict_t *ignore ) {
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
		edict_t *ent;

		for( ent = game.edicts + 1; PLAYERNUM( ent ) < server_gs.maxclients; ent++ ) {
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
