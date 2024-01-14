#include "qcommon/base.h"
#include "gameshared/q_math.h"

float LinearTosRGB( float linear ) {
	if( linear <= 0.0031308f )
		return linear * 12.92f;
	return 1.055f * powf( linear, 1.0f / 2.4f ) - 0.055f;
}

float sRGBToLinear( float srgb ) {
	if( srgb <= 0.04045f )
		return srgb * ( 1.0f / 12.92f );
	return float( powf( ( srgb + 0.055f ) * ( 1.0f / 1.055f ), 2.4f ) );
}

Vec3 sRGBToLinear( RGB8 srgb ) {
	return Vec3(
		sRGBToLinear( Dequantize01< u8 >( srgb.r ) ),
		sRGBToLinear( Dequantize01< u8 >( srgb.g ) ),
		sRGBToLinear( Dequantize01< u8 >( srgb.b ) )
	);
}

RGB8 LinearTosRGB( Vec3 linear ) {
	return RGB8(
		Quantize01< u8 >( LinearTosRGB( linear.x ) ),
		Quantize01< u8 >( LinearTosRGB( linear.y ) ),
		Quantize01< u8 >( LinearTosRGB( linear.z ) )
	);
}

Vec4 sRGBToLinear( RGBA8 srgb ) {
	return Vec4( sRGBToLinear( srgb.rgb() ), Dequantize01< u8 >( srgb.a ) );
}

RGBA8 LinearTosRGB( Vec4 linear ) {
	return RGBA8( LinearTosRGB( linear.xyz() ), Quantize01< u8 >( linear.w ) );
}
