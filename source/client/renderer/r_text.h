#pragma once

#include "qcommon/types.h"

struct Font;

bool R_InitText();
void R_ShutdownText();

const Font * RegisterFont( const char * path );
void TouchFonts();
void ResetTextVBO();

void DrawText( const Font * font, float pixel_size, const char * str, float x, float y, RGBA8 color, bool border = false, RGBA8 border_color = RGBA8( 0, 0, 0, 0 ) );
void DrawText( const Font * font, float pixel_size, Span< const char > str, float x, float y, RGBA8 color, bool border = false, RGBA8 border_color = RGBA8( 0, 0, 0, 0 ) );

Vec2 TextSize( const Font * font, float pixel_size, const char * str );
