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
// cl_screen.c -- master for refresh, status bar, console, chat, notify, etc

/*

full screen console
put up loading plaque
blanked background with loading plaque
blanked background with menu
full screen image for quit and victory

end of unit intermissions

*/

#include "client.h"
#include "ftlib/ftlib_public.h"

static bool scr_initialized;    // ready to draw

static int scr_draw_loading;

static cvar_t *scr_netgraph;
static cvar_t *scr_timegraph;
static cvar_t *scr_debuggraph;
static cvar_t *scr_graphheight;
static cvar_t *scr_graphscale;
static cvar_t *scr_graphshift;

//
//	Variable width (proportional) fonts
//

//===============================================================================
//FONT LOADING
//===============================================================================

/*
* SCR_RegisterFont
*/
qfontface_t *SCR_RegisterFont( const char *family, int style, unsigned int size ) {
	return FTLIB_RegisterFont( family, SYSTEM_FONT_FAMILY_FALLBACK, style, size );
}

/*
* SCR_RegisterSpecialFont
*/
qfontface_t *SCR_RegisterSpecialFont( const char *family, int style, unsigned int size ) {
	return FTLIB_RegisterFont( family, NULL, style, size );
}

/*
* SCR_RegisterConsoleFont
*/
static void SCR_RegisterConsoleFont( void ) {
	cls.consoleFont = SCR_RegisterFont( SYSTEM_FONT_FAMILY, SYSTEM_FONT_STYLE, SYSTEM_FONT_CONSOLE_SIZE );
	if( !cls.consoleFont ) {
		Com_Error( ERR_FATAL, "Couldn't load default font \"" SYSTEM_FONT_FAMILY "\"" );
	}
}


/*
* SCR_InitFonts
*/
static void SCR_InitFonts( void ) {
	SCR_RegisterConsoleFont();
}

/*
* SCR_ShutdownFonts
*/
static void SCR_ShutdownFonts( void ) {
	cls.consoleFont = NULL;
}

//===============================================================================
//STRINGS HELPERS
//===============================================================================


