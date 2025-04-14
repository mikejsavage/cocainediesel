#pragma once

#include "qcommon/types.h"
#include "client/renderer/text.h"

#include "clay/clay.h"

enum ClayFont : decltype( Clay_TextElementConfig::fontId ) {
	ClayFont_Regular,
	ClayFont_Bold,
	ClayFont_Italic,
	ClayFont_BoldItalic,

	ClayFont_Count
};

enum ClayCustomElementType {
	ClayCustomElementType_Callback,
	ClayCustomElementType_ImGui,
	ClayCustomElementType_Lua,
	ClayCustomElementType_FittedText,
};

using ClayCustomElementCallback = void ( * )( const Clay_BoundingBox & bounds, void * userdata );

struct FittedTextShadow {
	Vec4 color;
	float offset;
	Optional< float > angle;
};

struct ClayCustomElementConfig {
	ClayCustomElementType type;
	union {
		struct {
			ClayCustomElementCallback f;
			void * userdata;
		} callback;

		struct {
			int callback_ref;
			int arg_ref;
		} lua;

		struct {
			Clay_String text;
			Clay_TextElementConfig config;
			XAlignment alignment;
			Clay_Padding padding;
			Optional< Vec4 > border_color;
			Optional< FittedTextShadow > shadow;
		} fitted_text;
	};
};

ArenaAllocator * ClayAllocator();
Clay_String AllocateClayString( Span< const char > str );
