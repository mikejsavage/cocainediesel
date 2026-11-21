#pragma once

#ifdef APPLY_DYNAMICS

#include "../shader_shared.h"

struct Quaternion {
	float4 q;
};

void QuaternionToBasis( Quaternion q, out float3 normal, out float3 tangent, out float3 bitangent ) {
	normal = float3(
		1.0 - 2.0 * ( q.q.y * q.q.y + q.q.z * q.q.z ),
		2.0 * ( q.q.x * q.q.y + q.q.z * q.q.w ),
		2.0 * ( q.q.x * q.q.z - q.q.y * q.q.w )
	);

	tangent = float3(
		2.0 * ( q.q.x * q.q.y - q.q.z * q.q.w ),
		1.0 - 2.0 * ( q.q.x * q.q.x + q.q.z * q.q.z ),
		2.0 * ( q.q.y * q.q.z + q.q.x * q.q.w )
	);

	bitangent = float3(
		2.0 * ( q.q.x * q.q.z + q.q.y * q.q.w ),
		2.0 * ( q.q.y * q.q.z - q.q.x * q.q.w ),
		1.0 - 2.0 * ( q.q.x * q.q.x + q.q.y * q.q.y )
	);
}

float ProjectedScale( float3 p, float3 o, float3 d ) {
	return dot( p - o, d ) / dot( d, d );
}

float2 DecalUV( Decal decal, float3 pos, float3 bottom_left, float3 basis_u, float3 basis_v ) {
	float2 local_uv = float2( ProjectedScale( pos, bottom_left, basis_u ), ProjectedScale( pos, bottom_left, basis_v ) );
	return Dequantize01( decal.uv ) + Dequantize01( decal.wh ) * local_uv;
}

float3x3 float3x4_to_3x3( float3x4 m ) {
	return float3x3( m[ 0 ].xyz, m[ 1 ].xyz, m[ 2 ].xyz );
}

void AddDecals( float3 vertex_position, uint count, int tile_index, inout float4 diffuse, inout float3 surface_normal ) {
	float accumulated_alpha = 1.0f;
	float3 accumulated_color = 0.0f;
	float accumulated_height = 0.0f;

	float3 dPos_dx = ddx( vertex_position );
	float3 dPos_dy = ddy( vertex_position );

	for( uint i = 0; i < count; i++ ) {
		if( accumulated_alpha < 0.001f ) {
			accumulated_alpha = 0.0f;
			break;
		}

		uint idx = count - i - 1;
		Decal decal = u_Decals[ u_DecalTiles[ tile_index ].indices[ idx ] ];

		if( distance( decal.origin, vertex_position ) < decal.radius ) {
			Quaternion orientation = { Dequantize11( decal.orientation ) };
			float3 decal_normal, tangent, bitangent;
			QuaternionToBasis( orientation, decal_normal, tangent, bitangent );

			tangent *= decal.radius * 2.0f;
			bitangent *= decal.radius * -2.0f; // negate because uvs are y-down

			float3 bottom_left = decal.origin - ( tangent + bitangent ) * 0.5f;

			// manually compute UV derivatives because auto derivatives are undefined inside incoherent branches
			float2 uv = DecalUV( decal, vertex_position, bottom_left, tangent, bitangent );
			float2 dUV_dx = DecalUV( decal, vertex_position + dPos_dx, bottom_left, tangent, bitangent ) - uv;
			float2 dUV_dy = DecalUV( decal, vertex_position + dPos_dy, bottom_left, tangent, bitangent ) - uv;

			static const float lodbias = 0.5f; // 2^-1
			dUV_dx *= lodbias;
			dUV_dy *= lodbias;

			float alpha = u_SpriteAtlas.SampleGrad( u_StandardSampler, float3( uv, decal.layer ), dUV_dx, dUV_dy ).a;
			float inv_cos_45_degrees = 1.41421356237f;
			float decal_alpha = min( 1.0f, alpha * max( 0.0f, dot( decal_normal, surface_normal ) * inv_cos_45_degrees ) );
			accumulated_color += sRGBToLinear( decal.color ).xyz * decal_alpha * accumulated_alpha;
			accumulated_alpha *= 1.0f - decal_alpha;
			accumulated_height += decal.height * decal_alpha;
		}
	}

	float3 decal_normal = float3( ddx( accumulated_alpha ), ddy( accumulated_alpha ), 0.0f ) * accumulated_height;
	decal_normal = mul( Adjugate( u_View[ 0 ].inverse_V ), decal_normal );
	surface_normal = normalize( surface_normal + decal_normal );

	diffuse.rgb = diffuse.rgb * accumulated_alpha + accumulated_color;
}

#endif // #ifdef APPLY_DYNAMICS
