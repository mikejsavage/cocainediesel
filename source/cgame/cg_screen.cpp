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

// cg_screen.c -- master status bar, crosshairs, hud, etc

/*

full screen console
put up loading plaque
blanked background with loading plaque
blanked background with menu
full screen image for quit and victory

end of unit intermissions

*/

#include "cg_local.h"
#include "client/client.h"

cvar_t *cg_centerTime;
cvar_t *cg_showFPS;
cvar_t *cg_showPointedPlayer;
cvar_t *cg_draw2D;

cvar_t *cg_crosshair_color;
cvar_t *cg_crosshair_damage_color;
cvar_t *cg_crosshair_size;

cvar_t *cg_clientHUD;
cvar_t *cg_specHUD;
cvar_t *cg_showSpeed;
cvar_t *cg_showAwards;

cvar_t *cg_showPlayerNames;
cvar_t *cg_showPlayerNames_alpha;
cvar_t *cg_showPlayerNames_zfar;
cvar_t *cg_showPlayerNames_barWidth;

cvar_t *cg_showPressedKeys;

cvar_t *cg_scoreboardWidthScale;
cvar_t *cg_scoreboardStats;

cvar_t *cg_showViewBlends;

static int64_t scr_damagetime = 0;

/*
===============================================================================

CENTER PRINTING

===============================================================================
*/

char scr_centerstring[1024];
int scr_centertime_off;
int scr_erase_center;

/*
* CG_CenterPrint
*
* Called for important messages that should stay in the center of the screen
* for a few moments
*/
void CG_CenterPrint( const char *str ) {
	Q_strncpyz( scr_centerstring, str, sizeof( scr_centerstring ) );
	scr_centertime_off = cg_centerTime->value * 1000.0f;
}

static void CG_DrawCenterString( void ) {
	DrawText( cgs.fontMontserrat, cgs.textSizeMedium, scr_centerstring, Alignment_CenterTop, cgs.vidWidth * 0.5f, cgs.vidHeight * 0.35f, rgba8_white, true, rgba8_black );
}

//============================================================================

/*
* CG_ScreenInit
*/
void CG_ScreenInit( void ) {
	cg_showFPS =        trap_Cvar_Get( "cg_showFPS", "0", CVAR_ARCHIVE );
	cg_draw2D =     trap_Cvar_Get( "cg_draw2D", "1", 0 );
	cg_centerTime =     trap_Cvar_Get( "cg_centerTime", "2.5", 0 );

	cg_crosshair_color =    trap_Cvar_Get( "cg_crosshair_color", "255 255 255", CVAR_ARCHIVE );
	cg_crosshair_damage_color = trap_Cvar_Get( "cg_crosshair_damage_color", "255 0 0", CVAR_ARCHIVE );
	cg_crosshair_size = trap_Cvar_Get( "cg_crosshair_size", "3", CVAR_ARCHIVE );
	cg_crosshair_color->modified = true;
	cg_crosshair_damage_color->modified = true;

	cg_clientHUD =      trap_Cvar_Get( "cg_clientHUD", "default", CVAR_ARCHIVE );
	cg_specHUD =        trap_Cvar_Get( "cg_specHUD", "default", CVAR_ARCHIVE );
	cg_showSpeed =      trap_Cvar_Get( "cg_showSpeed", "0", CVAR_ARCHIVE );
	cg_showPointedPlayer =  trap_Cvar_Get( "cg_showPointedPlayer", "1", CVAR_ARCHIVE );
	cg_showViewBlends = trap_Cvar_Get( "cg_showViewBlends", "1", CVAR_ARCHIVE );
	cg_showAwards =     trap_Cvar_Get( "cg_showAwards", "1", CVAR_ARCHIVE );

	cg_showPlayerNames =        trap_Cvar_Get( "cg_showPlayerNames", "2", CVAR_ARCHIVE );
	cg_showPlayerNames_alpha =  trap_Cvar_Get( "cg_showPlayerNames_alpha", "0.4", CVAR_ARCHIVE );
	cg_showPlayerNames_zfar =   trap_Cvar_Get( "cg_showPlayerNames_zfar", "1024", CVAR_ARCHIVE );
	cg_showPlayerNames_barWidth =   trap_Cvar_Get( "cg_showPlayerNames_barWidth", "8", CVAR_ARCHIVE );

	cg_showPressedKeys = trap_Cvar_Get( "cg_showPressedKeys", "0", CVAR_ARCHIVE );

	cg_scoreboardWidthScale = trap_Cvar_Get( "cg_scoreboardWidthScale", "1.0", CVAR_ARCHIVE );
	cg_scoreboardStats =    trap_Cvar_Get( "cg_scoreboardStats", "1", CVAR_ARCHIVE );
}

