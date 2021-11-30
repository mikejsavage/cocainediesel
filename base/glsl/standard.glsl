#include "include/uniforms.glsl"
#include "include/common.glsl"
#include "include/skinning.glsl"
#include "include/dither.glsl"
#include "include/fog.glsl"

v2f vec3 v_Position;
v2f vec3 v_Normal;
v2f vec2 v_TexCoord;

#if VERTEX_COLORS
v2f vec4 v_Color;
#endif

#if APPLY_SOFT_PARTICLE
v2f float v_Depth;
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

#if APPLY_SOFT_PARTICLE
	vec4 modelPos = u_V * u_M * Position;
	v_Depth = -modelPos.z;
#endif
}

#else

#if TRANSPARENT
#include "include/oit.glsl"
#else
out vec4 f_Albedo;
#endif

#include "include/lighting.glsl"

uniform sampler2D u_BaseTexture;

#if APPLY_SOFT_PARTICLE
#include "include/softparticle.glsl"
uniform sampler2D u_DepthTexture;
#endif

#if APPLY_DECALS || APPLY_DLIGHTS
uniform isamplerBuffer u_DynamicCount;
#endif

#if APPLY_DECALS
#include "include/decals.glsl"
#endif

#if APPLY_SHADOWS
#include "include/shadow.glsl"
#endif

void main() {
	vec3 normal = normalize( v_Normal );
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

#if APPLY_SOFT_PARTICLE
	float softness = FragmentSoftness( v_Depth, u_DepthTexture, gl_FragCoord.xy, u_NearClip );
	diffuse *= mix(vec4(1.0), vec4(softness), u_BlendMix.xxxy);
#endif

#if APPLY_DECALS || APPLY_DLIGHTS
	float tile_size = float( TILE_SIZE );
	int tile_row = int( ( u_ViewportSize.y - gl_FragCoord.y ) / tile_size );
	int tile_col = int( gl_FragCoord.x / tile_size );
	int cols = int( u_ViewportSize.x + tile_size - 1 ) / int( tile_size );
	int tile_index = tile_row * cols + tile_col;
	ivec2 decal_dlight_count = texelFetch( u_DynamicCount, tile_index ).xy;
#endif

#if APPLY_DECALS
	applyDecals( decal_dlight_count.x, tile_index, diffuse, normal );
#endif

#if APPLY_FOG
	diffuse.rgb = Fog( diffuse.rgb, length( v_Position - u_CameraPos ) );
#endif

#if SHADED
	const vec3 suncolor = vec3( 1.0 );

	vec3 viewDir = normalize( u_CameraPos - v_Position );

	float shadowlight = 1.0;
	#if APPLY_SHADOWS
		shadowlight = GetLight( normal );
	#endif

	diffuse.rgb = BRDF( normal, viewDir, -u_LightDir, suncolor, diffuse.rgb, u_Roughness, u_Metallic, u_Anisotropic, shadowlight, decal_dlight_count.y, tile_index );
#endif

#if APPLY_FOG
	diffuse.rgb += Dither();
#endif

	diffuse.rgb = VoidFog( diffuse.rgb, v_Position.z );
	diffuse.a = VoidFogAlpha( diffuse.a, v_Position.z );

#if TRANSPARENT
	WritePixel( diffuse.rgb, diffuse.a );
#else
	f_Albedo = diffuse;
#endif
}

#endif
