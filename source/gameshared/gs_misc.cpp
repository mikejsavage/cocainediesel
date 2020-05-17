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

#include "qcommon/qcommon.h"

Vec3 GS_EvaluateJumppad( const SyncEntityState * jumppad, Vec3 velocity ) {
	if( jumppad->type == ET_PAINKILLER_JUMPPAD ) {
		Vec3 push = jumppad->origin2;

		Vec3 parallel = Project( velocity, push );
		Vec3 perpendicular = velocity - parallel;

		return perpendicular + push;
	}

	return jumppad->origin2;
}

void GS_TouchPushTrigger( const gs_state_t * gs, SyncPlayerState * playerState, const SyncEntityState * pusher ) {
	// spectators don't use jump pads
	if( playerState->pmove.pm_type != PM_NORMAL ) {
		return;
	}

	GS_EvaluateJumppad( pusher, playerState->pmove.velocity );

	// reset walljump counter
	playerState->pmove.pm_flags &= ~PMF_WALLJUMPCOUNT;
	playerState->pmove.pm_flags |= PMF_JUMPPAD_TIME;
	playerState->pmove.pm_flags &= ~PMF_ON_GROUND;
	gs->api.PredictedEvent( playerState->POVnum, EV_JUMP_PAD, 0 );
}

/*
* GS_WaterLevel
*/
int GS_WaterLevel( const gs_state_t * gs, SyncEntityState *state, Vec3 mins, Vec3 maxs ) {
	int waterlevel = 0;

	Vec3 point = state->origin;
	point.z += mins.z + 1;

	int cont = gs->api.PointContents( point, 0 );
	if( cont & MASK_WATER ) {
		waterlevel = 1;
		point.z += 26;
		cont = gs->api.PointContents( point, 0 );
		if( cont & MASK_WATER ) {
			waterlevel = 2;
			point.z += 22;
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
		case MOD_SHOTGUN:
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
		case MOD_BUBBLEGUN:
			strcpy( message, "was melted by" );
			strcpy( message2, "'s plasmagun" );
			break;
		case MOD_RAILGUN:
			strcpy( message, "was bolted by" );
			strcpy( message2, "'s electrobolt" );
			break;
		case MOD_LASERGUN:
			strcpy( message, "was cut by" );
			strcpy( message2, "'s lasergun" );
			break;

		default:
			strcpy( message, "was fragged by" );
			break;
	}
}