/*
* CG_ParseValue
*/
int CG_ParseValue( const char **s ) {
	int index;
	char *token;

	token = COM_Parse( s );
	if( !token[0] ) {
		return 0;
	}
	if( token[0] != '%' ) {
		return atoi( token );
	}

	index = atoi( token + 1 );
	if( index < 0 || index >= PS_MAX_STATS ) {
		CG_Error( "Bad stat index: %i", index );
	}

	return cg.predictedPlayerState.stats[index];
}

/*
* CG_DrawNet
*/
void CG_DrawNet( int x, int y, int w, int h, int align, vec4_t color ) {
	int64_t incomingAcknowledged, outgoingSequence;

	if( cgs.demoPlaying ) {
		return;
	}

	trap_NET_GetCurrentState( &incomingAcknowledged, &outgoingSequence, NULL );
	if( outgoingSequence - incomingAcknowledged < CMD_BACKUP - 1 ) {
		return;
	}
	x = CG_HorizontalAlignForWidth( x, align, w );
	y = CG_VerticalAlignForHeight( y, align, h );
	trap_R_DrawStretchPic( x, y, w, h, 0, 0, 1, 1, color, CG_MediaShader( cgs.media.shaderNet ) );
}

/*
* CG_DrawCrosshair
*/
void CG_ScreenCrosshairDamageUpdate( void ) {
	scr_damagetime = cg.monotonicTime;
}

static void CG_FillRect( int x, int y, int w, int h, vec4_t color ) {
	trap_R_DrawStretchPic( x, y, w, h, x, y, x + w, y + h, color, cgs.shaderWhite );
}

static vec4_t crosshair_color = { 1, 1, 1, 1 };
static vec4_t crosshair_damage_color = { 1, 0, 0, 1 };

void CG_DrawCrosshair() {
	float s = 1.0f / 255.0f;

	if( cg_crosshair_color->modified ) {
		cg_crosshair_color->modified = false;
		int rgb = COM_ReadColorRGBString( cg_crosshair_color->string );
		if( rgb != -1 ) {
			Vector4Set( crosshair_color, COLOR_R( rgb ) * s, COLOR_G( rgb ) * s, COLOR_B( rgb ) * s, 255 );
		} else {
			Vector4Set( crosshair_color, 1, 1, 1, 1 );
			trap_Cvar_Set( cg_crosshair_color->name, "255 255 255" );
		}
	}

	if( cg_crosshair_damage_color->modified ) {
		cg_crosshair_damage_color->modified = false;
		int rgb = COM_ReadColorRGBString( cg_crosshair_damage_color->string );
		if( rgb != -1 ) {
			Vector4Set( crosshair_damage_color, COLOR_R( rgb ) * s, COLOR_G( rgb ) * s, COLOR_B( rgb ) * s, 255 );
		} else {
			Vector4Set( crosshair_damage_color, 1, 0, 0, 1 );
			trap_Cvar_Set( cg_crosshair_damage_color->name, "255 255 255" );
		}
	}

	vec4_t inner;
	Vector4Copy( crosshair_color, inner );
	vec4_t border = { 0, 0, 0, 1 };
	if( cg.monotonicTime - scr_damagetime <= 300 )
		Vector4Copy( crosshair_damage_color, inner );

	int w = cgs.vidWidth;
	int h = cgs.vidHeight;
	int size = cg_crosshair_size->integer > 0 ? cg_crosshair_size->integer : 0;

	CG_FillRect( w / 2 - 2, h / 2 - 2 - size, 4, 4 + 2 * size, border );
	CG_FillRect( w / 2 - 2 - size, h / 2 - 2, 4 + 2 * size, 4, border );
	CG_FillRect( w / 2 - 1, h / 2 - 1 - size, 2, 2 + 2 * size, inner );
	CG_FillRect( w / 2 - 1 - size, h / 2 - 1, 2 + 2 * size, 2, inner );
}

void CG_DrawKeyState( int x, int y, int w, int h, int align, const char *key ) {
	int i;
	uint8_t on = 0;
	vec4_t color;

	if( !cg_showPressedKeys->integer ) {
		return;
	}

	if( !key ) {
		return;
	}

	for( i = 0; i < KEYICON_TOTAL; i++ )
		if( !Q_stricmp( key, gs_keyicon_names[i] ) ) {
			break;
		}

	if( i == KEYICON_TOTAL ) {
		return;
	}

	if( cg.predictedPlayerState.plrkeys & ( 1 << i ) ) {
		on = 1;
	}

	Vector4Copy( colorWhite, color );
	if( !on ) {
		color[3] = 0.5f;
	}

	trap_R_DrawStretchPic( x, y, w, h, 0, 0, 1, 1, color, CG_MediaShader( cgs.media.shaderKeyIcon[i] ) );
}

