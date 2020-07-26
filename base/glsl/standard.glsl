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
	v_Normal = mat3( u_M ) * Normal;
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

out vec4 f_Albedo;

uniform sampler2D u_BaseTexture;

#if APPLY_SOFT_PARTICLE
#include "include/softparticle.glsl"
uniform sampler2D u_DepthTexture;
#endif

#if APPLY_DECALS
layout( std140 ) uniform u_Decal {
	int u_NumDecals;
};

uniform samplerBuffer u_DecalData;
uniform isamplerBuffer u_DecalIndices;
uniform isamplerBuffer u_DecalTiles;
uniform sampler2DArray u_DecalAtlases;
#endif

float ProjectedScale( vec3 p, vec3 o, vec3 d ) {
	return dot( p - o, d ) / dot( d, d );
}

// must match the CPU OrthonormalBasis
void OrthonormalBasis( vec3 v, out vec3 tangent, out vec3 bitangent ) {
	float s = step( 0.0, v.z ) * 2.0 - 1.0;
	float a = -1.0 / ( s + v.z );
	float b = v.x * v.y * a;

	tangent = vec3( 1.0 + s * v.x * v.x * a, s * b, -s * v.x );
	bitangent = vec3( b, s + v.y * v.y * a, -v.y );
}

void main() {
#if APPLY_DRAWFLAT
	vec4 diffuse = vec4( 0.25, 0.25, 0.25, 1.0 );
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
	float tile_size = float( TILE_SIZE );
	int tile_row = int( ( u_ViewportSize.y - gl_FragCoord.y ) / tile_size );
	int tile_col = int( gl_FragCoord.x / tile_size );
	int cols = int( u_ViewportSize.x + tile_size - 1 ) / int( tile_size );
	int tile_index = tile_row * cols + tile_col;

	ivec2 tile = texelFetch( u_DecalTiles, tile_index ).xy;

	for( int i = 0; i < tile.y; i++ ) {
		int idx = texelFetch( u_DecalIndices, tile.x + i ).x;
		vec4 origin_radius = texelFetch( u_DecalData, idx * 4 + 0 );

		if( distance( origin_radius.xyz, v_Position ) < origin_radius.w ) {
			vec4 normal_angle = texelFetch( u_DecalData, idx * 4 + 1 );
			vec4 decal_color = texelFetch( u_DecalData, idx * 4 + 2 );
			vec4 uvwh = texelFetch( u_DecalData, idx * 4 + 3 );
			float layer = floor( uvwh.x );

			vec3 basis_u;
			vec3 basis_v;
			OrthonormalBasis( normal_angle.xyz, basis_u, basis_v );
			basis_u *= origin_radius.w * 2.0;
			basis_v *= origin_radius.w * -2.0;
			vec3 bottom_left = origin_radius.xyz - ( basis_u + basis_v ) * 0.5;

			float c = cos( normal_angle.w );
			float s = sin( normal_angle.w );
			mat2 rotation = mat2( c, s, -s, c );
			vec2 uv = vec2( ProjectedScale( v_Position, bottom_left, basis_u ), ProjectedScale( v_Position, bottom_left, basis_v ) );

			uv -= 0.5;
			uv = rotation * uv;
			uv += 0.5;
			uv = uvwh.xy + uvwh.zw * uv;

			vec4 sample = texture( u_DecalAtlases, vec3( uv, layer ) );
			float inv_cos_45_degrees = 1.41421356237;
			float decal_alpha = min( 1.0, sample.a * decal_color.a * max( 0.0, dot( v_Normal, normal_angle.xyz ) * inv_cos_45_degrees ) );
			diffuse.rgb = mix( diffuse.rgb, sample.rgb * decal_color.rgb, decal_alpha );
		}
	}
#endif

#if APPLY_FOG
	diffuse.rgb = Fog( diffuse.rgb, length( v_Position - u_CameraPos ) );
	diffuse.rgb += Dither();
#endif

	f_Albedo = LinearTosRGB( diffuse );
}

#endif
