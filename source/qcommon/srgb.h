#pragma once

#include "qcommon/types.h"

float sRGBToLinear( float srgb );
Vec3 sRGBToLinear( RGB8 srgb );
Vec4 sRGBToLinear( RGBA8 srgb );

float LinearTosRGB( float linear );
RGB8 LinearTosRGB( Vec3 linear );
RGBA8 LinearTosRGB( Vec4 linear );
