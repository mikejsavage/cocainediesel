#include "include/common.glsl"
#include "include/uniforms.glsl"

qf_varying vec3 v_Position;
qf_varying vec2 v_TexCoord;

#if VERTEX_COLORS
qf_varying vec4 v_Color;
#endif

#if APPLY_DRAWFLAT
qf_varying vec3 v_Normal;
#endif

#if APPLY_SOFT_PARTICLE
qf_varying float v_Depth;
#endif

#if VERTEX_SHADER

in vec4 a_Position;
in vec3 a_Normal;
in vec4 a_Color;
in vec2 a_TexCoord;

#include "include/skinning.glsl"

void main() {
	vec4 Position = a_Position;
	vec3 Normal = a_Normal;
	vec2 TexCoord = a_TexCoord;

#ifdef SKINNED
	Skin( Position, Normal );
#endif

	v_Position = ( u_M * Position ).xyz;
	/* v_TexCoord = TextureMatrix2x3Mul( u_TextureMatrix, a_TexCoord ); */
	v_TexCoord = a_TexCoord;

#ifdef VERTEX_COLORS
	v_Color = sRGBToLinear( a_Color );
#endif

#if APPLY_DRAWFLAT
	v_Normal = mat3( u_M ) * Normal;
#endif

	gl_Position = u_P * u_V * u_M * Position;

#if APPLY_SOFT_PARTICLE
	vec4 modelPos = u_V * u_M * Position;
	v_Depth = -modelPos.z;
#endif
}

#else

out vec4 f_Albedo;

#include "include/dither.glsl"
#include "include/fog.glsl"

uniform sampler2D u_BaseTexture;

#ifdef APPLY_DRAWFLAT
uniform vec3 u_WallColor;
uniform vec3 u_FloorColor;
#endif

#if APPLY_SOFT_PARTICLE
#include "include/softparticle.glsl"
uniform sampler2D u_DepthTexture;
#endif

void main() {
#ifdef APPLY_DRAWFLAT
	/* float n = float( step( DRAWFLAT_NORMAL_STEP, abs( v_Normal.z ) ) ); */
	/* vec4 diffuse = vec4( vec3( mix( u_WallColor, u_FloorColor, n ) ), 1.0 ); */
	vec4 diffuse = vec4( NormalToRGB( normalize( v_Normal ) ), 1.0 );
#else
	vec4 color = sRGBToLinear( u_ModelColor );

#if VERTEX_COLORS
	color *= v_Color;
#endif

	vec4 diffuse = qf_texture( u_BaseTexture, v_TexCoord ) * color;
#endif

#if ALPHA_TEST
	if( diffuse.a < u_AlphaCutoff )
		discard;
#endif

#if APPLY_SOFT_PARTICLE
	float softness = FragmentSoftness( v_Depth, u_DepthTexture, gl_FragCoord.xy, u_NearClip );
	diffuse *= mix(vec4(1.0), vec4(softness), u_BlendMix.xxxy);
#endif

#ifdef APPLY_FOG
	diffuse.rgb = Fog( diffuse.rgb, length( v_Position - u_CameraPos ) );
	diffuse.rgb += Dither();
#endif

	f_Albedo = LinearTosRGB( diffuse );
}

#endif
