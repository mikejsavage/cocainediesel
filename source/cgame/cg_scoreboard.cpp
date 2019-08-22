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

// cg_scoreboard.c -- scoreboard layouts for gametypes

#include "cg_local.h"

#define SCB_BACKGROUND_ALPHA 0.25f

#define SCB_TEAMNAME_PIXELWIDTH ( (int)( 260 * cg_scoreboardWidthScale->value ) * cgs.vidHeight / 600 )
#define SCB_SMALLFIELD_PIXELWIDTH ( (int)( 40 * cg_scoreboardWidthScale->value ) * cgs.vidHeight / 600 )
#define SCB_TINYFIELD_PIXELWIDTH ( (int)( 26 * cg_scoreboardWidthScale->value ) * cgs.vidHeight / 600 )

#define SCB_SCORENUMBER_SIZE ( 48 * cgs.vidHeight / 600 )
#define SCB_CENTERMARGIN ( 16 * cgs.vidHeight / 600 )

typedef struct {
	int playerNum;
	int ping;
} scr_spectator_t;

/*
* CG_DrawAlignPic
*/
static void CG_DrawAlignPic( int x, int y, int width, int height, int align, const vec4_t color, struct shader_s *shader ) {
	x = CG_HorizontalAlignForWidth( x, align, width );
	y = CG_VerticalAlignForHeight( y, align, height );

	trap_R_DrawStretchPic( x, y, width, height, 0, 0, 1, 1, color, shader );
}

/*
* CG_PingColor
*/
static void CG_PingColor( int ping, vec4_t color ) {
	if( ping < 70 ) {
		Vector4Copy( colorGreen, color );
	} else if( ping < 100 ) {
		Vector4Copy( colorYellow, color );
	} else if( ping < 150 ) {
		Vector4Copy( colorOrange, color );
	} else {
		Vector4Copy( colorRed, color );
	}
}

// ====================================================
// player scoreboards
// ====================================================

static char scoreboardString[MAX_STRING_CHARS];

/*
* SCR_ParseToken
* Parses the next token and checks that it is not empty or a new command.
*/
static bool SCR_ParseToken( const char **ptrptr, const char **tokenptr ) {
	const char *oldptr, *token;

	oldptr = *ptrptr;
	*tokenptr = COM_ParseExt( ptrptr, true );
	token = *tokenptr;

	if( !token[0] ) { // empty
		return false;
	}

	if( token[0] == '&' ) { // new command
		*ptrptr = oldptr;
		return false;
	}

	return true;
}

static bool SCR_ParseSpectator( scr_spectator_t *result, const char **ptrptr, bool havePing ) {
	const char *token;

	if( !SCR_ParseToken( ptrptr, &token ) ) {
		return false;
	}

	result->playerNum = atoi( token );

	if( result->playerNum < 0 || result->playerNum >= gs.maxclients ) {
		return false;
	}

	if( havePing ) {
		if( !SCR_ParseToken( ptrptr, &token ) ) {
			return false;
		}

		result->ping = atoi( token );
	}

	return true;
}

static void SCR_SpectatorString( char *string, size_t size, scr_spectator_t spec, bool havePing ) {
	if( havePing ) {
		if( spec.ping < 0 ) {
			Q_snprintfz( string, size, "%s%s ...", cgs.clientInfo[spec.playerNum].name, S_COLOR_WHITE );
		} else {
			Q_snprintfz( string, size, "%s%s %i", cgs.clientInfo[spec.playerNum].name, S_COLOR_WHITE, spec.ping );
		}
	} else {
		Q_snprintfz( string, size, "%s%s", cgs.clientInfo[spec.playerNum].name, S_COLOR_WHITE );
	}
}

