#pragma once

#define CLAY_EXTEND_CONFIG_TEXT \
	Optional< Clay_Color > borderColor;

#define CLAY_EXTEND_CONFIG_IMAGE \
	Clay_Color tint;

#define CLAY_EXTEND_CONFIG_CUSTOM \
	int callback_ref; \
	int arg_ref;

#include "qcommon/types.h"
#include "clay/clay.h"