/*
* CG_DrawClock
*/
void CG_DrawClock( int x, int y, int align, struct qfontface_s *font, vec4_t color ) {
	int64_t clocktime, startTime, duration, curtime;
	double seconds;
	int minutes;
	char string[12];

	if( GS_MatchState() > MATCH_STATE_PLAYTIME ) {
		return;
	}

	if( GS_RaceGametype() ) {
		if( cg.predictedPlayerState.stats[STAT_TIME_SELF] != STAT_NOTSET ) {
			clocktime = cg.predictedPlayerState.stats[STAT_TIME_SELF] * 100;
		} else {
			clocktime = 0;
		}
	} else if( GS_MatchClockOverride() ) {
		clocktime = GS_MatchClockOverride();
		if( clocktime < 0 )
			return;
	} else {
		curtime = ( GS_MatchWaiting() || GS_MatchPaused() ) ? cg.frame.serverTime : cg.time;
		duration = GS_MatchDuration();
		startTime = GS_MatchStartTime();

		// count downwards when having a duration
		if( duration ) {
			if( duration + startTime < curtime ) {
				duration = curtime - startTime; // avoid negative results

			}
			clocktime = startTime + duration - curtime;
		} else {
			if( curtime >= startTime ) { // avoid negative results
				clocktime = curtime - startTime;
			} else {
				clocktime = 0;
			}
		}
	}

	seconds = (double)clocktime * 0.001;
	minutes = (int)( seconds / 60 );
	seconds -= minutes * 60;

	// fixme?: this could have its own HUD drawing, I guess.

	if( GS_RaceGametype() ) {
		Q_snprintfz( string, sizeof( string ), "%i:%02i.%i",
					 minutes, ( int )seconds, ( int )( seconds * 10.0 ) % 10 );
	} else if( cg.predictedPlayerState.stats[STAT_NEXT_RESPAWN] ) {
		int respawn = cg.predictedPlayerState.stats[STAT_NEXT_RESPAWN];
		Q_snprintfz( string, sizeof( string ), "%i:%02i R:%02i", minutes, (int)seconds, respawn );
	} else {
		Q_snprintfz( string, sizeof( string ), "%i:%02i", minutes, (int)seconds );
	}

	trap_SCR_DrawString( x, y, align, string, font, color );
}

/*
* CG_ClearPointedNum
*/
void CG_ClearPointedNum( void ) {
	cg.pointedNum = 0;
	cg.pointRemoveTime = 0;
	cg.pointedHealth = 0;
}

/*
* CG_UpdatePointedNum
*/
static void CG_UpdatePointedNum( void ) {
	// disable cases
	if( CG_IsScoreboardShown() || cg.view.thirdperson || cg.view.type != VIEWDEF_PLAYERVIEW || !cg_showPointedPlayer->integer ) {
		CG_ClearPointedNum();
		return;
	}

	if( cg.predictedPlayerState.stats[STAT_POINTED_PLAYER] ) {
		cg.pointedNum = cg.predictedPlayerState.stats[STAT_POINTED_PLAYER];
		cg.pointRemoveTime = cg.time + 150;
		cg.pointedHealth = cg.predictedPlayerState.stats[STAT_POINTED_TEAMPLAYER];
	}

	if( cg.pointRemoveTime <= cg.time ) {
		CG_ClearPointedNum();
	}

	if( cg.pointedNum ) {
		if( cg_entities[cg.pointedNum].current.team != cg.predictedPlayerState.stats[STAT_TEAM] ) {
			CG_ClearPointedNum();
		}
	}
}

