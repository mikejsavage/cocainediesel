#include "include/uniforms.glsl"
#include "include/common.glsl"
#include "include/skinning.glsl"
#include "include/dither.glsl"
#include "include/fog.glsl"

v2f vec3 v_Position;
v2f vec3 v_Normal;
v2f vec2 v_TexCoord;
v2f vec4 v_NearShadowmapPosition;
v2f vec4 v_FarShadowmapPosition;

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
	v_NearShadowmapPosition = u_NearWorldToShadowmap * vec4( v_Position, 1.0 );
	v_FarShadowmapPosition = u_FarWorldToShadowmap * vec4( v_Position, 1.0 );

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
uniform sampler2D u_NearShadowmapTexture;
uniform sampler2D u_FarShadowmapTexture;

#if APPLY_SOFT_PARTICLE
#include "include/softparticle.glsl"
uniform sampler2D u_DepthTexture;
#endif

#if APPLY_DECALS
layout( std140 ) uniform u_Decal {
	int u_NumDecals;
};

uniform samplerBuffer u_DecalData;
uniform isamplerBuffer u_DecalCount;
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

vec2 ShadowOffsets( vec3 N, vec3 L ) {
	float cos_alpha = clamp( dot( N, L ), 0.0, 1.0 );
	float offset_scale_N = sqrt( 1.0 - cos_alpha * cos_alpha );
	float offset_scale_L = offset_scale_N / cos_alpha;
	return vec2( offset_scale_N, min( 5.0, offset_scale_L ) );
}

float GetLight( vec4 shadow_pos, vec3 normal, vec3 lightdir, sampler2D shadowmap, int pcfCount ) {
	vec3 light_ndc = shadow_pos.xyz / shadow_pos.w;
	vec3 light_norm = light_ndc * 0.5 + 0.5;
	if ( clamp( light_norm.xy, 0.0, 1.0 ) != light_norm.xy ) {
		return 0.0;
	}
	vec2 offsets = ShadowOffsets( normal, lightdir );
	float bias = 2e-6 + 1e-6 * offsets.x + 5e-6 * offsets.y;

	float shadow = 0.0;
	vec2 inv_shadowmap_size = 1.0 / textureSize( shadowmap, 0 );
	float pcfTotal = ( pcfCount * 2.0 + 1.0 ) * ( pcfCount * 2.0 + 1.0 );
	for ( int x = -pcfCount; x <= pcfCount; x++ ) {
		for ( int y = -pcfCount; y <= pcfCount; y++ ) {
			vec2 offset = vec2( x, y ) * inv_shadowmap_size;
			float shadow_depth = texture( shadowmap, light_norm.xy + offset ).r;
			shadow += step( light_norm.z - shadow_depth, bias );
			// shadow += smoothstep( bias, 0.0, light_norm.z - shadow_depth );
		}
	}
	return shadow / pcfTotal;
}

void main() {
#if APPLY_DRAWFLAT
	vec4 diffuse = vec4( 0.17, 0.17, 0.17, 1.0 );
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

	int count = texelFetch( u_DecalCount, tile_index ).x;

	float accumulated_alpha = 1.0;
	vec3 accumulated_color = vec3( 0.0 );

	for( int i = 0; i < count; i++ ) {
		if( accumulated_alpha < 0.001 ) {
			accumulated_alpha = 0.0;
			break;
		}

		int idx = tile_index * 100 + i * 2; // NOTE(msc): 100 = 2 * MAX_DECALS_PER_TILE

		vec4 data1 = texelFetch( u_DecalData, idx + 0 );
		vec3 origin = floor( data1.xyz );
		float radius = floor( data1.w );
		vec3 normal = ( fract( data1.xyz ) - 0.5 ) / 0.49;
		float angle = fract( data1.w ) * M_PI * 2.0;

		vec4 data2 = texelFetch( u_DecalData, idx + 1 );
		vec4 decal_color = vec4( floor( data2.yzw ) / 255.0, 1.0 );
		vec4 uvwh = vec4( data2.x, fract( data2.yzw ) );
		float layer = floor( uvwh.x );

		if( distance( origin, v_Position ) < radius ) {
			vec3 basis_u;
			vec3 basis_v;
			OrthonormalBasis( normal, basis_u, basis_v );
			basis_u *= radius * 2.0;
			basis_v *= radius * -2.0;
			vec3 bottom_left = origin - ( basis_u + basis_v ) * 0.5;

			float c = cos( angle );
			float s = sin( angle );
			mat2 rotation = mat2( c, s, -s, c );
			vec2 uv = vec2( ProjectedScale( v_Position, bottom_left, basis_u ), ProjectedScale( v_Position, bottom_left, basis_v ) );

			uv -= 0.5;
			uv = rotation * uv;
			uv += 0.5;
			uv = uvwh.xy + uvwh.zw * uv;

			float alpha = texture( u_DecalAtlases, vec3( uv, layer ) ).r;
			float inv_cos_45_degrees = 1.41421356237;
			float decal_alpha = min( 1.0, alpha * decal_color.a * max( 0.0, dot( v_Normal, normal ) * inv_cos_45_degrees ) );
			accumulated_color += decal_color.rgb * decal_alpha * accumulated_alpha;
			accumulated_alpha *= 1.0 - decal_alpha;
		}
	}

	diffuse.rgb = diffuse.rgb * accumulated_alpha + accumulated_color;
#endif

#if APPLY_FOG
	float lambertlight = dot( v_Normal, -u_LightDir ) * 0.5 + 0.5;
	diffuse.rgb *= lambertlight * 0.5 + 0.5;

	float light = 0.0;
	vec3 light_ndc = v_NearShadowmapPosition.xyz / v_NearShadowmapPosition.w;
	vec3 light_norm = light_ndc * 0.5 + 0.5;
	if ( light_norm.z > 1.0 || abs( dot( v_Normal, u_LightDir ) ) < 0.1 ) {
		light = 0.0;
	} else {
		float near_far_fract = smoothstep( 0.8, 1.0, length( v_NearShadowmapPosition.xy ) );
		float near_light = 0.0;
		if ( near_far_fract < 1.0 ) {
			near_light = GetLight( v_NearShadowmapPosition, v_Normal, u_LightDir, u_NearShadowmapTexture, 1 );
		}
		float far_light = GetLight( v_FarShadowmapPosition, v_Normal, u_LightDir, u_FarShadowmapTexture, 1 );
		light = mix( near_light, far_light, near_far_fract );
	}
	diffuse.rgb *= 0.5 + light * 0.5;

	diffuse.rgb = Fog( diffuse.rgb, length( v_Position - u_CameraPos ) );
	diffuse.rgb += Dither();
#endif

	diffuse.rgb = VoidFog( diffuse.rgb, v_Position.z );
	diffuse.a = VoidFogAlpha( diffuse.a, v_Position.z );

	f_Albedo = LinearTosRGB( diffuse );
}

#endif
