#include "../../source/client/renderer/shader_shared.h"

[[vk::binding( 0 )]] StructuredBuffer< ViewUniforms > u_View;
[[vk::binding( 1 )]] StructuredBuffer< TileCullingInputs > u_TileCulling;
[[vk::binding( 2 )]] StructuredBuffer< Decal > u_Decals;
[[vk::binding( 3 )]] StructuredBuffer< DynamicLight > u_Dlights;
[[vk::binding( 4 )]] RWStructuredBuffer< TileIndices > u_DecalTiles;
[[vk::binding( 5 )]] RWStructuredBuffer< TileIndices > u_DlightTiles;
[[vk::binding( 6 )]] RWStructuredBuffer< TileCountsUniforms > u_TileCounts;

bool SphereCone( float3 sphere_origin, float sphere_radius, float3 cone_origin, float3 cone_axis, float cone_tan ) {
	float3 cone_to_sphere = sphere_origin - cone_origin;
	float t = dot( cone_to_sphere, cone_axis );
	float3 cone_slant = normalize( cone_axis + normalize( cone_to_sphere - t * cone_axis ) * cone_tan );
	t = dot( cone_to_sphere, cone_slant );
	t = max( t, 0.0f );
	float3 point_on_cone = cone_origin + t * cone_slant;

	float3 cone_surface_to_sphere = sphere_origin - point_on_cone;
	float signed_dist = length( cone_surface_to_sphere ) * sign( dot( cone_slant - cone_axis, cone_surface_to_sphere ) );

	return signed_dist < sphere_radius;
}

bool SphereInTile( float3 origin, float radius, float3 tile_direction, float cone_tan ) {
	return SphereCone( origin, radius, u_View[ 0 ].camera_pos, tile_direction, cone_tan );
}

void CullTile( uint2 tile ) {
	float2 tile_min = FORWARD_PLUS_TILE_SIZE * ( tile.xy ) / u_View[ 0 ].viewport_size * 2.0f - 1.0f;
	float2 tile_max = FORWARD_PLUS_TILE_SIZE * ( tile.xy + 1 ) / u_View[ 0 ].viewport_size * 2.0f - 1.0f;
	// opengl y flip
	tile_min.y = -tile_min.y;
	tile_max.y = -tile_max.y;

	float3 tile_vmin = mul( u_View[ 0 ].inverse_V, mul( u_View[ 0 ].inverse_P, float4( tile_min, 1.0f, 1.0f ) ) );
	float3 tile_vmax = mul( u_View[ 0 ].inverse_V, mul( u_View[ 0 ].inverse_P, float4( tile_max, 1.0f, 1.0f ) ) );
	float cone_tan = distance( tile_vmin, tile_vmax ) * 0.5;
	float3 tile_direction = normalize( ( tile_vmin + tile_vmax ) * 0.5f );

	uint col = tile.x;
	uint row = tile.y;
	uint tile_idx = row * u_TileCulling[ 0 ].rows + col;

	{
		TileIndices decal_tile;
		uint decal_count = 0;
		for( uint32_t i = 0; i < u_TileCulling[ 0 ].num_decals; i++ ) {
			if( decal_count == FORWARD_PLUS_TILE_CAPACITY )
				break;

			Decal decal = u_Decals[ i ];
			float3 origin = floor( decal.origin_orientation_xyz );
			float radius = floor( decal.radius_orientation_w );
			if( SphereInTile( origin, radius, tile_direction, cone_tan ) ) {
				decal_tile.indices[ decal_count ] = i;
				decal_count++;
			}
		}

		u_DecalTiles[ tile_idx ] = decal_tile;
		u_TileCounts[ tile_idx ].num_decals = decal_count;
	}

	{
		TileIndices dlight_tile;
		uint dlight_count = 0;
		for( uint32_t i = 0; i < u_TileCulling[ 0 ].num_dlights; i++ ) {
			if( dlight_count == FORWARD_PLUS_TILE_CAPACITY )
				break;

			DynamicLight dlight = u_Dlights[ i ];
			float3 origin = floor( dlight.origin_color.xyz );
			float radius = dlight.radius;
			if( SphereInTile( origin, radius, tile_direction, cone_tan ) ) {
				dlight_tile.indices[ dlight_count ] = i;
				dlight_count++;
			}
		}

		u_DlightTiles[ tile_idx ] = dlight_tile;
		u_TileCounts[ tile_idx ].num_dlights = dlight_count;
	}
}

[numthreads( PARTICLE_THREADGROUP_SIZE, 1, 1 )]
void main( uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex ) {
	uint tile_idx = DTid.x;
	if( tile_idx > u_TileCulling[ 0 ].cols * u_TileCulling[ 0 ].rows )
		return;
	uint col = tile_idx % u_TileCulling[ 0 ].rows;
	uint row = tile_idx / u_TileCulling[ 0 ].rows;
	CullTile( uint2( col, row ) );
}
