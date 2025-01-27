#pragma once

#include "qcommon/types.h"
#include "client/renderer/text.h"

#define CLAY_EXTEND_CONFIG_TEXT \
	Optional< Clay_Color > borderColor;

#define CLAY_EXTEND_CONFIG_IMAGE \
	Clay_Color tint;

enum ClayCustomElementType {
	ClayCustomElementType_Lua,
	ClayCustomElementType_FittedText,
};

#define CLAY_EXTEND_CONFIG_CUSTOM \
	ClayCustomElementType type; \
	union { \
		struct { \
			int callback_ref; \
			int arg_ref; \
		} lua; \
		struct { \
			Clay_String text; \
			Clay_TextElementConfig config; \
			XAlignment alignment; \
			Clay_Padding padding; \
		} fitted_text; \
	};

#include "clay/clay.h"