/*
* SCR_DrawChallengers
*/
static int SCR_DrawChallengers( const char **ptrptr, int x, int y, int panelWidth, struct qfontface_s *font, int pass ) {
	char string[MAX_STRING_CHARS];
	scr_spectator_t spec;
	int yoffset = 0, xoffset = 0;
	int height;

	assert( ptrptr && *ptrptr );

	height = trap_SCR_FontHeight( font );

	// draw title
	yoffset = height;
	if( pass ) {
		trap_SCR_DrawString( x + xoffset, y + yoffset, ALIGN_CENTER_TOP, "Challengers", font, colorCyan );
	}
	yoffset += height;

	// draw challengers
	while( *ptrptr ) {
		if( !SCR_ParseSpectator( &spec, ptrptr, true ) ) {
			break;
		}

		if( pass ) {
			SCR_SpectatorString( string, sizeof( string ), spec, true );
			trap_SCR_DrawString( x + xoffset, y + yoffset, ALIGN_CENTER_TOP, string, font, colorWhite );
		}
		yoffset += height;
	}

	yoffset += height;
	return yoffset;
}

static int SCR_CountFromCenter( int index, int items ) {
	int result = ( index + 1 ) >> 1; // distance from center
	if( items % 2 == index % 2 ) {
		result *= -1; // swap sides half of the cases
	}
	return result + ( ( items - 1 ) >> 1 ); // relative to center
}

static int SCR_TableRows( int columns, int count ) {
	return count / columns + ( count % columns ? 1 : 0 );
}

static bool SCR_NiceSpecConfig( int rows, int columns, int count ) {
	return rows > 2 || columns == 1 || ( rows == columns && count % columns == 0 );
}

/*
* SCR_DrawSpectators
*/
static int SCR_DrawSpectators( const char **ptrptr, int x, int y, int panelWidth, struct qfontface_s *font, bool havePing, const char *title, const vec4_t titleColor, int pass ) {
	const char *backup;
	scr_spectator_t spec;
	char string[MAX_STRING_CHARS];
	int yoffset = 0, xoffset = 0;
	int fullwidth, height;
	int columns, colwidth, width, maxwidth = 0;
	int count = 0;
	bool titleDrawn = false;

	assert( ptrptr && *ptrptr );

	fullwidth = panelWidth * 2.0;
	if( fullwidth > cgs.vidWidth * 0.75 ) {
		fullwidth = cgs.vidWidth * 0.75;
	}

	height = trap_SCR_FontHeight( font );
	yoffset = height;

	backup = *ptrptr;

	// determine number of columns available
	while( *ptrptr ) {
		if( !SCR_ParseSpectator( &spec, ptrptr, havePing ) ) {
			break;
		}

		width = trap_SCR_strWidth( cgs.clientInfo[spec.playerNum].name, font, 0 ) + trap_SCR_strWidth( "M", font, 0 ) * ( havePing ? 6 : 2 );
		if( width > maxwidth ) {
			maxwidth = width;
		}
		count++;
	}
	if( !maxwidth ) {
		return yoffset;
	}
	columns = fullwidth / maxwidth;
	if( columns == 0 ) {
		columns = 1;
	} else if( count < columns ) {
		columns = count;
	} else if( columns < 3 && count >= 3 ) {
		columns = 3; // force 3 columns if less than 3 fit

	}
	int rows = SCR_TableRows( columns, count );
	while( !SCR_NiceSpecConfig( rows, columns, count ) ) { // avoid ugly configurations
		columns--;
		rows = SCR_TableRows( columns, count );
	}

	// optimize number of columns
	int best = columns;
	while( columns > 1 && SCR_TableRows( columns - 1, count ) == rows ) {
		columns--;
		if( SCR_NiceSpecConfig( rows, columns, count ) ) {
			best = columns;
		}
	}
	columns = best;

	// determine column width
	colwidth = fullwidth / columns;

	// use smaller columns if possible, and adjust the width of the whole area
	if( maxwidth < colwidth ) {
		colwidth = maxwidth;
	}
	fullwidth = colwidth * columns;

	int total = count;
	count = 0;

	// draw the spectators
	*ptrptr = backup;
	while( *ptrptr ) {
		if( !SCR_ParseSpectator( &spec, ptrptr, havePing ) ) {
			break;
		}

		// draw title if there are any spectators
		if( !titleDrawn ) {
			titleDrawn = true;
			if( pass ) {
				trap_SCR_DrawString( x, y + yoffset, ALIGN_CENTER_TOP, title, font, titleColor );
			}
			yoffset += height;
		}

		SCR_SpectatorString( string, sizeof( string ), spec, havePing );
		width = trap_SCR_strWidth( string, font, 0 );
		if( width > colwidth ) {
			width = colwidth;
		}
		xoffset = -fullwidth / 2 + SCR_CountFromCenter( count % columns, columns ) * colwidth +
				  CG_HorizontalAlignForWidth( colwidth / 2, ALIGN_CENTER_TOP, width );

		if( pass ) {
			trap_SCR_DrawClampString( x + xoffset, y + yoffset, string, x + xoffset, y + yoffset,
									  x + xoffset + width, y + yoffset + height, font, colorWhite );
		}

		count++;
		if( count % columns == 0 ) {
			yoffset += height;
			if( total - count < columns && total > count ) {
				// center the last row properly
				columns = total - count;
				count = 0;

				// redetermine column width
				colwidth = fullwidth / columns;

				// use smaller columns if possible, and adjust the width of the whole area
				if( maxwidth < colwidth ) {
					colwidth = maxwidth;
				}
				fullwidth = colwidth * columns;
			}
		}
	}

	return yoffset;
}

