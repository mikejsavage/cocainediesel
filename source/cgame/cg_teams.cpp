#include "cgame/cg_local.h"
#include "qcommon/srgb.h"

static constexpr RGB8 TEAM_COLORS[] = {
	RGB8( 255, 255, 255 ), // Team_None

	RGB8( 40, 204, 255 ), //blue
	RGB8( 255, 24, 96 ), //red
	RGB8( 100, 255, 100 ), //green
	RGB8( 255, 255, 34 ), //yellow
	RGB8( 170, 75, 255 ), //purple
	RGB8( 0, 70, 255 ), //dark-blue
	RGB8( 255, 100, 255 ), //pink
	RGB8( 0, 255, 170 ), //light-blue
};

static constexpr RGB8 COLORBLIND_TEAM_COLORS[] = {
	RGB8( 255, 255, 255 ), // Team_None

	RGB8( 40, 204, 255 ),
	RGB8( 255, 150, 40 ), //orange
	RGB8( 100, 255, 100 ),
	RGB8( 255, 255, 34 ),
	RGB8( 138, 43, 226 ),
	RGB8( 0, 47, 167 ),
	RGB8( 255, 179, 222 ),
	RGB8( 172, 229, 255 ),
};

static bool IsAlly( Team team ) {
	Team my_team = cg.predictedPlayerState.real_team;
	if( my_team == Team_None )
		return team == Team_One;
	return team == my_team;
}

RGB8 CG_RealTeamColor( Team team ) {
	return Cvar_Bool( "cg_colorBlind" ) ? COLORBLIND_TEAM_COLORS[ team ] : TEAM_COLORS[ team ];
}

RGB8 CG_TeamColor( Team team ) {
	if( cg.frame.gameState.gametype == Gametype_Gladiator )
		return CG_RealTeamColor( team );
	return IsAlly( team ) ? AllyColor() : EnemyColor();
}

RGB8 AllyColor() {
	const RGB8 * colors = Cvar_Bool( "cg_colorBlind" ) ? COLORBLIND_TEAM_COLORS : TEAM_COLORS;
	return colors[ Team_One ];
}

RGB8 EnemyColor() {
	const RGB8 * colors = Cvar_Bool( "cg_colorBlind" ) ? COLORBLIND_TEAM_COLORS : TEAM_COLORS;
	return colors[ Team_Two ];
}

static Vec4 RGB8ToVec4( RGB8 rgb ) {
	return Vec4( sRGBToLinear( rgb ), 1.0f );
}

Vec4 CG_TeamColorVec4( Team team ) {
	return RGB8ToVec4( CG_TeamColor( team ) );
}

Vec4 AllyColorVec4() {
	return RGB8ToVec4( AllyColor() );
}

Vec4 EnemyColorVec4() {
	return RGB8ToVec4( EnemyColor() );
}
