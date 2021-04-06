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
	float u_CrtEffect;
	float u_Brightness;
	float u_Contrast;
};

out vec4 f_Albedo;

#define SPEED 0.025 // 0.1
#define ABERRATION 1.0 // 1.0
#define STRENGTH 1.0 // 1.0

#define MPEG_BLOCKSIZE 4.0 // 16.0
#define MPEG_OFFSET 0.3 // 0.3
#define MPEG_INTERLEAVE 8.0 // 3.0

#define MPEG_BLOCKS 0.1 // 0.2
#define MPEG_BLOCK_LUMA 0.1 // 0.5
#define MPEG_BLOCK_INTERLEAVE 0.6 // 0.6

#define MPEG_LINES 0.1 // 0.7
#define MPEG_LINE_DISCOLOR 1.0 // 0.3
#define MPEG_LINE_INTERLEAVE 0.4 // 0.4

#define LENS_DISTORT 1.5 // -1.0

vec2 lensDistort( vec2 p, float power ) {
	vec2 new_p = p - 0.5;
	float rsq = new_p.x * new_p.x + new_p.y * new_p.y;
	new_p += new_p * ( power * rsq );
	new_p *= 1.0 - ( 0.25 * power );
	new_p += 0.5;
	new_p = clamp( new_p, 0.0, 1.0 );
	return mix( p, new_p, step( abs( p - 0.5 ), vec2( 0.5 ) ) );
}

vec4 noise( vec2 p ) {
	return texture( u_Noise, p );
}

float fnoise(vec2 v) {
	return fract( sin( dot( v, vec2( 12.9898, 78.233 ) ) ) * 43758.5453 );
}

vec3 mod289( vec3 x ) {
	return x - floor( x * ( 1.0 / 289.0 ) ) * 289.0;
}

vec2 mod289( vec2 x ) {
	return x - floor( x * ( 1.0 / 289.0 ) ) * 289.0;
}

vec3 permute( vec3 x ) {
	return mod289( ( ( x * 34.0 ) + 1.0 ) * x );
}

float snoise(vec2 v) {
	const vec4 C = vec4( 0.211324865405187,  // (3.0-sqrt(3.0))/6.0
	                     0.366025403784439,  // 0.5*(sqrt(3.0)-1.0)
	                     -0.577350269189626, // -1.0 + 2.0 * C.x
	                     0.024390243902439); // 1.0 / 41.0
	// First corner
	vec2 i = floor( v + dot( v, C.yy ) );
	vec2 x0 = v - i + dot( i, C.xx );

	// Other corners
	vec2 i1;
	i1 = ( x0.x > x0.y ) ? vec2(1.0, 0.0) : vec2( 0.0, 1.0 );
	vec4 x12 = x0.xyxy + C.xxzz;
	x12.xy -= i1;

	// Permutations
	i = mod289( i ); // Avoid truncation effects in permutation
	vec3 p = permute( permute( i.y + vec3( 0.0, i1.y, 1.0 ) ) + i.x + vec3( 0.0, i1.x, 1.0 ) );

	vec3 m = max( 0.5 - vec3( dot( x0, x0 ), dot( x12.xy, x12.xy ), dot( x12.zw, x12.zw ) ), 0.0 );
	m = m * m;
	m = m * m;

	// Gradients: 41 points uniformly over a line, mapped onto a diamond.
	// The ring size 17*17 = 289 is close to a multiple of 41 (41*7 = 287)

	vec3 x = 2.0 * fract( p * C.www ) - 1.0;
	vec3 h = abs( x ) - 0.5;
	vec3 ox = floor( x + 0.5 );
	vec3 a0 = x - ox;

	// Normalise gradients implicitly by scaling m
	// Approximation of: m *= inversesqrt( a0*a0 + h*h );
	m *= 1.79284291400159 - 0.85373472095314 * ( a0 * a0 + h * h );

	// Compute final noise value at P
	vec3 g;
	g.x = a0.x * x0.x + h.x * x0.y;
	g.yz = a0.yz * x12.xz + h.yz * x12.yw;
	return 130.0 * dot( m, g );
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
	return vec3( 0.0, 0.0, 0.0 );
}