/*
* SCR_GetNextColumnLayout
*/
static const char *SCR_GetNextColumnLayout( const char **ptrlay, const char **ptrtitle, char *type, int *width, struct qfontface_s *font ) {
	static const char *empty = "";
	const char *token;

	assert( ptrlay && *ptrlay );

	// get the token type from the layout
	token = COM_ParseExt( ptrlay, true );
	if( !token[0] ) {
		return NULL;
	}

	if( token[0] != '%' ) {
		CG_Error( "SCR_GetNextColumnLayout: Invalid player tab layout (expecting token type. found '%s')\n", token );
	}

	if( type ) {
		*type = token[1];
	}

	// get the column width from the layout
	token = COM_ParseExt( ptrlay, true );
	if( !token[0] || token[0] == '%' ) {
		CG_Error( "SCR_GetNextColumnLayout: Invalid player tab layout (expecting token width. found '%s')\n", token );
	}

	if( width ) {
		float widthScale = cg_scoreboardWidthScale->value;
		bool relative = true;
		if( token[0] == 'l' ) { // line heights
			widthScale *= trap_SCR_FontHeight( font );
			relative = false;
			token++;
		}
		*width = (int)( atof( token ) * widthScale );
		if( relative ) {
			*width = *width * cgs.vidHeight / 600;
		}

		if( *width < 0 ) {
			*width = 0;
		}
	}

	if( ptrtitle && *ptrtitle ) {
		// get the column title token from the layout
		token = COM_ParseExt( ptrtitle, true );
		if( !token[0] ) {
			CG_Error( "SCR_GetNextColumnLayout: Invalid player tab layout (expecting token tittle. found '%s')\n", token );
		}
	} else {
		token = empty;
	}

	return token;
}

/**
 * Checks if the scoreboard column of the specific type needs to be skipped.
 *
 * @param type the column type
 * @return whether the column needs to be skipped
 */
static bool SCR_SkipColumn( char type ) {
	switch( type ) {
		case 'r':
			return GS_MatchState() != MATCH_STATE_WARMUP;
	}

	return false;
}

