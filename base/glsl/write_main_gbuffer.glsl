#include "include/uniforms.glsl"
#include "include/common.glsl"
#include "include/skinning.glsl"

v2f vec3 v_Position;
v2f vec3 v_Normal;
v2f vec2 v_TexCoord;

#if VERTEX_COLORS
v2f vec4 v_Color;
#endif

#if VERTEX_SHADER

in vec4 a_Position;
in vec3 a_Normal;
in vec4 a_Color;
in vec2 a_TexCoord;

vec2 ApplyTCMod( vec2 uv ) {
	mat3x2 m = transpose( mat2x3( u_TextureMatrix[ 0 ], u_TextureMatrix[ 1 ] ) );
	return ( m * vec3( uv, 1.0 ) ).xy;
}

void main() {
	vec4 Position = a_Position;
	vec3 Normal = a_Normal;
	vec2 TexCoord = a_TexCoord;

#if SKINNED
	Skin( Position, Normal );
#endif

	v_Position = ( u_M * Position ).xyz;

	mat3 m = transpose( inverse( mat3( u_M ) ) );
	v_Normal = m * Normal;

	v_TexCoord = ApplyTCMod( a_TexCoord );

#if VERTEX_COLORS
	v_Color = sRGBToLinear( a_Color );
#endif

	gl_Position = u_P * u_V * u_M * Position;
}

#else

layout( location = 0 ) out vec4 f_Albedo;
layout( location = 1 ) out vec4 f_RMAC;
layout( location = 2 ) out vec2 f_Normal;

uniform sampler2D u_BaseTexture;

#if APPLY_DECALS
uniform isamplerBuffer u_DynamicCount;
#include "include/decals.glsl"
#endif

void main() {
	vec3 normal = normalize( v_Normal );
	float curvature = step( 0.0000004, length( fwidth( normal ) ) );
#if APPLY_DRAWFLAT
	vec4 diffuse = u_MaterialColor;
#else
	vec4 color = u_MaterialColor;

#if VERTEX_COLORS
	color *= v_Color;
#endif

	vec4 diffuse = texture( u_BaseTexture, v_TexCoord ) * color;
#endif

#if ALPHA_TEST
	if( diffuse.a < u_AlphaCutoff )
		discard;
#endif

#if APPLY_DECALS
	float tile_size = float( TILE_SIZE );
	int tile_row = int( ( u_ViewportSize.y - gl_FragCoord.y ) / tile_size );
	int tile_col = int( gl_FragCoord.x / tile_size );
	int cols = int( u_ViewportSize.x + tile_size - 1 ) / int( tile_size );
	int tile_index = tile_row * cols + tile_col;
	int decal_count = texelFetch( u_DynamicCount, tile_index ).x;
	applyDecals( decal_count, tile_index, diffuse, normal );
#endif

	f_Albedo = diffuse;
	f_RMAC = vec4( u_Roughness, u_Metallic, u_Anisotropic * 0.5 + 0.5, curvature );
	f_Normal = CompressNormal( normal );
}

#endif