/*
* CG_DrawPlayerNames
*/
void CG_DrawPlayerNames( struct qfontface_s *font, vec4_t color ) {
	static vec4_t alphagreen = { 0, 1, 0, 0 }, alphared = { 1, 0, 0, 0 }, alphayellow = { 1, 1, 0, 0 }, alphamagenta = { 1, 0, 1, 1 }, alphagrey = { 0.85, 0.85, 0.85, 1 };
	centity_t *cent;
	vec4_t tmpcolor;
	vec3_t dir, drawOrigin;
	vec2_t coords;
	float dist, fadeFrac;
	trace_t trace;
	int i;

	if( !cg_showPlayerNames->integer && !cg_showPointedPlayer->integer ) {
		return;
	}

	CG_UpdatePointedNum();

	// don't draw when scoreboard is up
	if( CG_IsScoreboardShown() ) {
		return;
	}

	for( i = 0; i < gs.maxclients; i++ ) {
		if( !cgs.clientInfo[i].name[0] || ISVIEWERENTITY( i + 1 ) ) {
			continue;
		}

		cent = &cg_entities[i + 1];
		if( cent->serverFrame != cg.frame.serverFrame ) {
			continue;
		}

		if( cent->current.effects & EF_PLAYER_HIDENAME ) {
			continue;
		}

		// only show the pointed player
		if( !cg_showPlayerNames->integer && ( cent->current.number != cg.pointedNum ) ) {
			continue;
		}

		if( ( cg_showPlayerNames->integer == 2 ) && ( cent->current.team != cg.predictedPlayerState.stats[STAT_TEAM] ) ) {
			continue;
		}

		if( !cent->current.modelindex || !cent->current.solid ||
			cent->current.solid == SOLID_BMODEL || cent->current.team == TEAM_SPECTATOR ) {
			continue;
		}

		// Kill if behind the view
		VectorSubtract( cent->ent.origin, cg.view.origin, dir );
		dist = VectorNormalize( dir ) * cg.view.fracDistFOV;

		if( DotProduct( dir, &cg.view.axis[AXIS_FORWARD] ) < 0 ) {
			continue;
		}

		Vector4Copy( color, tmpcolor );

		if( cent->current.number != cg.pointedNum ) {
			if( dist > cg_showPlayerNames_zfar->value ) {
				continue;
			}

			fadeFrac = Clamp01( ( cg_showPlayerNames_zfar->value - dist ) / ( cg_showPlayerNames_zfar->value * 0.25f ) );

			tmpcolor[3] = cg_showPlayerNames_alpha->value * color[3] * fadeFrac;
		} else {
			fadeFrac = Clamp01( ( cg.pointRemoveTime - cg.time ) / 150.0f );

			tmpcolor[3] = color[3] * fadeFrac;
		}

		if( tmpcolor[3] <= 0.0f ) {
			continue;
		}

		CG_Trace( &trace, cg.view.origin, vec3_origin, vec3_origin, cent->ent.origin, cg.predictedPlayerState.POVnum, MASK_OPAQUE );
		if( trace.fraction < 1.0f && trace.ent != cent->current.number ) {
			continue;
		}

		VectorSet( drawOrigin, cent->ent.origin[0], cent->ent.origin[1], cent->ent.origin[2] + playerbox_stand_maxs[2] + 8 );

		// find the 3d point in 2d screen
		trap_R_TransformVectorToScreen( &cg.view.refdef, drawOrigin, coords );
		if( ( coords[0] < 0 || coords[0] > cgs.vidWidth ) || ( coords[1] < 0 || coords[1] > cgs.vidHeight ) ) {
			continue;
		}

		trap_SCR_DrawString( coords[0], coords[1], ALIGN_CENTER_BOTTOM, cgs.clientInfo[i].name, font, tmpcolor );

		// if not the pointed player we are done
		if( cent->current.number != cg.pointedNum ) {
			continue;
		}

		int pointed_health = cg.pointedHealth / 2;
		if( cg.pointedHealth == 1 )
			pointed_health = 1;

		// pointed player hasn't a health value to be drawn, so skip adding the bars
		if( pointed_health && cg_showPlayerNames_barWidth->integer > 0 ) {
			int x, y;
			int barwidth = trap_SCR_strWidth( "_", font, 0 ) * cg_showPlayerNames_barWidth->integer; // size of 8 characters
			int barheight = trap_SCR_FontHeight( font ) * 0.25; // quarter of a character height
			int barseparator = barheight * 0.333;

			alphagreen[3] = alphared[3] = alphayellow[3] = alphamagenta[3] = alphagrey[3] = tmpcolor[3];

			// soften the alpha of the box color
			tmpcolor[3] *= 0.4f;

			// we have to align first, then draw as left top, cause we want the bar to grow from left to right
			x = CG_HorizontalAlignForWidth( coords[0], ALIGN_CENTER_TOP, barwidth );
			y = CG_VerticalAlignForHeight( coords[1], ALIGN_CENTER_TOP, barheight );

			y += barseparator;

			// draw the background box
			CG_DrawHUDRect( x, y, ALIGN_LEFT_TOP, barwidth, barheight + 2 * barseparator, 100, 100, tmpcolor, NULL );

			y += barseparator;

			if( pointed_health <= 33 ) {
				CG_DrawHUDRect( x, y, ALIGN_LEFT_TOP, barwidth, barheight, pointed_health, 100, alphared, NULL );
			} else if( pointed_health <= 66 ) {
				CG_DrawHUDRect( x, y, ALIGN_LEFT_TOP, barwidth, barheight, pointed_health, 100, alphayellow, NULL );
			} else {
				CG_DrawHUDRect( x, y, ALIGN_LEFT_TOP, barwidth, barheight, pointed_health, 100, alphagreen, NULL );
			}
		}
	}
}

