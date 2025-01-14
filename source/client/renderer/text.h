#pragma once

#include "qcommon/types.h"

struct Font;

void InitText();
void ShutdownText();

const Font * RegisterFont( Span< const char > path );

void DrawText( const Font * font, float pixel_size,
	Span< const char > str,
	float x, float y,
	Vec4 color, Optional< Vec4 > border_color = NONE );
void DrawText( const Font * font, float pixel_size,
	const char * str,
	float x, float y,
	Vec4 color, Optional< Vec4 > border_color = NONE );

MinMax2 TextBounds( const Font * font, float pixel_size, Span< const char > str );
MinMax2 TextBounds( const Font * font, float pixel_size, const char * str );

void DrawText( const Font * font, float pixel_size,
	const char * str,
	Alignment align, float x, float y,
	Vec4 color, Optional< Vec4 > border_color = NONE );

void Draw3DText( const Font * font, float size,
	Span< const char > str,
	Vec3 origin, EulerDegrees3 angles, Vec4 color );
