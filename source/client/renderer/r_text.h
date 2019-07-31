#pragma once

#include "qcommon/types.h"

struct Font;

enum XAlignment {
	XAlignment_Left,
	XAlignment_Center,
	XAlignment_Right,
};

enum YAlignment {
	YAlignment_Top,
	YAlignment_Middle,
	YAlignment_Bottom,
};

struct Alignment {
	XAlignment x;
	YAlignment y;
};

constexpr Alignment Alignment_LeftTop = { XAlignment_Left, YAlignment_Top };
constexpr Alignment Alignment_CenterTop = { XAlignment_Center, YAlignment_Top };
constexpr Alignment Alignment_RightTop = { XAlignment_Right, YAlignment_Top };
constexpr Alignment Alignment_LeftMiddle = { XAlignment_Left, YAlignment_Middle };
constexpr Alignment Alignment_CenterMiddle = { XAlignment_Center, YAlignment_Middle };
constexpr Alignment Alignment_RightMiddle = { XAlignment_Right, YAlignment_Middle };
constexpr Alignment Alignment_LeftBottom = { XAlignment_Left, YAlignment_Bottom };
constexpr Alignment Alignment_CenterBottom = { XAlignment_Center, YAlignment_Bottom };
constexpr Alignment Alignment_RightBottom = { XAlignment_Right, YAlignment_Bottom };

bool R_InitText();
void R_ShutdownText();

const Font * RegisterFont( const char * path );
void TouchFonts();
void ResetTextVBO();

void DrawText( const Font * font, float pixel_size,
	const char * str,
	float x, float y,
	RGBA8 color, bool border = false, RGBA8 border_color = RGBA8( 0, 0, 0, 0 ) );
void DrawText( const Font * font, float pixel_size,
	Span< const char > str,
	float x, float y,
	RGBA8 color, bool border = false, RGBA8 border_color = RGBA8( 0, 0, 0, 0 ) );

MinMax2 TextBounds( const Font * font, float pixel_size, const char * str );

void DrawText( const Font * font, float pixel_size,
	const char * str,
	Alignment align, float x, float y,
	RGBA8 color, bool border = false, RGBA8 border_color = RGBA8( 0, 0, 0, 0 ) );
