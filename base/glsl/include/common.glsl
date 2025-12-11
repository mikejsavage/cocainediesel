const float PI = 3.14159265358979323846;

const uint MASK_CURVED = 1u;

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

mat3 Adjugate( mat3 m ) {
	return mat3(
		cross( m[ 1 ], m[ 2 ] ),
		cross( m[ 2 ], m[ 0 ] ),
		cross( m[ 0 ], m[ 1 ] )
	);
}

mat4 AffineToMat4( AffineTransform t ) {
	return mat4(
		t.m[ 0 ][ 0 ], t.m[ 0 ][ 1 ], t.m[ 0 ][ 2 ], 0.0,
		t.m[ 0 ][ 3 ], t.m[ 1 ][ 0 ], t.m[ 1 ][ 1 ], 0.0,
		t.m[ 1 ][ 2 ], t.m[ 1 ][ 3 ], t.m[ 2 ][ 0 ], 0.0,
		t.m[ 2 ][ 1 ], t.m[ 2 ][ 2 ], t.m[ 2 ][ 3 ], 1.0
	);
}

// out of bounds texelFetch is UB
vec4 ClampedTexelFetch( sampler2D texture, ivec2 uv, int lod ) {
	uv = clamp( uv, ivec2( 0 ), ivec2( textureSize( texture, lod ) ) - 1 );
	return texelFetch( texture, uv, lod );
}

vec4 ClampedTexelFetch( sampler2DMS texture, ivec2 uv, int msaa_sample ) {
	uv = clamp( uv, ivec2( 0 ), ivec2( textureSize( texture ) ) - 1 );
	return texelFetch( texture, uv, msaa_sample );
}

struct Quaternion {
	vec4 q;
};

void QuaternionToBasis( Quaternion q, out vec3 normal, out vec3 tangent, out vec3 bitangent ) {
	normal = vec3(
		1.0 - 2.0 * ( q.q.y * q.q.y + q.q.z * q.q.z ),
		2.0 * ( q.q.x * q.q.y + q.q.z * q.q.w ),
		2.0 * ( q.q.x * q.q.z - q.q.y * q.q.w )
	);

	tangent = vec3(
		2.0 * ( q.q.x * q.q.y - q.q.z * q.q.w ),
		1.0 - 2.0 * ( q.q.x * q.q.x + q.q.z * q.q.z ),
		2.0 * ( q.q.y * q.q.z + q.q.x * q.q.w )
	);

	bitangent = vec3(
		2.0 * ( q.q.x * q.q.z + q.q.y * q.q.w ),
		2.0 * ( q.q.y * q.q.z - q.q.x * q.q.w ),
		1.0 - 2.0 * ( q.q.x * q.q.x + q.q.y * q.q.y )
	);
}

float IGNDither( vec2 pos, float intensity ) {
	return (mod( 52.9829189 * mod( 0.06711056 * pos.x + 0.00583715 * pos.y, 1.0 ), 1.0 ) - 0.5) * intensity;
}