#include "include/uniforms.glsl"
#include "include/common.glsl"
#include "include/skinning.glsl"
#include "include/dither.glsl"
#include "include/fog.glsl"
#include "include/lighting.glsl"

v2f vec3 v_Position;
v2f vec3 v_Normal;
v2f vec2 v_TexCoord;
#if APPLY_SHADOWS
v2f vec4 v_NearShadowmapPosition;
v2f vec4 v_FarShadowmapPosition;
#endif

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

	// TODO: this fixes models that don't have 1,1,1 scale
	mat3 m = mat3( u_M );
	m = mat3( normalize( m[ 0 ] ), normalize( m[ 1 ] ), normalize( m[ 2 ] ) );
	m *= sign( determinant( m ) );
	v_Normal = m * Normal;

	v_TexCoord = ApplyTCMod( a_TexCoord );
#if APPLY_SHADOWS
	v_NearShadowmapPosition = u_NearWorldToShadowmap * vec4( v_Position, 1.0 );
	v_FarShadowmapPosition = u_FarWorldToShadowmap * vec4( v_Position, 1.0 );
#endif

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

out vec4 f_Albedo;

uniform sampler2D u_BaseTexture;

#if APPLY_SOFT_PARTICLE
#include "include/softparticle.glsl"
uniform sampler2D u_DepthTexture;
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

#if APPLY_DECALS
	applyDecals( diffuse, normal );
#endif

#if SHADED
	float lambertlight = LambertLight( normal, -u_LightDir ) * 0.5 + 0.5;
	#if APPLY_DRAWFLAT
		lambertlight = lambertlight * 0.5 + 0.5;
	#endif

	vec3 viewDir = normalize( u_CameraPos - v_Position );
	float specularlight = SpecularLight( normal, u_LightDir, viewDir, u_Shininess ) * u_Specular;

	float shadowlight = 1.0;
	#if APPLY_SHADOWS
		shadowlight = GetLight( normal );
		specularlight = specularlight * shadowlight;
	#endif
	shadowlight = shadowlight * 0.5 + 0.5;
	diffuse.rgb *= shadowlight * ( lambertlight + specularlight );

#endif

#if APPLY_FOG
	diffuse.rgb = Fog( diffuse.rgb, length( v_Position - u_CameraPos ) );
	diffuse.rgb += Dither();
#endif

	diffuse.rgb = VoidFog( diffuse.rgb, v_Position.z );
	diffuse.a = VoidFogAlpha( diffuse.a, v_Position.z );

	f_Albedo = LinearTosRGB( diffuse );
}

#endif
