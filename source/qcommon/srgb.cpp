#include "qcommon/base.h"

static float DequantizeU8( u8 x ) { return x / 255.0f; }
static u8 QuantizeU8( float x ) { return u8( x * 255.0f + 0.5f ); }

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
		sRGBToLinear( DequantizeU8( srgb.r ) ),
		sRGBToLinear( DequantizeU8( srgb.g ) ),
		sRGBToLinear( DequantizeU8( srgb.b ) )
	);
}

RGB8 LinearTosRGB( Vec3 linear ) {
	return RGB8(
		QuantizeU8( LinearTosRGB( linear.x ) ),
		QuantizeU8( LinearTosRGB( linear.y ) ),
		QuantizeU8( LinearTosRGB( linear.z ) )
	);
}

Vec4 sRGBToLinear( RGBA8 srgb ) {
	return Vec4( sRGBToLinear( srgb.rgb() ), DequantizeU8( srgb.a ) );
}

RGBA8 LinearTosRGB( Vec4 linear ) {
	return RGBA8( LinearTosRGB( linear.xyz() ), QuantizeU8( linear.w ) );
}
