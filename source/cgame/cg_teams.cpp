#include "cgame/cg_local.h"
#include "qcommon/srgb.h"

static constexpr RGB8 TEAM_COLORS[] = {
	RGB8( 40, 204, 255 ),
	RGB8( 255, 24, 96 ),
};

static constexpr RGB8 COLORBLIND_TEAM_COLORS[] = {
	RGB8( 255, 150, 40 ),
	RGB8( 190, 0, 240 ),
};

static bool IsAlly( int team ) {
	int myteam = cg.predictedPlayerState.team;
	if( myteam == TEAM_SPECTATOR )
		return team == TEAM_ALPHA;
	if( myteam == TEAM_PLAYERS )
		return false;
	return team == myteam;
}

RGB8 CG_TeamColor( int team ) {
	if( team == TEAM_PLAYERS )
		return RGB8( 255, 255, 255 );
	return IsAlly( team ) ? AllyColor() : EnemyColor();
}

RGB8 AllyColor() {
	const RGB8 * colors = Cvar_Bool( "cg_colorBlind" ) ? COLORBLIND_TEAM_COLORS : TEAM_COLORS;
	return colors[ 0 ];
}

RGB8 EnemyColor() {
	const RGB8 * colors = Cvar_Bool( "cg_colorBlind" ) ? COLORBLIND_TEAM_COLORS : TEAM_COLORS;
	return colors[ 1 ];
}

static Vec4 RGB8ToVec4( RGB8 rgb ) {
	return Vec4( sRGBToLinear( rgb ), 1.0f );
}

Vec4 CG_TeamColorVec4( int team ) {
	return RGB8ToVec4( CG_TeamColor( team ) );
}

Vec4 AllyColorVec4() {
	return RGB8ToVec4( AllyColor() );
}

Vec4 EnemyColorVec4() {
	return RGB8ToVec4( EnemyColor() );
}