static int SCR_HorizontalAlignForString( const int x, int align, int width ) {
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

static int SCR_VerticalAlignForString( const int y, int align, int height ) {
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

size_t SCR_FontSize( qfontface_t *font ) {
	return FTLIB_FontSize( font );
}

size_t SCR_FontHeight( qfontface_t *font ) {
	return FTLIB_FontHeight( font );
}

size_t SCR_strWidth( const char *str, qfontface_t *font, size_t maxlen ) {
	return FTLIB_StringWidth( str, font, maxlen );
}

size_t SCR_StrlenForWidth( const char *str, qfontface_t *font, size_t maxwidth ) {
	return FTLIB_StrlenForWidth( str, font, maxwidth );
}

int SCR_FontUnderline( qfontface_t *font, int *thickness ) {
	return FTLIB_FontUnderline( font, thickness );
}

fdrawchar_t SCR_SetDrawCharIntercept( fdrawchar_t intercept ) {
	return FTLIB_SetDrawCharIntercept( intercept );
}

//===============================================================================
//STRINGS DRAWING
//===============================================================================

void SCR_DrawRawChar( int x, int y, wchar_t num, qfontface_t *font, const vec4_t color ) {
	FTLIB_DrawRawChar( x, y, num, font, color );
}

void SCR_DrawClampChar( int x, int y, wchar_t num, int xmin, int ymin, int xmax, int ymax, qfontface_t *font, const vec4_t color ) {
	FTLIB_DrawClampChar( x, y, num, xmin, ymin, xmax, ymax, font, color );
}

void SCR_DrawClampString( int x, int y, const char *str, int xmin, int ymin, int xmax, int ymax, qfontface_t *font, const vec4_t color ) {
	FTLIB_DrawClampString( x, y, str, xmin, ymin, xmax, ymax, font, color );
}

int SCR_DrawMultilineString( int x, int y, const char *str, int halign, int maxwidth, int maxlines, qfontface_t *font, const vec4_t color ) {
	return FTLIB_DrawMultilineString( x, y, str, halign, maxwidth, maxlines, font, color );
}

/*
* SCR_DrawString
*/
int SCR_DrawString( int x, int y, int align, const char *str, qfontface_t *font, const vec4_t color ) {
	int width;
	int fontHeight;

	if( !str ) {
		return 0;
	}

	if( !font ) {
		font = cls.consoleFont;
	}
	fontHeight = FTLIB_FontHeight( font );

	if( ( align % 3 ) != 0 ) { // not left - don't precalculate the width if not needed
		x = SCR_HorizontalAlignForString( x, align, FTLIB_StringWidth( str, font, 0 ) );
	}
	y = SCR_VerticalAlignForString( y, align, fontHeight );

	FTLIB_DrawRawString( x, y, str, 0, &width, font, color );

	return width;
}

/*
* SCR_DrawStringWidth
*
* ClampS to width in pixels. Returns drawn len
*/
size_t SCR_DrawStringWidth( int x, int y, int align, const char *str, size_t maxwidth, qfontface_t *font, const vec4_t color ) {
	size_t width;
	int fontHeight;

	if( !str ) {
		return 0;
	}

	if( !font ) {
		font = cls.consoleFont;
	}
	fontHeight = FTLIB_FontHeight( font );

	width = FTLIB_StringWidth( str, font, 0 );
	if( width ) {
		if( maxwidth && width > maxwidth ) {
			width = maxwidth;
		}

		x = SCR_HorizontalAlignForString( x, align, width );
		y = SCR_VerticalAlignForString( y, align, fontHeight );

		return FTLIB_DrawRawString( x, y, str, maxwidth, NULL, font, color );
	}

	return 0;
}

//===============================================================================

/*
* SCR_RegisterPic
*/
struct shader_s *SCR_RegisterPic( const char *name ) {
	return re.RegisterPic( name );
}

/*
* SCR_DrawStretchPic
*/
void SCR_DrawStretchPic( int x, int y, int w, int h, float s1, float t1, float s2, float t2, const float *color, const struct shader_s *shader ) {
	re.DrawStretchPic( x, y, w, h, s1, t1, s2, t2, color, shader );
}

/*
* SCR_DrawFillRect
*
* Fills a box of pixels with a single color
*/
void SCR_DrawFillRect( int x, int y, int w, int h, const vec4_t color ) {
	re.DrawStretchPic( x, y, w, h, 0, 0, 1, 1, color, cls.whiteShader );
}

/*
===============================================================================

BAR GRAPHS

===============================================================================
*/

/*
* CL_AddNetgraph
*
* A new packet was just parsed
*/
void CL_AddNetgraph( void ) {
	int i;
	int ping;

	// if using the debuggraph for something else, don't
	// add the net lines
	if( scr_timegraph->integer ) {
		return;
	}

	for( i = 0; i < cls.netchan.dropped; i++ )
		SCR_DebugGraph( 30.0f, 0.655f, 0.231f, 0.169f );

	// see what the latency was on this packet
	ping = cls.realtime - cl.cmd_time[cls.ucmdAcknowledged & CMD_MASK];
	ping /= 30;
	if( ping > 30 ) {
		ping = 30;
	}
	SCR_DebugGraph( ping, 1.0f, 0.75f, 0.06f );
}


typedef struct {
	float value;
	vec4_t color;
} graphsamp_t;

static int current;
static graphsamp_t values[1024];

/*
* SCR_DebugGraph
*/
void SCR_DebugGraph( float value, float r, float g, float b ) {
	values[current].value = value;
	values[current].color[0] = r;
	values[current].color[1] = g;
	values[current].color[2] = b;
	values[current].color[3] = 1.0f;

	current++;
	current &= 1023;
}

/*
* SCR_DrawDebugGraph
*/
static void SCR_DrawDebugGraph( void ) {
	int a, x, y, w, i, h, s;
	float v;

	//
	// draw the graph
	//
	w = viddef.width;
	x = 0;
	y = 0 + viddef.height;
	SCR_DrawFillRect( x, y - scr_graphheight->integer,
					  w, scr_graphheight->integer, colorBlack );

	s = ( w + 1024 - 1 ) / 1024; //scale for resolutions with width >1024

	for( a = 0; a < w; a++ ) {
		i = ( current - 1 - a + 1024 ) & 1023;
		v = values[i].value;
		v = v * scr_graphscale->integer + scr_graphshift->integer;

		if( v < 0 ) {
			v += scr_graphheight->integer * ( 1 + (int)( -v / scr_graphheight->integer ) );
		}
		h = (int)v % scr_graphheight->integer;
		SCR_DrawFillRect( x + w - 1 - a * s, y - h, s, h, values[i].color );
	}
}

//============================================================================

/*
* SCR_InitScreen
*/
void SCR_InitScreen( void ) {
	scr_netgraph = Cvar_Get( "netgraph", "0", 0 );
	scr_timegraph = Cvar_Get( "timegraph", "0", 0 );
	scr_debuggraph = Cvar_Get( "debuggraph", "0", 0 );
	scr_graphheight = Cvar_Get( "graphheight", "32", 0 );
	scr_graphscale = Cvar_Get( "graphscale", "1", 0 );
	scr_graphshift = Cvar_Get( "graphshift", "0", 0 );

	scr_initialized = true;
}

/*
* SCR_GetScreenWidth
*/
unsigned int SCR_GetScreenWidth( void ) {
	return VID_GetWindowWidth();
}

/*
* SCR_GetScreenHeight
*/
unsigned int SCR_GetScreenHeight( void ) {
	return VID_GetWindowHeight();
}

/*
* SCR_ShutdownScreen
*/
void SCR_ShutdownScreen( void ) {
	scr_initialized = false;
}

/*
* SCR_DrawChat
*/
void SCR_DrawChat( int x, int y, int width, struct qfontface_s *font ) {
	Con_DrawChat( x, y, width, font );
}

//=============================================================================

/*
* SCR_BeginLoadingPlaque
*/
void SCR_BeginLoadingPlaque( void ) {
	S_StopAllSounds( true );

	memset( cl.configstrings, 0, sizeof( cl.configstrings ) );

	scr_draw_loading = 2;   // clear to black first
}

/*
* SCR_EndLoadingPlaque
*/
void SCR_EndLoadingPlaque( void ) {
	cls.disable_screen = 0;
}


//=======================================================

/*
* SCR_RegisterConsoleMedia
*/
void SCR_RegisterConsoleMedia() {
	cls.whiteShader = re.RegisterPic( "$whiteimage" );

	SCR_InitFonts();
}

/*
* SCR_ShutDownConsoleMedia
*/
void SCR_ShutDownConsoleMedia( void ) {
	SCR_ShutdownFonts();
}

//============================================================================

/*
* SCR_RenderView
*/
static void SCR_RenderView() {
	// frame is not valid until we load the CM data
	if( cl.cms != NULL ) {
		CL_GameModule_RenderView();
	}
}

/*
* SCR_UpdateScreen
*
* This is called every frame, and can also be called explicitly to flush
* text to the screen.
*/
void SCR_UpdateScreen( void ) {

	// if the screen is disabled (loading plaque is up, or vid mode changing)
	// do nothing at all
	if( cls.disable_screen ) {
		if( Sys_Milliseconds() - cls.disable_screen > 120000 ) {
			cls.disable_screen = 0;
			Com_Printf( "Loading plaque timed out.\n" );
		}
		return;
	}

	if( !scr_initialized || !cls.mediaInitialized ) {
		return; // not ready yet
	}

	CL_ForceVsync( cls.state == CA_DISCONNECTED );

	re.BeginFrame();

	CL_ImGuiBeginFrame();

	if( scr_draw_loading == 2 ) {
		// loading plaque over APP_STARTUP_COLOR screen
		scr_draw_loading = 0;
		UI_UpdateConnectScreen();
	} else if( cls.state == CA_DISCONNECTED ) {
		UI_Refresh();
	} else if( cls.state == CA_CONNECTING || cls.state == CA_HANDSHAKE ) {
		UI_UpdateConnectScreen();
	} else if( cls.state == CA_CONNECTED ) {
		if( cls.cgameActive ) {
			UI_UpdateConnectScreen();
			SCR_RenderView();
		} else {
			UI_UpdateConnectScreen();
		}
	} else if( cls.state == CA_ACTIVE ) {
		SCR_RenderView();

		UI_Refresh();

		if( scr_timegraph->integer ) {
			SCR_DebugGraph( cls.frametime * 0.3f, 1, 1, 1 );
		}

		if( scr_debuggraph->integer || scr_timegraph->integer || scr_netgraph->integer ) {
			SCR_DrawDebugGraph();
		}
	}

	CL_ImGuiEndFrame();

	re.EndFrame();
}
