#pragma once

#include "qcommon/types.h"

struct Font;

bool InitText();
void ShutdownText();

const Font * RegisterFont( const char * path );

void DrawText( const Font * font, float pixel_size,
	const char * str,
	float x, float y,
	Vec4 color, bool border = false );
void DrawText( const Font * font, float pixel_size,
	const char * str,
	float x, float y,
	Vec4 color, Vec4 border_color );

MinMax2 TextBounds( const Font * font, float pixel_size, const char * str );

void DrawText( const Font * font, float pixel_size,
	const char * str,
	Alignment align, float x, float y,
	Vec4 color, bool border = false );
void DrawText( const Font * font, float pixel_size,
	const char * str,
	Alignment align, float x, float y,
	Vec4 color, Vec4 border_color );
