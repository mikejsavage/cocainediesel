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
#include "gameshared/gs_synctypes.h"

Vec3 GS_ClipVelocity( Vec3 in, Vec3 normal ) {
	const float overbounce = 1.0001f;
	float dot = Dot( in, normal );
	if( dot < 0.0f ) {
		dot *= overbounce;
	}
	else {
		dot /= overbounce;
	}
	return in - normal * dot;
}

Vec3 GS_LinearMovement( const SyncEntityState * ent, int64_t time ) {
	int64_t moveTime = Max2( s64( 0 ), time - ent->linearMovementTimeStamp );

	if( ent->linearMovementDuration == 0 ) {
		return ent->linearMovementBegin + ent->linearMovementVelocity * moveTime * 0.001f;
	}

	moveTime = Min2( moveTime, s64( ent->linearMovementDuration ) );
	float t = float( moveTime ) / float( ent->linearMovementDuration );
	return Lerp( ent->linearMovementBegin, t, ent->linearMovementEnd );
}

Vec3 GS_LinearMovementDelta( const SyncEntityState * ent, int64_t oldTime, int64_t curTime ) {
	return GS_LinearMovement( ent, curTime ) - GS_LinearMovement( ent, oldTime );
}