/*
* SCR_DrawTeamTab
*/
static int SCR_DrawTeamTab( const char **ptrptr, int *curteam, int x, int y, int panelWidth, struct qfontface_s *font, struct qfontface_s *titleFont, int pass ) {
	const char *token;
	const char *layout, *titles;
	char type;
	int yoffset = 0, xoffset = 0;
	int dir = 0, align, width, height;
	vec4_t teamcolor = { 0.0f, 0.0f, 0.0f, 1.0f };

	// team tab is always the same. Sets the current team and draws its score

	if( !( *ptrptr ) || !( *ptrptr[0] ) || *ptrptr[0] == '&' ) {
		return yoffset;
	}

	int team = CG_ParseValue( ptrptr );
	if( team < TEAM_PLAYERS || team > TEAM_BETA ) {
		CG_Error( "SCR_ParseTeamTab: Invalid team value\n" );
	}

	*curteam = team;

	if( *ptrptr[0] == '&' ) {
		return yoffset;
	}

	int team_score = CG_ParseValue( ptrptr );
	( void ) team_score;

	if( *ptrptr[0] == '&' ) {
		return yoffset;
	}

	if( ( team == TEAM_ALPHA ) || ( team == TEAM_BETA ) ) {
		CG_TeamColor( team, teamcolor );
	}
	teamcolor[3] = SCB_BACKGROUND_ALPHA; // make transparent

	if( GS_TeamBasedGametype() ) { // we only draw the team tabs in team based gametypes
		dir = ( team == TEAM_ALPHA ) ? -1 : 1;
		align = ( team == TEAM_ALPHA ) ? ALIGN_RIGHT_TOP : ALIGN_LEFT_TOP;

		// draw the tab

		xoffset = ( SCB_CENTERMARGIN * dir );

		width = ( cgs.vidWidth * 0.5 ) - SCB_CENTERMARGIN;
		height = trap_SCR_FontHeight( titleFont ) + 2;

		if( !pass ) {
			CG_DrawAlignPic( x + xoffset, y + yoffset + SCB_SCORENUMBER_SIZE - height,
							 width, height, align, teamcolor, cgs.shaderWhite );
		}

		if( pass ) {
			trap_SCR_DrawStringWidth( x + ( ( SCB_TINYFIELD_PIXELWIDTH + ( 16 * cgs.vidHeight / 600 ) ) * dir ),
									  y + yoffset + SCB_SCORENUMBER_SIZE - ( trap_SCR_FontHeight( titleFont ) + 1 ),
									  align, va( "%i", team_score ), SCB_SCORENUMBER_SIZE, titleFont, colorWhite );

			xoffset += ( ( SCB_SCORENUMBER_SIZE * strlen( va( "%i", team_score ) ) + ( 16 * cgs.vidHeight / 600 ) ) * dir );

			trap_SCR_DrawStringWidth( x + xoffset + ( ( SCB_TINYFIELD_PIXELWIDTH + ( 16 * cgs.vidHeight / 600 ) ) * dir ),
									  y + yoffset + SCB_SCORENUMBER_SIZE - ( trap_SCR_FontHeight( titleFont ) + 1 ),
									  align, GS_TeamName( team ), SCB_TEAMNAME_PIXELWIDTH, titleFont, colorWhite );
		}

		yoffset += SCB_SCORENUMBER_SIZE;
	} else {
		dir = 0;
		align = ALIGN_CENTER_TOP;
	}

	// draw the player tab column titles
	layout = cgs.configStrings[CS_SCB_PLAYERTAB_LAYOUT];
	titles = cgs.configStrings[CS_SCB_PLAYERTAB_TITLES];

	height = trap_SCR_FontHeight( font );

	// start from the center again
	xoffset = CG_HorizontalAlignForWidth( 0, align, panelWidth );
	xoffset += ( SCB_CENTERMARGIN * dir );

	while( ( token = SCR_GetNextColumnLayout( &layout, &titles, &type, &width, font ) ) != NULL ) {
		if( SCR_SkipColumn( type ) ) {
			continue;
		}

		if( width ) {
			if( pass ) {
				trap_SCR_DrawClampString( x + xoffset, y + yoffset, token,
										  x + xoffset, y + yoffset, x + xoffset + width, y + yoffset + height, font, colorWhite );
			}
			xoffset += width;
		}
	}

	yoffset += trap_SCR_FontHeight( font );

	return yoffset;
}

typedef struct
{
	struct shader_s *image;
	int x, y;
	float alpha;
} scr_playericon_t;