/*
* CG_DrawTeamMates
*/
void CG_DrawTeamMates( void ) {
	centity_t *cent;
	vec3_t dir, drawOrigin;
	vec2_t coords;
	vec4_t color;
	int i;
	int pic_size = 18 * cgs.vidHeight / 600;

	// don't draw when scoreboard is up
	if( CG_IsScoreboardShown() ) {
		return;
	}
	if( cg.predictedPlayerState.stats[STAT_TEAM] < TEAM_ALPHA ) {
		return;
	}

	for( i = 0; i < gs.maxclients; i++ ) {
		trace_t trace;
		cgs_media_handle_t *media;

		if( !cgs.clientInfo[i].name[0] || ISVIEWERENTITY( i + 1 ) ) {
			continue;
		}

		cent = &cg_entities[i + 1];
		if( cent->serverFrame != cg.frame.serverFrame ) {
			continue;
		}

		if( cent->current.team != cg.predictedPlayerState.stats[STAT_TEAM] ) {
			continue;
		}

		VectorSet( drawOrigin, cent->ent.origin[0], cent->ent.origin[1], cent->ent.origin[2] + playerbox_stand_maxs[2] + 16 );
		VectorSubtract( drawOrigin, cg.view.origin, dir );

		// ignore, if not in view
		if( DotProduct( dir, &cg.view.axis[AXIS_FORWARD] ) < 0 ) {
			continue;
		}

		if( !cent->current.modelindex || !cent->current.solid ||
			cent->current.solid == SOLID_BMODEL || cent->current.team == TEAM_SPECTATOR ) {
			continue;
		}

		// players might be SVF_FORCETEAM'ed for teammates, prevent ugly flickering for specs
		if( cg.predictedPlayerState.stats[STAT_REALTEAM] == TEAM_SPECTATOR && !trap_CM_InPVS( cg.view.origin, cent->ent.origin ) ) {
			continue;
		}

		// find the 3d point in 2d screen
		trap_R_TransformVectorToScreen( &cg.view.refdef, drawOrigin, coords );
		if( ( coords[0] < 0 || coords[0] > cgs.vidWidth ) || ( coords[1] < 0 || coords[1] > cgs.vidHeight ) ) {
			continue;
		}

		CG_Trace( &trace, cg.view.origin, vec3_origin, vec3_origin, cent->ent.origin, cg.predictedPlayerState.POVnum, MASK_OPAQUE );
		if( trace.fraction == 1.0f ) {
			continue;
		}

		coords[0] -= pic_size / 2;
		coords[1] -= pic_size / 2;
		coords[0] = Clamp( 0.0f, coords[0], float( cgs.vidWidth - pic_size ) );
		coords[1] = Clamp( 0.0f, coords[1], float( cgs.vidHeight - pic_size ) );

		CG_TeamColor( cg.predictedPlayerState.stats[STAT_TEAM], color );

		if( cent->current.effects & EF_CARRIER ) {
			media = cgs.media.shaderTeamCarrierIndicator;
		} else {
			media = cgs.media.shaderTeamMateIndicator;
		}

		trap_R_DrawStretchPic( coords[0], coords[1], pic_size, pic_size, 0, 0, 1, 1, color, CG_MediaShader( media ) );
	}
}

//=============================================================================

static const char * mini_obituaries[] = { "GG", "RIP", "BYE", "CYA", "L8R", "CHRS", "PLZ", "HAX" };
constexpr int MINI_OBITUARY_DAMAGE = 255;

struct DamageNumber {
	vec3_t origin;
	float drift;
	int64_t t;
	const char * obituary;
	int damage;
};

static DamageNumber damage_numbers[ 16 ];
size_t damage_numbers_head;

void CG_InitDamageNumbers() {
	damage_numbers_head = 0;
	for( DamageNumber & dn : damage_numbers ) {
		dn.damage = 0;
	}
}

void CG_AddDamageNumber( entity_state_t * ent ) {
	if( !cg_damageNumbers->integer && ent->damage != MINI_OBITUARY_DAMAGE )
		return;

	DamageNumber * dn = &damage_numbers[ damage_numbers_head ];
	VectorCopy( ent->origin, dn->origin );
	dn->t = cg.time;
	dn->damage = ent->damage;

	float distance_jitter = 4;
	dn->origin[ 0 ] += random_float11( &cls.rng ) * distance_jitter;
	dn->origin[ 1 ] += random_float11( &cls.rng ) * distance_jitter;
	dn->origin[ 2 ] += 48;
	dn->drift = random_float11( &cls.rng );
	dn->obituary = random_select( &cls.rng, mini_obituaries );

	damage_numbers_head = ( damage_numbers_head + 1 ) % ARRAY_COUNT( damage_numbers );
}

