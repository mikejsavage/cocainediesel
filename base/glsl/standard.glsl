#include "include/uniforms.glsl"
#include "include/common.glsl"
#include "include/skinning.glsl"
#include "include/dither.glsl"
#include "include/fog.glsl"
#include "include/lighting.glsl"

v2f vec3 v_Position;
v2f vec3 v_Normal;
v2f vec2 v_TexCoord;

#ifdef INSTANCED
struct Instance {
	AffineTransform transform;
	vec4 color;
	vec3 texture_matrix[ 2 ];
};

layout( std430 ) readonly buffer b_Instances {
	Instance instances[];
};

v2f flat int v_Instance;
#endif

layout( constant_id = 0 ) const bool VertexColors = false;

v2f vec4 v_Color;

#if VERTEX_SHADER

in vec4 a_Position;
in vec3 a_Normal;
in vec4 a_Color;
in vec2 a_TexCoord;

vec2 ApplyTCMod( vec2 uv ) {
#if INSTANCED
	mat3x2 m = transpose( mat2x3( instances[ gl_InstanceIndex ].texture_matrix[ 0 ], instances[ gl_InstanceIndex ].texture_matrix[ 1 ] ) );
#else
	mat3x2 m = transpose( mat2x3( u_TextureMatrix[ 0 ], u_TextureMatrix[ 1 ] ) );
#endif
	return ( m * vec3( uv, 1.0 ) ).xy;
}

void main() {
#if INSTANCED
	mat4 u_M = AffineToMat4( instances[ gl_InstanceIndex ].transform );
	v_Instance = gl_InstanceIndex;
#endif
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

	if( VertexColors ) {
		v_Color = sRGBToLinear( a_Color );
	}

	gl_Position = u_P * u_V * u_M * Position;
}

#else

out vec4 f_Albedo;
out uint f_Mask;

const uint MASK_CURVED = 1u;

/*layout( set = DescriptorSet_Material )*/ uniform sampler2D u_BaseTexture;

#if APPLY_DECALS || APPLY_DLIGHTS
struct DynamicTile {
	uint num_decals;
	uint num_dlights;
};

layout( std430/*, set = DescriptorSet_RenderPass*/ ) readonly buffer b_DynamicTiles {
	DynamicTile dynamic_tiles[];
};
#endif

#if APPLY_DECALS
#include "include/decals.glsl"
#endif

#if APPLY_DLIGHTS
#include "include/dlights.glsl"
#endif

#if APPLY_SHADOWS
#include "include/shadow.glsl"
#endif

void main() {
	vec3 normal = normalize( v_Normal );
	f_Mask = length( fwidth( normal ) ) < 0.000001 ? 0u : MASK_CURVED;
#if APPLY_DRAWFLAT
#if INSTANCED
	vec4 diffuse = instances[ v_Instance ].color;
#else
	vec4 diffuse = u_MaterialColor;
#endif
#else
#if INSTANCED
	vec4 color = instances[ v_Instance ].color;
#else
	vec4 color = u_MaterialColor;
#endif

#if VERTEX_COLORS
	color *= v_Color;
#endif

	vec4 diffuse = texture( u_BaseTexture, v_TexCoord ) * color;
#endif

#if ALPHA_TEST
	if( diffuse.a < u_AlphaCutoff )
		discard;
#endif

#if APPLY_DECALS || APPLY_DLIGHTS
	float tile_size = float( FORWARD_PLUS_TILE_SIZE );
	int tile_row = int( ( u_ViewportSize.y - gl_FragCoord.y - 1.0 ) / tile_size );
	int tile_col = int( gl_FragCoord.x / tile_size );
	int cols = int( u_ViewportSize.x + tile_size - 1 ) / int( tile_size );
	int tile_index = tile_row * cols + tile_col;
	DynamicTile dynamic_tile = dynamic_tiles[ tile_index ];
#endif

#if APPLY_DECALS
	ApplyDecals( dynamic_tile.num_decals, tile_index, diffuse, normal );
#endif

#if SHADED
	const vec3 suncolor = vec3( 1.0 );

	vec3 viewDir = normalize( u_CameraPos - v_Position );

	vec3 lambertlight = suncolor * LambertLight( normal, -u_LightDir );
	vec3 specularlight = suncolor * SpecularLight( normal, u_LightDir, viewDir, u_Shininess ) * u_Specular;

	float shadowlight = 1.0;
	#if APPLY_SHADOWS
		shadowlight = GetLight( normal );
		specularlight = specularlight * shadowlight;
	#endif
	shadowlight = shadowlight * 0.5 + 0.5;

#if APPLY_DLIGHTS
	applyDynamicLights( dynamic_tile.num_dlights, tile_index, v_Position, normal, viewDir, lambertlight, specularlight );
#endif
	lambertlight = lambertlight * 0.5 + 0.5;

	#if APPLY_DRAWFLAT
		lambertlight = lambertlight * 0.5 + 0.5;
	#endif

	diffuse.rgb *= shadowlight * ( lambertlight + specularlight );

#endif

#if APPLY_FOG
	diffuse.rgb = Fog( diffuse.rgb, length( v_Position - u_CameraPos ) );
	diffuse.rgb += Dither();
#endif

	diffuse.rgb = VoidFog( diffuse.rgb, v_Position.z );
	diffuse.a = VoidFogAlpha( diffuse.a, v_Position.z );

	f_Albedo = diffuse;
}

#endif
