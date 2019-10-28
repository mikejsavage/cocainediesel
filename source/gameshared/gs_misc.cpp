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

#include "q_arch.h"
#include "q_math.h"
#include "q_shared.h"
#include "q_comref.h"
#include "q_collision.h"
#include "gs_public.h"

/*
* GS_TouchPushTrigger
*/
void GS_TouchPushTrigger( const gs_state_t * gs, player_state_t *playerState, entity_state_t *pusher ) {
	// spectators don't use jump pads
	if( playerState->pmove.pm_type != PM_NORMAL ) {
		return;
	}

	VectorCopy( pusher->origin2, playerState->pmove.velocity );

	// reset walljump counter
	playerState->pmove.pm_flags &= ~PMF_WALLJUMPCOUNT;
	playerState->pmove.pm_flags |= PMF_JUMPPAD_TIME;
	playerState->pmove.pm_flags &= ~PMF_ON_GROUND;
	gs->api.PredictedEvent( playerState->POVnum, EV_JUMP_PAD, 0 );
}

/*
* GS_WaterLevel
*/
int GS_WaterLevel( const gs_state_t * gs, entity_state_t *state, vec3_t mins, vec3_t maxs ) {
	vec3_t point;
	int cont;
	int waterlevel;

	waterlevel = 0;

	point[0] = state->origin[0];
	point[1] = state->origin[1];
	point[2] = state->origin[2] + mins[2] + 1;
	cont = gs->api.PointContents( point, 0 );
	if( cont & MASK_WATER ) {
		waterlevel = 1;
		point[2] += 26;
		cont = gs->api.PointContents( point, 0 );
		if( cont & MASK_WATER ) {
			waterlevel = 2;
			point[2] += 22;
			cont = gs->api.PointContents( point, 0 );
			if( cont & MASK_WATER ) {
				waterlevel = 3;
			}
		}
	}

	return waterlevel;
}

//============================================================================

/*
* GS_Obituary
*
* Can be called by either the server or the client
*/
void GS_Obituary( void *victim, void *attacker, int mod, char *message, char *message2 ) {
	message[0] = 0;
	message2[0] = 0;

	if( !attacker || attacker == victim ) {
		switch( mod ) {
			case MOD_SUICIDE:
				strcpy( message, "suicides" );
				break;
			case MOD_CRUSH:
				strcpy( message, "was squished" );
				break;
			case MOD_SLIME:
				strcpy( message, "melted" );
				break;
			case MOD_LAVA:
				strcpy( message, "sacrificed to the lava god" ); // wsw : pb : some killed messages
				break;
			case MOD_TRIGGER_HURT:
				strcpy( message, "was in the wrong place" );
				break;
			case MOD_LASER:
				strcpy( message, "was cut in half" );
				break;
			case MOD_SPIKES:
				strcpy( message, "was impaled by spikes" );
				break;
			default:
				strcpy( message, "died" );
				break;
		}
		return;
	}

	switch( mod ) {
		case MOD_TELEFRAG:
			strcpy( message, "tried to invade" );
			strcpy( message2, "'s personal space" );
			break;
		case MOD_GUNBLADE:
			strcpy( message, "was impaled by" );
			strcpy( message2, "'s gunblade" );
			break;
		case MOD_MACHINEGUN:
			strcpy( message, "was penetrated by" );
			strcpy( message2, "'s machinegun" );
			break;
		case MOD_RIOTGUN:
			strcpy( message, "was shredded by" );
			strcpy( message2, "'s riotgun" );
			break;
		case MOD_GRENADE:
			strcpy( message, "was popped by" );
			strcpy( message2, "'s grenade" );
			break;
		case MOD_ROCKET:
			strcpy( message, "ate" );
			strcpy( message2, "'s rocket" );
			break;
		case MOD_PLASMA:
			strcpy( message, "was melted by" );
			strcpy( message2, "'s plasmagun" );
			break;
		case MOD_ELECTROBOLT:
			strcpy( message, "was bolted by" );
			strcpy( message2, "'s electrobolt" );
			break;
		case MOD_LASERGUN:
			strcpy( message, "was cut by" );
			strcpy( message2, "'s lasergun" );
			break;
		case MOD_GRENADE_SPLASH:
			strcpy( message, "didn't see" );
			strcpy( message2, "'s grenade" );
			break;
		case MOD_ROCKET_SPLASH:
			strcpy( message, "almost dodged" );
			strcpy( message2, "'s rocket" );
			break;

		case MOD_PLASMA_SPLASH:
			strcpy( message, "was melted by" );
			strcpy( message2, "'s plasmagun" );
			break;

		default:
			strcpy( message, "was fragged by" );
			break;
	}
}
