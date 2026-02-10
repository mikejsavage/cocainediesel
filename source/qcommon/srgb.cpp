#include "qcommon/base.h"
#include "gameshared/q_math.h"

u8 LinearTosRGB( float linear ) {
	if( linear <= 0.0031308f )
		return linear * 12.92f;
	return Quantize01< u8 >( 1.055f * powf( linear, 1.0f / 2.4f ) - 0.055f );
}

float sRGBToLinear( u8 srgb8 ) {
	float srgb = Dequantize01( srgb8 );
	if( srgb <= 0.04045f )
		return srgb * ( 1.0f / 12.92f );
	return powf( ( srgb + 0.055f ) * ( 1.0f / 1.055f ), 2.4f );
}

Vec3 sRGBToLinear( RGB8 srgb ) {
	return Vec3(
		sRGBToLinear( srgb.r ),
		sRGBToLinear( srgb.g ),
		sRGBToLinear( srgb.b )
	);
}

RGB8 LinearTosRGB( Vec3 linear ) {
	return RGB8(
		LinearTosRGB( linear.x ),
		LinearTosRGB( linear.y ),
		LinearTosRGB( linear.z )
	);
}

Vec4 sRGBToLinear( RGBA8 srgb ) {
	return Vec4( sRGBToLinear( srgb.rgb() ), Dequantize01( srgb.a ) );
}

RGBA8 LinearTosRGB( Vec4 linear ) {
	return RGBA8( LinearTosRGB( linear.xyz() ), Quantize01< u8 >( linear.w ) );
}
