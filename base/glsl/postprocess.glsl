#include "include/uniforms.glsl"
#include "include/common.glsl"

#if VERTEX_SHADER

in vec4 a_Position;

void main() {
	gl_Position = a_Position;
}

#else

uniform sampler2D u_Screen;
uniform sampler2D u_Noise;

layout( std140 ) uniform u_PostProcess {
	float u_Time;
	float u_Damage;
};

out vec4 f_Albedo;

#define SPEED 0.1 // 0.1
#define ABERRATION 1.0 // 1.0
#define STRENGTH 1.0 // 1.0

#define MPEG_BLOCKSIZE 16.0 // 16.0
#define MPEG_OFFSET 0.3 // 0.3
#define MPEG_INTERLEAVE 3.0 // 3.0

#define MPEG_BLOCKS 0.2 // 0.2
#define MPEG_BLOCK_LUMA 0.5 // 0.5
#define MPEG_BLOCK_INTERLEAVE 0.6 // 0.6

#define MPEG_LINES 0.7 // 0.7
#define MPEG_LINE_DISCOLOR 0.3 // 0.3
#define MPEG_LINE_INTERLEAVE 0.4 // 0.4

#define LENS_DISTORT -1.0 // -1.0

vec2 lensDistort( vec2 p, float power ) {
	p -= 0.5;
	float rsq = p.x * p.x + p.y * p.y;
	p += p * ( power * rsq );
	p *= 1.0 - ( 0.25 * power );
	return p + 0.5;
}

vec4 noise( vec2 p ) {
	return texture( u_Noise, p );
}

vec4 vec4pow( in vec4 v, in float p ) {
	return vec4( pow( v.xyz, vec3( p ) ), v.w );
}

vec3 interleave( vec2 uv ) {
	float line = fract( uv.y / MPEG_INTERLEAVE );
	vec3 mask = vec3( MPEG_INTERLEAVE, 0.0, 0.0 );
	if( line > 0.333 )
		mask = vec3( 0.0, MPEG_INTERLEAVE, 0.0 );
	if( line > 0.666 )
		mask = vec3( 0.0, 0.0, MPEG_INTERLEAVE );

	return mask;
}

vec3 discolor( vec3 col ) {
	return vec3( 0.0, 1.0 - dot( col.rgb, vec3(1.0) ), 0.0 );
}

vec3 glitch( vec2 uv ) {
	vec3 col;

	float time = u_Time * SPEED;

	float amount = u_Damage * STRENGTH;

	uv = lensDistort( uv, amount * LENS_DISTORT );
	vec2 distorted_fragCoord = uv * u_ViewportSize;

	vec2 block = floor( distorted_fragCoord / MPEG_BLOCKSIZE );
	vec2 uv_noise = block / vec2( 64.0 ) + floor( vec2( time ) * vec2( 1234.0, 3543.0 ) ) / vec2( 64.0 );

	float block_thresh = pow( fract( time * 1236.0453 ), 2.0 ) * MPEG_BLOCKS * amount;
	float line_thresh = pow( fract( time * 2236.0453 ), 3.0 ) * MPEG_LINES * amount;

	vec2 uv_r = uv;
	vec2 uv_g = uv;
	vec2 uv_b = uv;

	vec4 blocknoise = noise( uv_noise );
	vec4 linenoise = noise( vec2( uv_noise.y, 0.0 ) );

	// glitch some blocks and lines / chromatic aberration
	if( blocknoise.r < block_thresh || linenoise.g < line_thresh ) {
		vec2 dist = ( fract( uv_noise ) - 0.5 ) * MPEG_OFFSET * ABERRATION;
		uv_r += dist * 0.1;
		uv_g += dist * 0.2;
		uv_b += dist * 0.125;
	}
	else { // remainder gets just chromatic aberration
		vec4 power = vec4pow( noise( vec2( time, time * 0.08 ) ), 8.0 );
		vec4 shift = power * vec4( vec3( amount * ABERRATION ), 1.0 );
		shift *= 2.0 * shift.w - 1.0;
		uv_r += vec2( shift.x, -shift.y );
		uv_g += vec2( shift.y, -shift.z );
		uv_b += vec2( shift.z, -shift.x );
	}

	col.r = texture( u_Screen, uv_r ).r;
	col.g = texture( u_Screen, uv_g ).g;
	col.b = texture( u_Screen, uv_b ).b;

	// lose luma for some blocks
	if( blocknoise.g < block_thresh * MPEG_BLOCK_LUMA )
		col = col.ggg;

	// discolor block lines
	if( linenoise.b < line_thresh * MPEG_LINE_DISCOLOR )
		col = discolor( col );

	// interleave lines in some blocks
	if( blocknoise.g < block_thresh * MPEG_BLOCK_INTERLEAVE ||	linenoise.g < line_thresh * MPEG_LINE_INTERLEAVE ) {
		col *= interleave( distorted_fragCoord );
	}

	return col;
}

void main() {
	vec2 uv = gl_FragCoord.xy / u_ViewportSize;
	f_Albedo = vec4( LinearTosRGB( glitch( uv ) ), 1.0 );
}

#endif
