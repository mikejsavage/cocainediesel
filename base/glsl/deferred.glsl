#include "include/uniforms.glsl"
#include "include/common.glsl"
#include "include/skinning.glsl"
#include "include/dither.glsl"
#include "include/fog.glsl"

#if VERTEX_SHADER

in vec4 a_Position;

void main() {
	gl_Position = a_Position;
}

#else

#define APPLY_DLIGHTS 1

layout( std140 ) uniform u_Outline {
	vec4 u_OutlineColor;
};

uniform sampler2D u_Albedo;
uniform sampler2D u_RMAC;
uniform sampler2D u_Normal;
#if MSAA
uniform sampler2DMS u_Depth;
#else
uniform sampler2D u_Depth;
#endif
uniform isamplerBuffer u_DynamicCount;

out vec4 f_Albedo;

#include "include/lighting.glsl"
#include "include/shadow.glsl"

void GetPositionNormal( ivec2 uv, float depth, out vec3 pos, out vec3 norm ) {
	norm = DecompressNormal( texelFetch( u_Normal, uv, 0 ).xy );
	pos = vec3( vec2( uv ) / u_ViewportSize, depth ) * 2.0 - 1.0;
	vec4 clip = u_InverseP * vec4( pos, 1.0 );
	clip /= clip.w;
	pos = ( u_InverseV * clip ).xyz;
}

vec3 GetPosition( ivec2 uv ) {
	float depth = texelFetch( u_Depth, uv, 0 ).r;
	vec3 pos = vec3( vec2( uv ) / u_ViewportSize, depth ) * 2.0 - 1.0;
	vec4 clip = u_InverseP * vec4( pos, 1.0 );
	clip /= clip.w;
	return ( u_InverseV * clip ).xyz;
}

vec3 GetNormal( ivec2 uv ) {
	return DecompressNormal( texelFetch( u_Normal, uv, 0 ).xy );
}

float edgeDetect( float center, float up, float down_left, float down_right, float epsilon ) {
	float delta = 4.0 * center - 2.0 * up - down_left - down_right;
	return smoothstep( 0.0, epsilon, abs( delta ) );
}

void main() {
	ivec2 p = ivec2( gl_FragCoord.xy );
	vec4 albedo = texelFetch( u_Albedo, p, 0 );
	if( albedo.a == 0.0 ) discard;

	vec3 position = GetPosition( p );
	vec3 normal = GetNormal( p );

	vec4 rmac = texelFetch( u_RMAC, p, 0 ).xyzw;

	ivec3 pixel = ivec3( 0, 1, -1 );
	float epsilon = mix( 0.00001, 0.001, rmac.w );
	float edgeness = 0.0;
	float avg_depth = 0.0;
#if MSAA
	for( int i = 0; i < u_Samples; i++ )
#else
	const int i = 0;
#endif
	{
		float depth =            texelFetch( u_Depth, p, i ).r;
		float depth_up =         texelFetch( u_Depth, p + pixel.xz, i ).r;
		float depth_down_left =  texelFetch( u_Depth, p + pixel.yy, i ).r;
		float depth_down_right = texelFetch( u_Depth, p + pixel.zy, i ).r;
		edgeness += edgeDetect( depth, depth_up, depth_down_left, depth_down_right, epsilon );
		avg_depth += depth + depth_up + depth_down_left + depth_down_right;
	}
#if MSAA
	edgeness /= u_Samples;
	avg_depth /= u_Samples * 4.0;
#else
	avg_depth /= 4.0;
#endif

	vec2 clamping = clamp( u_ViewportSize - abs( u_ViewportSize - gl_FragCoord.xy * 2.0 ), 1.0, 2.0 ) - 1.0;
	edgeness *= min( clamping.x, clamping.y );

	float shadowlight = GetLight( position, normal );

	float tile_size = float( TILE_SIZE );
	int tile_row = int( ( u_ViewportSize.y - gl_FragCoord.y ) / tile_size );
	int tile_col = int( gl_FragCoord.x / tile_size );
	int cols = int( u_ViewportSize.x + tile_size - 1 ) / int( tile_size );
	int tile_index = tile_row * cols + tile_col;
	int dlight_count = texelFetch( u_DynamicCount, tile_index ).y;

	vec3 viewDir = normalize( u_CameraPos - position );
	vec4 diffuse = vec4( 1.0 );
	albedo.rgb = Fog( albedo.rgb, length( position - u_CameraPos ) );
	edgeness = FogAlpha( edgeness, LinearizeDepth( avg_depth ) );
	diffuse.rgb = BRDF( position, normal, viewDir, albedo.rgb, rmac.x, rmac.y, rmac.z * 2.0 - 1.0, shadowlight, dlight_count, tile_index);

	diffuse.rgb = mix( diffuse.rgb, u_OutlineColor.rgb, edgeness );

	diffuse.rgb += Dither();
	diffuse.rgb = VoidFog( diffuse.rgb, position.z );
	diffuse.a = VoidFogAlpha( diffuse.a, position.z );

	f_Albedo = diffuse;
}

#endif
