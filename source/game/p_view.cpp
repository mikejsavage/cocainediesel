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

#include "qcommon/base.h"
#include "game/g_local.h"

//====================================================================
// DEAD VIEW
//====================================================================

static void G_ProjectThirdPersonView( Vec3 * vieworg, EulerDegrees3 * viewangles, edict_t *passent ) {
	float thirdPersonRange = 60;
	float thirdPersonAngle = 0;
	Vec3 v_forward, v_right, v_up;

	AngleVectors( *viewangles, &v_forward, &v_right, &v_up );

	// calc exact destination
	Vec3 chase_dest = *vieworg;
	float r = Radians( thirdPersonAngle );
	float f = -cosf( r );
	r = -sinf( r );
	chase_dest += v_forward * thirdPersonRange * f;
	chase_dest += v_right * thirdPersonRange * r;
	chase_dest.z += 8;

	// find the spot the player is looking at
	Vec3 dest = *vieworg + v_forward * 512.0f;
	trace_t trace = G_Trace( *vieworg, MinMax3( 4.0f ), dest, passent, SolidMask_Opaque );

	// calculate pitch to look at the same spot from camera
	Vec3 stop = trace.endpos - *vieworg;
	float dist = Length( stop.xy() );
	if( dist < 1 ) {
		dist = 1;
	}
	viewangles->pitch = Degrees( -atan2f( stop.z, dist ) );
	viewangles->yaw -= thirdPersonAngle;
	AngleVectors( *viewangles, &v_forward, &v_right, &v_up );

	// move towards destination
	trace = G_Trace( *vieworg, MinMax3( 4.0f ), chase_dest, passent, SolidMask_Opaque );

	if( trace.HitSomething() ) {
		stop = trace.endpos;
		stop.z += ( 1.0f - trace.fraction ) * 32;
		trace = G_Trace( *vieworg, MinMax3( 4.0f ), stop, passent, SolidMask_Opaque );
		chase_dest = trace.endpos;
	}

	*vieworg = chase_dest;
}

static void G_Client_DeadView( edict_t *ent ) {
	gclient_t * client = ent->r.client;
	edict_t * body = &game.edicts[ ent->s.ownerNum ];

	if( body->activator != ent ) { // ran all the list and didn't find our body
		return;
	}

	// move us to body position
	ent->s.origin = body->s.origin;
	ent->s.teleported = true;
	client->ps.viewangles.roll = 0;
	client->ps.viewangles.pitch = 0;

	// see if our killer is still in view
	if( body->enemy && ( body->enemy != ent ) ) {
		trace_t trace = G_Trace( ent->s.origin, MinMax3( 0.0f ), body->enemy->s.origin, body, SolidMask_Opaque );
		if( trace.HitSomething() ) {
			body->enemy = NULL;
		} else {
			client->ps.viewangles.yaw = LookAtKillerYAW( ent, NULL, body->enemy );
		}
	} else {   // nobody killed us, so just circle around the body ?

	}

	G_ProjectThirdPersonView( &ent->s.origin, &ent->s.angles, body );
	client->ps.pmove.origin = ent->s.origin;
	client->ps.viewangles = ent->s.angles;
	client->ps.pmove.velocity = Vec3( 0.0f );
}

//====================================================================
// EFFECTS
//====================================================================

void G_ClientAddDamageIndicatorImpact( gclient_t *client, int damage, const Vec3 basedir ) {
	if( damage < 1 ) {
		return;
	}

	if( !client || client - game.clients < 0 || client - game.clients >= server_gs.maxclients ) {
		return;
	}

	Vec3 dir = SafeNormalize( basedir );

	float frac = (float)damage / ( damage + client->snap.damageTaken );
	client->snap.damageTakenDir = Lerp( client->snap.damageTakenDir, frac, dir );
	client->snap.damageTaken += damage;
}

/*
* G_ClientDamageFeedback
*
* Adds color blends, hitsounds, etc
*/
void G_ClientDamageFeedback( edict_t *ent ) {
	if( ent->r.client->snap.damageTaken ) {
		int damage = ent->r.client->snap.damageTaken;
		u64 parm = DirToU64( ent->r.client->snap.damageTakenDir );

		if( damage <= 10 ) {
			G_AddPlayerStateEvent( ent->r.client, PSEV_DAMAGE_10, parm );
		} else if( damage <= 20 ) {
			G_AddPlayerStateEvent( ent->r.client, PSEV_DAMAGE_20, parm );
		} else if( damage <= 30 ) {
			G_AddPlayerStateEvent( ent->r.client, PSEV_DAMAGE_30, parm );
		} else {
			G_AddPlayerStateEvent( ent->r.client, PSEV_DAMAGE_40, parm );
		}
	}

	// add hitsounds from given damage
	if( ent->r.client->snap.kill ) { //kill
		G_AddPlayerStateEvent( ent->r.client, PSEV_HIT, 4 );
	} else if( ent->r.client->snap.damage_given >= 35 ) {
		G_AddPlayerStateEvent( ent->r.client, PSEV_HIT, 0 );
	} else if( ent->r.client->snap.damage_given >= 20 ) {
		G_AddPlayerStateEvent( ent->r.client, PSEV_HIT, 1 );
	} else if( ent->r.client->snap.damage_given >= 10 ) {
		G_AddPlayerStateEvent( ent->r.client, PSEV_HIT, 2 );
	} else if( ent->r.client->snap.damage_given ) {
		G_AddPlayerStateEvent( ent->r.client, PSEV_HIT, 3 );
	}
}

void G_ClientSetStats( edict_t * ent ) {
	gclient_t * client = ent->r.client;
	SyncPlayerState * ps = &client->ps;

	ps->ready = server_gs.gameState.match_state <= MatchState_Warmup && level.ready[ PLAYERNUM( ent ) ];
	ps->voted = G_Callvotes_HasVoted( ent );
	ps->team = ent->s.team;
	ps->real_team = ent->s.team;
	ps->health = ent->s.team == Team_None ? 0 : HEALTH_TO_INT( ent->health );
	ps->max_health = HEALTH_TO_INT( ent->max_health );
}

/*
* G_ClientEndSnapFrame
*
* Called for each player at the end of the server frame
* and right after spawning
*/
void G_ClientEndSnapFrame( edict_t *ent ) {
	if( PF_GetClientState( PLAYERNUM( ent ) ) < CS_SPAWNED ) {
		return;
	}

	gclient_t * client = ent->r.client;

	// If the end of unit layout is displayed, don't give
	// the player any normal movement attributes
	if( server_gs.gameState.match_state >= MatchState_PostMatch ) {
		G_ClientSetStats( ent );
	} else {
		if( G_IsDead( ent ) ) {
			G_Client_DeadView( ent );
		}

		G_ClientDamageFeedback( ent ); // show damage taken along the snap
		G_ClientSetStats( ent );
	}

	G_ReleaseClientPSEvent( client );

	client->ps.pmove.angles = EulerDegrees3( client->ucmd.angles );

	// this is pretty hackish
	if( !G_ISGHOSTING( ent ) ) {
		ent->s.origin2 = ent->velocity;
	}
}
