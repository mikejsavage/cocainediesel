#define M_PI 3.14159265358979323846

float sRGBToLinear( float srgb ) {
	if( srgb <= 0.04045 )
		return srgb * ( 1.0 / 12.92 );
	return pow( ( srgb + 0.055 ) * ( 1.0 / 1.055 ), 2.4 );
}

vec3 sRGBToLinear( vec3 srgb ) {
	return vec3( sRGBToLinear( srgb.r ), sRGBToLinear( srgb.g ), sRGBToLinear( srgb.b ) );
}

vec4 sRGBToLinear( vec4 srgb ) {
	return vec4( sRGBToLinear( srgb.r ), sRGBToLinear( srgb.g ), sRGBToLinear( srgb.b ), srgb.a );
}

float DequantizeU8( uint x ) {
	return x / 255.0f;
}

vec4 sRGBToLinear( uint srgb ) {
	uint r = ( srgb >> 0u  ) & 0xffu;
	uint g = ( srgb >> 8u  ) & 0xffu;
	uint b = ( srgb >> 16u ) & 0xffu;
	uint a = ( srgb >> 24u ) & 0xffu;

	return vec4(
		sRGBToLinear( DequantizeU8( r ) ),
		sRGBToLinear( DequantizeU8( g ) ),
		sRGBToLinear( DequantizeU8( b ) ),
		sRGBToLinear( DequantizeU8( a ) )
	);
}

float LinearTosRGB( float linear ) {
	if( linear <= 0.0031308 )
		return linear * 12.92;
	return 1.055 * pow( linear, 1.0 / 2.4 ) - 0.055;
}

vec3 LinearTosRGB( vec3 linear ) {
	return vec3( LinearTosRGB( linear.r ), LinearTosRGB( linear.g ), LinearTosRGB( linear.b ) );
}

vec4 LinearTosRGB( vec4 linear ) {
	return vec4( LinearTosRGB( linear.r ), LinearTosRGB( linear.g ), LinearTosRGB( linear.b ), linear.a );
}

float LinearizeDepth( float ndc ) {
	return u_NearClip / ( 1.0 - ndc );
}

vec3 NormalToRGB( vec3 normal ) {
	return normal * 0.5 + 0.5;
}

float Unlerp( float lo, float x, float hi ) {
	return ( x - lo ) / ( hi - lo );
}

mat4 AffineToMat4( mat4x3 m ) {
	return mat4( m );
	/* return mat4( m[ 0 ], m[ 1 ], m[ 2 ], vec4( 0.0, 0.0, 0.0, 1.0 ) ); */
}
