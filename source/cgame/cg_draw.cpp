/*
Copyright (C) 2002-2003 Victor Luchits

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

#include "cg_local.h"

int CG_HorizontalAlignForWidth( const int x, int align, int width ) {
	int nx = x;

	if( align % 3 == 0 ) { // left
		nx = x;
	}
	if( align % 3 == 1 ) { // center
		nx = x - width / 2;
	}
	if( align % 3 == 2 ) { // right
		nx = x - width;
	}

	return nx;
}

int CG_VerticalAlignForHeight( const int y, int align, int height ) {
	int ny = y;

	if( align / 3 == 0 ) { // top
		ny = y;
	} else if( align / 3 == 1 ) { // middle
		ny = y - height / 2;
	} else if( align / 3 == 2 ) { // bottom
		ny = y - height;
	}

	return ny;
}

static Vec2 ClipToScreen( Vec2 clip ) {
	clip.y = -clip.y;
	return ( clip + 1.0f ) / 2.0f * frame_static.viewport;
}

Vec2 WorldToScreen( Vec3 v ) {
	Vec4 clip = frame_static.P * frame_static.V * Vec4( v, 1.0 );
	if( clip.z == 0 )
		return Vec2( 0, 0 );
	return ClipToScreen( clip.xy() / clip.z );
}

Vec2 WorldToScreenClamped( Vec3 v, Vec2 screen_border, bool * clamped ) {
	*clamped = false;

	Vec4 clip = frame_static.P * frame_static.V * Vec4( v, 1.0 );
	if( clip.z == 0 )
		return Vec2( 0, 0 );

	Vec2 res = clip.xy() / clip.z;

	Vec3 forward = -frame_static.V.row2().xyz();
	float d = Dot( v - frame_static.position, forward );
	if( d < 0 )
		res = -res;

	screen_border = 1.0f - ( screen_border / frame_static.viewport );
	if( fabsf( res.x ) > screen_border.x || fabsf( res.y ) > screen_border.y || d < 0 ) {
		float rx = screen_border.x / fabsf( res.x );
		float ry = screen_border.y / fabsf( res.y );

		res *= Min2( rx, ry );

		*clamped = true;
	}

	return ClipToScreen( res );
}
