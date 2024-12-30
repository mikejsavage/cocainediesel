#include "../../source/client/renderer/shader_shared.h"

static const float PI = 3.14159265358979323846;

static const uint MASK_CURVED = 1u;

float sRGBToLinear( float srgb ) {
	if( srgb <= 0.04045f )
		return srgb * ( 1.0f / 12.92f );
	return pow( ( srgb + 0.055f ) * ( 1.0f / 1.055f ), 2.4f );
}

float3 sRGBToLinear( float3 srgb ) {
	return float3( sRGBToLinear( srgb.r ), sRGBToLinear( srgb.g ), sRGBToLinear( srgb.b ) );
}

float4 sRGBToLinear( float4 srgb ) {
	return float4( sRGBToLinear( srgb.r ), sRGBToLinear( srgb.g ), sRGBToLinear( srgb.b ), srgb.a );
}

float DequantizeU8( uint x ) {
	return x / 255.0f;
}

float4 sRGBToLinear( uint srgb ) {
	uint r = ( srgb >> 0u  ) & 0xffu;
	uint g = ( srgb >> 8u  ) & 0xffu;
	uint b = ( srgb >> 16u ) & 0xffu;
	uint a = ( srgb >> 24u ) & 0xffu;

	return float4(
		sRGBToLinear( DequantizeU8( r ) ),
		sRGBToLinear( DequantizeU8( g ) ),
		sRGBToLinear( DequantizeU8( b ) ),
		sRGBToLinear( DequantizeU8( a ) )
	);
}

float LinearTosRGB( float lin ) {
	if( lin <= 0.0031308f )
		return lin * 12.92f;
	return 1.055f * pow( lin, 1.0f / 2.4f ) - 0.055f;
}

float3 LinearTosRGB( float3 lin ) {
	return float3( LinearTosRGB( lin.r ), LinearTosRGB( lin.g ), LinearTosRGB( lin.b ) );
}

float4 LinearTosRGB( float4 lin ) {
	return float4( LinearTosRGB( lin.r ), LinearTosRGB( lin.g ), LinearTosRGB( lin.b ), lin.a );
}

// out of bounds texture load is UB
template< typename T >
T ClampedTextureLoad( Texture2D< T > tex, int2 uv ) {
	float2 texture_size;
	tex.GetDimensions( texture_size.x, texture_size.y );
	uv = clamp( uv, 0, int2( texture_size ) - 1 );
	return tex.Load( uv );
}

float4 ClampedTextureLoad( Texture2DMS< float > tex, int2 uv, int msaa_sample ) {
	float2 texture_size;
	float num_samples_dont_care;
	tex.GetDimensions( texture_size.x, texture_size.y, num_samples_dont_care );
	uv = clamp( uv, 0, int2( texture_size ) - 1 );
	return tex.Load( uv, msaa_sample );
}

float3x3 Adjugate( float3x4 m ) {
	float3 col0 = m._m00_m10_m20;
	float3 col1 = m._m01_m11_m21;
	float3 col2 = m._m02_m12_m22;
    return float3x3(
		cross( col1, col2 ),
		cross( col2, col0 ),
		cross( col0, col1 )
	);
}

float4 mul34( float3x4 m, float4 v ) {
	return float4( mul( m, v ), v.w );
}

float3x4 mul34( float3x4 lhs, float3x4 rhs ) {
	float4 col0 = float4( rhs._m00_m10_m20, 0.0f );
	float4 col1 = float4( rhs._m01_m11_m21, 0.0f );
	float4 col2 = float4( rhs._m02_m12_m22, 0.0f );
	float4 col3 = float4( rhs._m03_m13_m23, 1.0f );
	return float3x4(
		dot( lhs[ 0 ], col0 ),
		dot( lhs[ 0 ], col1 ),
		dot( lhs[ 0 ], col2 ),
		dot( lhs[ 0 ], col3 ),

		dot( lhs[ 1 ], col0 ),
		dot( lhs[ 1 ], col1 ),
		dot( lhs[ 1 ], col2 ),
		dot( lhs[ 1 ], col3 ),

		dot( lhs[ 2 ], col0 ),
		dot( lhs[ 2 ], col1 ),
		dot( lhs[ 2 ], col2 ),
		dot( lhs[ 2 ], col3 )
	);
}