void CG_DrawDamageNumbers() {
	for( const DamageNumber & dn : damage_numbers ) {
		if( dn.damage == 0 )
			continue;

		float lifetime = 750.0f + 5 * dn.damage;
		float frac = ( cg.time - dn.t ) / lifetime;
		if( frac > 1 )
			continue;

		vec3_t o;
		VectorCopy( dn.origin, o );
		o[ 2 ] += frac * 32;

		vec3_t to_target;
		VectorSubtract( o, cg.view.origin, to_target );
		if( DotProduct( &cg.view.axis[ AXIS_FORWARD ], to_target ) <= 0 )
			continue;

		vec2_t coords;
		trap_R_TransformVectorToScreen( &cg.view.refdef, o, coords );
		if( ( coords[ 0 ] < 0 || coords[ 0 ] > cgs.vidWidth ) || ( coords[ 1 ] < 0 || coords[ 1 ] > cgs.vidHeight ) ) {
			continue;
		}

		coords[ 0 ] += dn.drift * frac * 8;

		char buf[ 16 ];
		RGBA8 color;
		float font_size;
		if( dn.damage == MINI_OBITUARY_DAMAGE ) {
			Q_strncpyz( buf, dn.obituary, sizeof( buf ) );
			RGB8 c = CG_TeamColor( TEAM_ENEMY );
			color = RGBA8( c.r, c.g, c.b, 255 );
			font_size = cgs.textSizeSmall;
		}
		else {
			Q_snprintfz( buf, sizeof( buf ), "%d", dn.damage );
			color = rgba8_white;
			font_size = cgs.textSizeTiny;
		}

		RGBA8 border_color = rgba8_black;
		float alpha = 1 - max( 0, frac - 0.75f ) / 0.25f;
		color.a *= alpha;
		border_color.a *= alpha;

		DrawText( cgs.fontMontserrat, font_size, buf, Alignment_CenterMiddle, coords[ 0 ], coords[ 1 ], color, true, border_color );
	}
}

//=============================================================================

struct BombSite {
	vec3_t origin;
	int team;
	char letter;
};

enum BombState {
	BombState_None,
	BombState_Dropped,
	BombState_Placed,
	BombState_Armed,
};

struct Bomb {
	BombState state;
	vec3_t origin;
	int team;
};

static BombSite bomb_sites[ 2 ];
static size_t num_bomb_sites;
static Bomb bomb;

void CG_AddBombHudEntity( centity_t * cent ) {
	if( cent->current.counterNum != 0 ) {
		assert( num_bomb_sites < ARRAY_COUNT( bomb_sites ) );

		BombSite * site = &bomb_sites[ num_bomb_sites ];
		VectorCopy( cent->current.origin, site->origin );
		site->team = cent->current.team;
		site->letter = cent->current.counterNum;

		num_bomb_sites++;
	}
	else {
		if( cent->current.svflags & SVF_ONLYTEAM ) {
			bomb.state = cent->current.radius == BombDown_Dropped ? BombState_Dropped : BombState_Placed;
		}
		else {
			bomb.state = BombState_Armed;
		}

		bomb.team = cent->current.team;
		VectorCopy( cent->current.origin, bomb.origin );
	}
}

void CG_DrawBombHUD() {
	int my_team = cg.predictedPlayerState.stats[STAT_REALTEAM];
	bool show_labels = my_team != TEAM_SPECTATOR && GS_MatchState() == MATCH_STATE_PLAYTIME;

	// TODO: draw arrows when clamped

	if( bomb.state == BombState_None || bomb.state == BombState_Dropped ) {
		for( size_t i = 0; i < num_bomb_sites; i++ ) {
			const BombSite * site = &bomb_sites[ i ];
			vec2_t coords;
			bool clamped = trap_R_TransformVectorToScreenClamped( &cg.view.refdef, site->origin, cgs.fontSystemMediumSize * 2, coords );

			char buf[ 4 ];
			Q_snprintfz( buf, sizeof( buf ), "%c", site->letter );
			DrawText( cgs.fontMontserrat, cgs.textSizeMedium, buf, Alignment_CenterMiddle, coords[ 0 ], coords[ 1 ], rgba8_white, true, rgba8_black );

			if( show_labels && !clamped && bomb.state != BombState_Dropped ) {
				const char * msg = my_team == site->team ? "DEFEND" : "ATTACK";
				coords[ 1 ] -= ( cgs.fontSystemMediumSize * 3 ) / 4;
				DrawText( cgs.fontMontserrat, cgs.textSizeTiny, msg, Alignment_CenterMiddle, coords[ 0 ], coords[ 1 ], rgba8_white, true, rgba8_black );
			}
		}
	}

	if( bomb.state != BombState_None ) {
		vec2_t coords;
		bool clamped = trap_R_TransformVectorToScreenClamped( &cg.view.refdef, bomb.origin, cgs.fontSystemMediumSize * 2, coords );

		cgs_media_handle_t * icon = cgs.media.shaderBombIcon;
		int icon_size = cgs.fontSystemMediumSize;

		if( !clamped ) {
			icon = cgs.media.shaderTeamMateIndicator;
			icon_size = cgs.fontSystemMediumSize / 2;

			if( show_labels ) {
				const char * msg = "RETRIEVE";
				if( bomb.state == BombState_Placed )
					msg = "PLANTING";
				else if( bomb.state == BombState_Armed )
					msg = my_team == bomb.team ? "PROTECT" : "DEFUSE";
				float y = coords[ 1 ] - icon_size - cgs.fontSystemTinySize / 2;
				DrawText( cgs.fontMontserrat, cgs.textSizeTiny, msg, Alignment_CenterMiddle, coords[ 0 ], y, rgba8_white, true, rgba8_black );
			}
		}

		icon_size = ( icon_size * cgs.vidHeight ) / 600;
		trap_R_DrawStretchPic( coords[0] - icon_size / 2, coords[1] - icon_size / 2, icon_size, icon_size, 0, 0, 1, 1, colorWhite, CG_MediaShader( icon ) );
	}
}