vec3 SampleScreen( vec2 uv ) {
	return texture( u_Screen, uv ).rgb;
}

vec3 glitch( vec2 uv, float amount ) {
	vec3 col;

	float time = u_Time * SPEED;

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

	col.r = SampleScreen( uv_r ).r;
	col.g = SampleScreen( uv_g ).g;
	col.b = SampleScreen( uv_b ).b;

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

float roundBox( vec2 p, vec2 b, float r ) {
	return length( max( abs( p ) - b, 0.0 ) ) - r;
}

float crt_on_screen( vec2 uv ) {
	float dist_screen = roundBox( uv, vec2( 1.0 ), 0.0 ) * 150.0;
	dist_screen = clamp( dist_screen, 0.0, 1.0 );
	return 1.0 - dist_screen;
}

vec2 crtDistort( vec2 uv, float amount ) {
	vec2 screen_uv = uv;

	// screen distort
	screen_uv = screen_uv * 2.0 - 1.0;
	screen_uv *= vec2( 1.11, 1.14 ) * 0.97;
	float bend = 4.0;
	screen_uv.x *= 1.0 + pow( abs( screen_uv.y ) / bend, 2.0 );
	screen_uv.y *= 1.0 + pow( abs( screen_uv.x ) / bend, 2.0 );

	float on_screen = crt_on_screen( screen_uv );

	screen_uv = mix( screen_uv, clamp( screen_uv, -1.0, 1.0 ), on_screen );

	screen_uv = screen_uv * 0.5 + 0.5;

	// horizontal fuzz
	screen_uv.x += fnoise( vec2( u_Time * 15.0, uv.y * 80.0 ) ) * 0.002 * on_screen;

	uv = mix( uv, screen_uv, amount );

	return uv;
}

vec3 crtEffect( vec3 color, vec2 uv, float amount ) {
	const vec3 edge_color = vec3( 0.3, 0.25, 0.3 ) * 0.01;
	vec3 new_color = color;

	// screen edge
	vec2 screen_uv = uv * 2.0 - 1.0;

	float dist_in = roundBox( screen_uv, vec2( 1.0, 1.03 ), 0.05 ) * 150.0;
	dist_in = clamp( dist_in, 0.0, 1.0 );

	float on_screen = crt_on_screen( screen_uv );

	float light = dot( screen_uv, vec2( -1.0 ) );
	light = clamp( light, 0.0, 1.0 );
	light = smoothstep( -0.05, 0.2, light );
	// return vec3( light );

	vec3 screen_edge = mix( ( edge_color + light * 0.03 ) * dist_in, vec3( 0.0 ), 0.0 );

	// banding
	float banding = 0.6 + fract( uv.y + fnoise( uv ) * 0.05 - u_Time * 0.3 ) * 0.4;
	new_color = mix( screen_edge, new_color, banding * on_screen );

	// scanlines
	float scanline = abs( sin( u_Time * 10.0 - uv.y * 400.0 ) ) * 0.01 * on_screen;
	new_color -= scanline;

	new_color = mix( color, new_color, amount );
	return mix( new_color, screen_edge, 1.0 - on_screen );
}

vec3 brightnessContrast( vec3 value, float brightness, float contrast ) {
	return (value - 0.5) * contrast + 0.5 + brightness;
}

void main() {
	vec2 uv = gl_FragCoord.xy / u_ViewportSize;

	float crt_amount = u_CrtEffect;
	float glitch_amount = u_Damage * STRENGTH;
	uv = crtDistort( uv, crt_amount );
	uv = lensDistort( uv, glitch_amount * LENS_DISTORT );

	vec3 color = glitch( uv, glitch_amount );
	color = crtEffect( color, uv, crt_amount );
	color = LinearTosRGB( color );
	if( all( lessThanEqual( abs( uv - 0.5 ), vec2( 0.5 ) ) ) ) {
		color = brightnessContrast( color, u_Brightness, u_Contrast );
	}
	f_Albedo = vec4( color, 1.0 );
}

#endif
