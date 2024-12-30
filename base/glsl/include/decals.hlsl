#include "../../source/client/renderer/shader_shared.h"

float ProjectedScale( float3 p, float3 o, float3 d ) {
	return dot( p - o, d ) / dot( d, d );
}

float2 DecalUV( float4 uvwh, float3 pos, float3 bottom_left, float3 basis_u, float3 basis_v ) {
	float2 uv = float2( ProjectedScale( pos, bottom_left, basis_u ), ProjectedScale( pos, bottom_left, basis_v ) );
	uv = uvwh.xy + uvwh.zw * uv;
	return uv;
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

		float3 origin = floor( decal.origin_orientation_xyz );
		float radius = floor( decal.radius_orientation_w );
		Quaternion orientation = {
			float4(
				( frac( decal.origin_orientation_xyz ) - 0.5f ) / 0.49f,
				( frac( decal.radius_orientation_w ) - 0.5f ) / 0.49f
			)
		};
		float4 decal_color = float4( frac( floor( decal.color_uvwh_height.yzw ) / 256.0f ), 1.0f );
		float decal_height = floor( decal.color_uvwh_height.y / 256.0f );
		float4 uvwh = float4( decal.color_uvwh_height.x, frac( decal.color_uvwh_height.yzw ) );
		float layer = floor( decal.color_uvwh_height.x );

		if( distance( origin, vertex_position ) < radius ) {
			float3 decal_normal, tangent, bitangent;
			QuaternionToBasis( orientation, decal_normal, tangent, bitangent );

			tangent *= radius * 2.0f;
			bitangent *= radius * -2.0f; // negate because uvs are y-down

			float3 bottom_left = origin - ( tangent + bitangent ) * 0.5;

			// manually compute UV derivatives because auto derivatives are undefined inside incoherent branches
			float2 uv = DecalUV( uvwh, vertex_position, bottom_left, tangent, bitangent );
			float2 dUV_dx = DecalUV( uvwh, vertex_position + dPos_dx, bottom_left, tangent, bitangent ) - uv;
			float2 dUV_dy = DecalUV( uvwh, vertex_position + dPos_dy, bottom_left, tangent, bitangent ) - uv;

			float alpha = u_DecalAtlases.SampleGrad( float3( uv, layer ), dUV_dx, dUV_dy );
			float inv_cos_45_degrees = 1.41421356237f;
			float decal_alpha = min( 1.0f, alpha * decal_color.a * max( 0.0f, dot( decal_normal, surface_normal ) * inv_cos_45_degrees ) );
			accumulated_color += decal_color.rgb * decal_alpha * accumulated_alpha;
			accumulated_alpha *= 1.0f - decal_alpha;
			accumulated_height += decal_height * decal_alpha;
		}
	}

	float3 decal_normal = float3( ddx( accumulated_alpha ), ddy( accumulated_alpha ), 0.0f ) * accumulated_height;
	decal_normal = float3x3( AffineToMat4( u_InverseV ) ) * decal_normal;
	surface_normal = normalize( surface_normal + decal_normal );

	diffuse.rgb = diffuse.rgb * accumulated_alpha + accumulated_color;
}