void CG_ResetBombHUD() {
	num_bomb_sites = 0;
	bomb.state = BombState_None;
}

//=============================================================================

/*
* CG_DrawRSpeeds
*/
void CG_DrawRSpeeds( int x, int y, int align, struct qfontface_s *font, const vec4_t color ) {
	char msg[1024];

	trap_R_GetSpeedsMessage( msg, sizeof( msg ) );

	if( msg[0] ) {
		int height;
		const char *p, *start, *end;

		height = trap_SCR_FontHeight( font );

		p = start = msg;
		do {
			end = strchr( p, '\n' );
			if( end ) {
				msg[end - start] = '\0';
			}

			trap_SCR_DrawString( x, y, align, p, font, color );
			y += height;

			if( end ) {
				p = end + 1;
			} else {
				break;
			}
		} while( 1 );
	}
}

//=============================================================================

/*
* CG_EscapeKey
*/
void CG_EscapeKey( void ) {
	if( cgs.demoPlaying ) {
		UI_ShowDemoMenu();
		return;
	}

	bool spectator = cg.predictedPlayerState.stats[STAT_REALTEAM] == TEAM_SPECTATOR;
	bool is_ready = false;

	if( GS_MatchState() <= MATCH_STATE_WARMUP && !spectator ) {
		bool ready = ( cg.predictedPlayerState.stats[STAT_LAYOUTS] & STAT_LAYOUT_READY ) != 0;
		is_ready = ready;
	}

	UI_ShowGameMenu( spectator, is_ready );
}

//=============================================================================

/*
* CG_DrawLoading
*/
void CG_DrawLoading( void ) {
	if( !cgs.configStrings[CS_MAPNAME][0] ) {
		return;
	}

	float scale = cgs.vidHeight / 1080.0f;

	const vec4_t color = { 22.0f / 255.0f, 20.0f / 255.0f, 28.0f / 255.0f, 1.0f };
	trap_R_DrawStretchPic( 0, 0, cgs.vidWidth, cgs.vidHeight, 0.0f, 0.0f, 1.0f, 1.0f, color, cgs.shaderWhite );
	trap_R_DrawStretchPic( cgs.vidWidth / 2 - ( int )( 375 * scale ), cgs.vidHeight / 2 - ( int )( 128 * scale ),
						   750 * scale, 256 * scale, 0.0f, 0.0f, 1.0f, 1.0f, colorWhite, trap_R_RegisterPic( UI_SHADER_LOADINGLOGO ) );

	if( cgs.precacheCount && cgs.precacheTotal ) {
		struct shader_s *shader = trap_R_RegisterPic( UI_SHADER_LOADINGBAR );
		int width = 700 * scale;
		int height = 32 * scale;
		float percent = ( ( float )cgs.precacheCount / ( float )cgs.precacheTotal );
		int barWidth = ( width - height ) * bound( 0.0f, percent, 1.0f );
		int x = ( cgs.vidWidth - width ) / 2;
		int y = cgs.vidHeight / 2 + ( int )( 160 * scale );

		trap_R_DrawStretchPic( x, y, height, height, 0.0f, 0.0f, 0.5f, 0.5f, colorWhite, shader );
		trap_R_DrawStretchPic( x + height, y, width - height * 2, height, 0.5f, 0.0f, 0.5f, 0.5f, colorWhite, shader );
		trap_R_DrawStretchPic( x + width - height, y, height, height, 0.5f, 0.0f, 1.0f, 0.5f, colorWhite, shader );
		trap_R_DrawStretchPic( x + height / 2, y, barWidth, height, 0.25f, 0.5f, 0.25f, 1.0f, colorWhite, shader );
		trap_R_DrawStretchPic( x + barWidth, y, height, height, 0.5f, 0.5f, 1.0f, 1.0f, colorWhite, shader );
	}
}

