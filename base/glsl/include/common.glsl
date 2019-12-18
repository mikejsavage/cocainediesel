#define M_PI 3.14159265358979323846

float sRGBToLinear( float srgb ) {
	if( srgb <= 0.04045 )
		return srgb * ( 1.0 / 12.92 );
	return float( pow( ( srgb + 0.055 ) * ( 1.0 / 1.055 ), 2.4 ) );
}

vec3 sRGBToLinear( vec3 srgb ) {
	return vec3( sRGBToLinear( srgb.r ), sRGBToLinear( srgb.g ), sRGBToLinear( srgb.b ) );
}

vec4 sRGBToLinear( vec4 srgb ) {
	return vec4( sRGBToLinear( srgb.r ), sRGBToLinear( srgb.g ), sRGBToLinear( srgb.b ), srgb.a );
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

vec2 OctahedronWrap( vec2 v ) {
	float x = ( 1.0 - abs( v.y ) ) * ( v.x >= 0.0 ? 1.0 : -1.0 );
	float y = ( 1.0 - abs( v.x ) ) * ( v.y >= 0.0 ? 1.0 : -1.0 );
	return vec2( x, y );
}

vec2 CompressNormal( vec3 n ) {
	n /= abs( n.x ) + abs( n.y ) + abs( n.z );
	vec2 oct = n.z >= 0.0 ? n.xy : OctahedronWrap( n.xy );
	return oct * 0.5 + 0.5;
}

vec3 DecompressNormal( vec2 oct ) {
	oct = oct * 2.0 - 1.0;
	vec3 n = vec3( oct.x, oct.y, 1.0 - abs( oct.x ) - abs( oct.y ) );
	float t = max( -n.z, 0.0 );
	n.x += n.x >= 0.0 ? -t : t;
	n.y += n.y >= 0.0 ? -t : t;
	return normalize( n );
}