static scr_playericon_t scr_playericons[128];
static unsigned scr_numplayericons;

/*
* SCR_ComparePlayerIcons
*/
static int SCR_ComparePlayerIcons( const scr_playericon_t *first, const scr_playericon_t *second ) {
	return ( ( void * )( first->image ) > ( void * )( second->image ) ) -
		   ( ( void * )( first->image ) < ( void * )( second->image ) );
}

/*
* SCR_DrawPlayerIcons
*/
static void SCR_DrawPlayerIcons( struct qfontface_s *font ) {
	if( !scr_numplayericons ) {
		return;
	}

	qsort( scr_playericons, scr_numplayericons, sizeof( scr_playericons[0] ),
		   ( int ( * )( const void *, const void * ) )SCR_ComparePlayerIcons );

	int height = trap_SCR_FontHeight( font );
	vec4_t color;
	Vector4Copy( colorWhite, color );

	for( unsigned i = 0; i < scr_numplayericons; i++ ) {
		scr_playericon_t &icon = scr_playericons[i];
		color[3] = icon.alpha;
		trap_R_DrawStretchPic( icon.x, icon.y, height, height, 0, 0, 1, 1, color, icon.image );
	}

	scr_numplayericons = 0;
}

/*
* SCR_AddPlayerIcon
*/
static void SCR_AddPlayerIcon( struct shader_s *image, int x, int y, float alpha, struct qfontface_s *font ) {
	if( !image ) {
		return;
	}

	scr_playericon_t &icon = scr_playericons[scr_numplayericons++];

	icon.image = image;
	icon.x = x;
	icon.y = y;
	icon.alpha = alpha;

	if( scr_numplayericons >= ARRAY_COUNT( scr_playericons ) ) {
		SCR_DrawPlayerIcons( font );
	}
}

