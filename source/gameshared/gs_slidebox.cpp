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
Vec3 GS_ClipVelocity( Vec3 in, Vec3 normal, float overbounce ) {
	float backoff = Dot( in, normal );
	if( backoff <= 0 ) {
		backoff *= overbounce;
	} else {
		backoff /= overbounce;
	}

	Vec3 out = in - normal * backoff;

	float oldspeed = Length( in );
	float newspeed = Length( out );
	if( newspeed > oldspeed ) {
		out = out / newspeed * oldspeed;
	}

	return out;
}

/*
* GS_LinearMovement
*/
int GS_LinearMovement( const SyncEntityState *ent, int64_t time, Vec3 * dest ) {
	int moveTime = time - ent->linearMovementTimeStamp;
	if( moveTime < 0 ) {
		moveTime = 0;
	}

	if( ent->linearMovementDuration ) {
		if( moveTime > (int)ent->linearMovementDuration ) {
			moveTime = ent->linearMovementDuration;
		}

		Vec3 dist = ent->linearMovementEnd - ent->linearMovementBegin;
		float moveFrac = Clamp01( (float)moveTime / (float)ent->linearMovementDuration );
		*dest = ent->linearMovementBegin + dist * moveFrac;
	} else {
		float moveFrac = moveTime * 0.001f;
		*dest = ent->linearMovementBegin + ent->linearMovementVelocity * moveFrac;
	}

	return moveTime;
}

/*
* GS_LinearMovementDelta
*/
void GS_LinearMovementDelta( const SyncEntityState *ent, int64_t oldTime, int64_t curTime, Vec3 * dest ) {
	Vec3 p1, p2;
	GS_LinearMovement( ent, oldTime, &p1 );
	GS_LinearMovement( ent, curTime, &p2 );
	*dest = p2 - p1;
}