/*
* CG_LoadingItemName
*
* Allow at least one item per frame to be precached.
* Stop accepting new precaches after the timelimit for this frame has been reached.
*/
bool CG_LoadingItemName( const char *str ) {
	if( cgs.precacheCount > cgs.precacheStart && ( trap_Milliseconds() > cgs.precacheStartMsec + 33 ) ) {
		return false;
	}
	cgs.precacheCount++;
	return true;
}

//===============================================================

/*
* CG_AddBlend - wsw
*/
static void CG_AddBlend( float r, float g, float b, float a, float *v_blend ) {
	float a2, a3;

	if( a <= 0 ) {
		return;
	}
	a2 = v_blend[3] + ( 1 - v_blend[3] ) * a; // new total alpha
	a3 = v_blend[3] / a2; // fraction of color from old

	v_blend[0] = v_blend[0] * a3 + r * ( 1 - a3 );
	v_blend[1] = v_blend[1] * a3 + g * ( 1 - a3 );
	v_blend[2] = v_blend[2] * a3 + b * ( 1 - a3 );
	v_blend[3] = a2;
}

/*
* CG_CalcColorBlend - wsw
*/
static void CG_CalcColorBlend( float *color ) {
	float time;
	float uptime;
	float delta;
	int i, contents;

	//clear old values
	for( i = 0; i < 4; i++ )
		color[i] = 0.0f;

	// Add colorblend based on world position
	contents = CG_PointContents( cg.view.origin );
	if( contents & CONTENTS_WATER ) {
		CG_AddBlend( 0.0f, 0.1f, 8.0f, 0.2f, color );
	}
	if( contents & CONTENTS_LAVA ) {
		CG_AddBlend( 1.0f, 0.3f, 0.0f, 0.6f, color );
	}
	if( contents & CONTENTS_SLIME ) {
		CG_AddBlend( 0.0f, 0.1f, 0.05f, 0.6f, color );
	}

	// Add colorblends from sfx
	for( i = 0; i < MAX_COLORBLENDS; i++ ) {
		if( cg.time > cg.colorblends[i].timestamp + cg.colorblends[i].blendtime ) {
			continue;
		}

		time = (float)( ( cg.colorblends[i].timestamp + cg.colorblends[i].blendtime ) - cg.time );
		uptime = ( (float)cg.colorblends[i].blendtime ) * 0.5f;
		delta = 1.0f - ( fabs( time - uptime ) / uptime );
		if( delta <= 0.0f ) {
			continue;
		}
		if( delta > 1.0f ) {
			delta = 1.0f;
		}

		CG_AddBlend( cg.colorblends[i].blend[0],
					 cg.colorblends[i].blend[1],
					 cg.colorblends[i].blend[2],
					 cg.colorblends[i].blend[3] * delta,
					 color );
	}
}

/*
* CG_SCRDrawViewBlend
*/
static void CG_SCRDrawViewBlend( void ) {
	vec4_t colorblend;

	if( !cg_showViewBlends->integer ) {
		return;
	}

	CG_CalcColorBlend( colorblend );
	if( colorblend[3] < 0.01f ) {
		return;
	}

	trap_R_DrawStretchPic( 0, 0, cgs.vidWidth, cgs.vidHeight, 0, 0, 1, 1, colorblend, cgs.shaderWhite );
}


//=======================================================

/*
* CG_DrawHUD
*/
void CG_DrawHUD() {
	// if changed from or to spec, reload the HUD
	if( cg.specStateChanged ) {
		cg_specHUD->modified = cg_clientHUD->modified = true;
		cg.specStateChanged = false;
	}

	cvar_t *hud = ISREALSPECTATOR() ? cg_specHUD : cg_clientHUD;
	if( hud->modified ) {
		CG_LoadStatusBar();
		hud->modified = false;
	}

	CG_ExecuteLayoutProgram( cg.statusBar );
}

/*
* CG_Draw2DView
*/
void CG_Draw2DView( void ) {
	MICROPROFILE_SCOPEI( "Main", "CG_Draw2DView", 0xffffffff );

	if( !cg.view.draw2D ) {
		return;
	}

	CG_SCRDrawViewBlend();

	CG_DrawHUD();

	scr_centertime_off -= cg.frameTime;
	UI_ShowScoreboard( CG_IsScoreboardShown() );

	if( scr_centertime_off > 0 ) {
		CG_DrawCenterString();
	}

	CG_DrawRSpeeds( cgs.vidWidth, cgs.vidHeight / 2 + 8 * cgs.vidHeight / 600,
					ALIGN_RIGHT_TOP, cgs.fontSystemSmall, colorWhite );
}

/*
* CG_Draw2D
*/
void CG_Draw2D( void ) {
	if( !cg_draw2D->integer ) {
		return;
	}

	CG_Draw2DView();
	CG_DrawDemocam2D();
}
