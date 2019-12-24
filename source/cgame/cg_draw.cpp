#include "cg_local.h"

int CG_HorizontalAlignForWidth( int x, Alignment alignment, int width ) {
	if( alignment.x == XAlignment_Left )
		return x;
	if( alignment.x == XAlignment_Center )
		return x - width / 2;
	return x - width;
}

int CG_VerticalAlignForHeight( int y, Alignment alignment, int height ) {
	if( alignment.y == YAlignment_Top )
		return y;
	if( alignment.y == YAlignment_Middle )
		return y - height / 2;
	return y - height;
}

static Vec2 ClipToScreen( Vec2 clip ) {
	clip.y = -clip.y;
	return ( clip + 1.0f ) / 2.0f * frame_static.viewport;
}

Vec2 WorldToScreen( Vec3 v ) {
	Vec4 clip = frame_static.P * frame_static.V * Vec4( v, 1.0 );
	if( clip.z == 0 )
		return Vec2( 0, 0 );
	return ClipToScreen( clip.xy() / clip.w );
}

Vec2 WorldToScreenClamped( Vec3 v, Vec2 screen_border, bool * clamped ) {
	*clamped = false;

	Vec4 clip = frame_static.P * frame_static.V * Vec4( v, 1.0 );
	if( clip.z == 0 )
		return Vec2( 0, 0 );

	Vec2 res = clip.xy() / clip.w;

	Vec3 forward = -frame_static.V.row2().xyz();
	float d = Dot( v - frame_static.position, forward );
	if( d < 0 )
		res = -res;

	screen_border = 1.0f - ( screen_border / frame_static.viewport );
	if( Abs( res.x ) > screen_border.x || Abs( res.y ) > screen_border.y || d < 0 ) {
		float rx = screen_border.x / Abs( res.x );
		float ry = screen_border.y / Abs( res.y );

		res *= Min2( rx, ry );

		*clamped = true;
	}

	return ClipToScreen( res );
}