/*
* SCR_DrawPlayerTab
*/
static int SCR_DrawPlayerTab( const char **ptrptr, int team, int x, int y, int panelWidth, struct qfontface_s *font, int pass ) {
	int dir, align, i, columncount;
	char type, string[MAX_STRING_CHARS];
	const char *token, *layout;
	int height, width, xoffset, yoffset;
	vec4_t teamcolor = { 0.0f, 0.0f, 0.0f, 1.0f }, color;
	int iconnum;
	struct shader_s *icon;
	bool highlight = false, trans = false;

	if( GS_TeamBasedGametype() ) {
		dir = ( team == TEAM_ALPHA ) ? -1 : 1;
		align = ( team == TEAM_ALPHA ) ? ALIGN_RIGHT_TOP : ALIGN_LEFT_TOP;
	} else {
		dir = 0;
		align = ALIGN_CENTER_TOP;
	}

	xoffset = 0;
	yoffset = 0;

	height = trap_SCR_FontHeight( font );

	// start from the center again
	xoffset = CG_HorizontalAlignForWidth( 0, align, panelWidth );
	xoffset += ( SCB_CENTERMARGIN * dir );

	// draw the background
	columncount = 0;
	if( ( team == TEAM_ALPHA ) || ( team == TEAM_BETA ) ) {
		CG_TeamColor( team, teamcolor );
	}

	// draw the player tab column titles
	layout = cgs.configStrings[CS_SCB_PLAYERTAB_LAYOUT];

	while( SCR_GetNextColumnLayout( &layout, NULL, &type, &width, font ) != NULL ) {
		// grab the actual scoreboard data
		if( !SCR_ParseToken( ptrptr, &token ) ) {
			break;
		}

		if( SCR_SkipColumn( type ) ) {
			continue;
		}

		Vector4Copy( colorWhite, color ); // reset to white after each column
		icon = NULL;
		string[0] = 0;

		// interpret the data based on the type defined in the layout
		switch( type ) {
			default:
				CG_Error( "SCR_DrawPlayerTab: Invalid player tab layout\n" );
				break;

			case 's': // is a string
			{
				Q_strncpyz( string, token, sizeof( string ) );
			}
			break;

			case 'n': // is a player name indicated by player number
				i = atoi( token );

				if( i < 0 ) { // negative numbers toggle transparency on
					trans = true;
					i = -1 - i;
				}

				if( i < 0 || i >= gs.maxclients ) {
					Q_strncpyz( string, "invalid", sizeof( string ) );
				} else {
					Q_strncpyz( string, cgs.clientInfo[i].name, sizeof( string ) );
				}

				if( ISVIEWERENTITY( i + 1 ) ) { // highlight if it's our own player
					highlight = true;
				}

				break;
			case 'i': // is a integer (negatives are colored in red)
				i = atoi( token );
				Q_snprintfz( string, sizeof( string ), "%i", i );
				VectorCopy( i >= 0 ? colorWhite : colorRed, color );
				break;

			case 'f': // is a float
				Q_snprintfz( string, sizeof( string ), "%.2f", atof( token ) );
				break;

			case 'l': // p is an integer colored in latency style
				i = atoi( token );
				Q_snprintfz( string, sizeof( string ), "%i", i );
				CG_PingColor( i, color );
				break;

			case 'b': // is a Y/N boolean
				i = atoi( token );
				Q_snprintfz( string, sizeof( string ), "%s", i != 0 ? "Yes" : "No" );
				VectorCopy( i ? colorGreen : colorRed, color );
				break;

			case 'p': // is a picture. It uses height for width to get a square
				iconnum = atoi( token );
				if( ( iconnum > 0 ) && ( iconnum < MAX_IMAGES ) ) {
					icon = cgs.imagePrecache[iconnum];
				}
				break;

			case 't': // is a race time. Convert time into MM:SS:mm
			{
				unsigned int milli, min, sec;

				milli = (unsigned int)( atoi( token ) );
				if( !milli ) {
					Q_snprintfz( string, sizeof( string ), "%s", "no time" );
				} else {
					min = milli / 60000;
					milli -= min * 60000;
					sec = milli / 1000;
					milli -= sec * 1000;
					Q_snprintfz( string, sizeof( string ), "%02i:%02i.%03i", min, sec, milli );
				}
			}
			break;

			case 'r': // is a ready state tick that is hidden when not in warmup
				if( atoi( token ) ) {
					icon = CG_MediaShader( cgs.media.shaderTick );
				}
				break;
		}

		if( !width ) {
			continue;
		}

		// draw the column background
		teamcolor[3] = SCB_BACKGROUND_ALPHA;
		if( columncount & 1 ) {
			teamcolor[3] -= 0.15;
		}

		if( highlight ) {
			teamcolor[3] += 0.3;
		}

		if( trans ) {
			color[3] = 0.3;
		}

		if( !pass ) {
			trap_R_DrawStretchPic( x + xoffset, y + yoffset, width, height, 0, 0, 1, 1, teamcolor, cgs.shaderWhite );

			if( icon ) {
				SCR_AddPlayerIcon( icon, x + xoffset, y + yoffset, color[3], font );
			}
		}

		// draw the column value
		if( pass && string[0] ) {
			trap_SCR_DrawClampString( x + xoffset, y + yoffset, string,
									  x + xoffset, y + yoffset,
									  x + xoffset + width, y + yoffset + height, font, color );
		}

		columncount++;

		xoffset += width;
	}

	yoffset += height;
	return yoffset;
}

/*
* CG_ScoreboardFont
*/
struct qfontface_s *CG_ScoreboardFont( const char * family, int size ) {
	struct qfontface_s *font = trap_SCR_RegisterFont( family, QFONT_STYLE_NONE, ceilf( size * ( (float)cgs.vidHeight / 600.0f ) ) );
	if( !font ) {
		font = size > SCOREBOARD_FONT_SIZE ? cgs.fontSystemBig : cgs.fontSystemSmall;
	}

	return font;
}

