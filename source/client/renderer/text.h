#pragma once

#include "qcommon/types.h"

enum XAlignment {
	XAlignment_Left,
	XAlignment_Center,
	XAlignment_Right,
};

enum YAlignment {
	YAlignment_Ascent,
	YAlignment_Baseline,
	YAlignment_Descent,
};

struct Alignment {
	XAlignment x;
	YAlignment y;
};

constexpr Alignment Alignment_LeftTop = { XAlignment_Left, YAlignment_Ascent };
constexpr Alignment Alignment_CenterTop = { XAlignment_Center, YAlignment_Ascent };
constexpr Alignment Alignment_RightTop = { XAlignment_Right, YAlignment_Ascent };
constexpr Alignment Alignment_LeftMiddle = { XAlignment_Left, YAlignment_Baseline };
constexpr Alignment Alignment_CenterMiddle = { XAlignment_Center, YAlignment_Baseline };
constexpr Alignment Alignment_RightMiddle = { XAlignment_Right, YAlignment_Baseline };
constexpr Alignment Alignment_LeftBottom = { XAlignment_Left, YAlignment_Descent };
constexpr Alignment Alignment_CenterBottom = { XAlignment_Center, YAlignment_Descent };
constexpr Alignment Alignment_RightBottom = { XAlignment_Right, YAlignment_Descent };

struct Font;

void InitText();
void ShutdownText();

const Font * RegisterFont( Span< const char > path );

void DrawTextBaseline( const Font * font, float pixel_size,
	Span< const char > str,
	float x, float y,
	Vec4 color, Optional< Vec4 > border_color = NONE );

void DrawFittedText( const Font * font, Span< const char > str,
	MinMax2 bounds, XAlignment x_alignment,
	Vec4 color, Optional< Vec4 > border_color = NONE );

void DrawText( const Font * font, float pixel_size,
	Span< const char > str,
	float x, float y,
	Vec4 color, Optional< Vec4 > border_color = NONE );
void DrawText( const Font * font, float pixel_size,
	const char * str,
	float x, float y,
	Vec4 color, Optional< Vec4 > border_color = NONE );

MinMax2 TextVisualBounds( const Font * font, float pixel_size, Span< const char > str );
MinMax2 TextBaselineBounds( const Font * font, float pixel_size, Span< const char > str );

void DrawText( const Font * font, float pixel_size,
	Span< const char > str,
	Alignment align, float x, float y,
	Vec4 color, Optional< Vec4 > border_color = NONE );
void DrawText( const Font * font, float pixel_size,
	const char * str,
	Alignment align, float x, float y,
	Vec4 color, Optional< Vec4 > border_color = NONE );

void Draw3DText( const Font * font, float size,
	Span< const char > str,
	Vec3 origin, EulerDegrees3 angles, Vec4 color );
