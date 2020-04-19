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
#include "gameshared/gs_public.h"

#define STOP_EPSILON    0.1

//==================================================
// SNAP AND CLIP ORIGIN AND VELOCITY
//==================================================

/*
* GS_ClipVelocity
*/
void GS_ClipVelocity( vec3_t in, vec3_t normal, vec3_t out, float overbounce ) {
	float backoff = DotProduct( in, normal );
	if( backoff <= 0 ) {
		backoff *= overbounce;
	} else {
		backoff /= overbounce;
	}

	for( int i = 0; i < 3; i++ ) {
		float change = normal[i] * backoff;
		out[i] = in[i] - change;
	}

	float oldspeed, newspeed;
	oldspeed = VectorLength( in );
	newspeed = VectorLength( out );
	if( newspeed > oldspeed ) {
		VectorNormalize( out );
		VectorScale( out, oldspeed, out );
	}
}

/*
* GS_LinearMovement
*/
int GS_LinearMovement( const SyncEntityState *ent, int64_t time, vec3_t dest ) {
	vec3_t dist;
	int moveTime;
	float moveFrac;

	moveTime = time - ent->linearMovementTimeStamp;
	if( moveTime < 0 ) {
		moveTime = 0;
	}

	if( ent->linearMovementDuration ) {
		if( moveTime > (int)ent->linearMovementDuration ) {
			moveTime = ent->linearMovementDuration;
		}

		VectorSubtract( ent->linearMovementEnd, ent->linearMovementBegin, dist );
		moveFrac = Clamp01( (float)moveTime / (float)ent->linearMovementDuration );
		VectorMA( ent->linearMovementBegin, moveFrac, dist, dest );
	} else {
		moveFrac = moveTime * 0.001f;
		VectorMA( ent->linearMovementBegin, moveFrac, ent->linearMovementVelocity, dest );
	}

	return moveTime;
}

/*
* GS_LinearMovementDelta
*/
void GS_LinearMovementDelta( const SyncEntityState *ent, int64_t oldTime, int64_t curTime, vec3_t dest ) {
	vec3_t p1, p2;
	GS_LinearMovement( ent, oldTime, p1 );
	GS_LinearMovement( ent, curTime, p2 );
	VectorSubtract( p2, p1, dest );
}