/*
* CG_DrawScoreboard
*/
void CG_DrawScoreboard( void ) {
	int pass;
	const char *ptr, *token, *layout;
	char type;
	int team = TEAM_PLAYERS;
	int xpos;
	int ypos, yoffset, maxyoffset;
	struct qfontface_s *font;
	struct qfontface_s *titlefont;
	int width, panelWidth;
	vec4_t whiteTransparent = { 1.0f, 1.0f, 1.0f, 0.5f };

	// no layout defined
	if( !cgs.configStrings[CS_SCB_PLAYERTAB_LAYOUT][0] ) {
		return;
	}

	if( scoreboardString[0] != '&' ) { // nothing to draw
		return;
	}

	font = CG_ScoreboardFont( SYSTEM_FONT_FAMILY, SCOREBOARD_FONT_SIZE );
	titlefont = CG_ScoreboardFont( SYSTEM_FONT_FAMILY, SCOREBOARD_TITLE_FONT_SIZE );

	xpos = (int)( cgs.vidWidth * 0.5 );
	ypos = (int)( cgs.vidHeight * 0.3 );

	// draw title
	trap_SCR_DrawStringWidth( xpos, ypos, ALIGN_CENTER_TOP, cgs.configStrings[CS_HOSTNAME], cgs.vidWidth * 0.75, font, whiteTransparent );
	ypos += trap_SCR_FontHeight( font );

	// calculate the panel width from the layout
	panelWidth = 0;
	layout = cgs.configStrings[CS_SCB_PLAYERTAB_LAYOUT];
	while( SCR_GetNextColumnLayout( &layout, NULL, &type, &width, font ) != NULL ) {
		if( !SCR_SkipColumn( type ) ) {
			panelWidth += width;
		}
	}

	// parse and draw the scoreboard message
	for( pass = 0; pass < 2; pass++ ) {
		yoffset = 0;
		maxyoffset = 0;
		scr_numplayericons = 0;
		ptr = scoreboardString;
		while( ptr ) {
			token = COM_ParseExt( &ptr, true );
			if( token[0] != '&' ) {
				break;
			}

			if( !Q_stricmp( token, "&t" ) ) { // team tab
				yoffset = 0;
				yoffset += SCR_DrawTeamTab( &ptr, &team, xpos, ypos + yoffset, panelWidth, font, titlefont, pass );
			} else if( !Q_stricmp( token, "&p" ) ) {   // player tab
				yoffset += SCR_DrawPlayerTab( &ptr, team, xpos, ypos + yoffset, panelWidth, font, pass );
			} else if( !Q_stricmp( token, "&w" ) ) {   // list of challengers
				if( yoffset < maxyoffset ) {
					yoffset = maxyoffset;
				}

				maxyoffset += SCR_DrawChallengers( &ptr, xpos, ypos + yoffset, panelWidth, font, pass );
			} else if( !Q_stricmp( token, "&s" ) ) {   // list of spectators
				if( yoffset < maxyoffset ) {
					yoffset = maxyoffset;
				}

				maxyoffset += SCR_DrawSpectators( &ptr, xpos, ypos + yoffset, panelWidth, font, true, "Spectators", colorYellow, pass );
			}

			if( yoffset > maxyoffset ) {
				maxyoffset = yoffset;
			}
		}
		if( !pass ) {
			SCR_DrawPlayerIcons( font );
		}
	}
}



/*
* SCR_UpdateScoreboardMessage

void SCR_UpdateScoreboardMessage( const char *string ) {
	Q_strncpyz( scoreboardString, string, sizeof( scoreboardString ) );
}*/

/*
* CG_ScoresOn_f
*/
void CG_ScoresOn_f( void ) {
	if( cgs.demoPlaying || cg.frame.multipov ) {
		cg.showScoreboard = true;
	} else {
		trap_Cmd_ExecuteText( EXEC_NOW, "svscore 1" );
	}
}

/*
* CG_ScoresOff_f
*/
void CG_ScoresOff_f( void ) {
	if( cgs.demoPlaying || cg.frame.multipov ) {
		cg.showScoreboard = false;
	} else {
		trap_Cmd_ExecuteText( EXEC_NOW, "svscore 0" );
	}
}