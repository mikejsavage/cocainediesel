#include "../../source/client/renderer/shader_shared.h"

void AddDynamicLights( uint count, int tile_index, float3 position, float3 normal, float3 viewDir, inout float3 lambertlight, inout float3 specularlight ) {
	for( uint i = 0; i < count; i++ ) {
		DynamicLight dlight = u_Dlights[ u_DlightTiles[ tile_index ].indices[ i ] ];

		float3 origin = floor( dlight.origin_color.xyz );
		float3 dlight_color = frac( dlight.origin_color.xyz ) / 0.9f;
		float radius = dlight.radius;

		float intensity = DLIGHT_CUTOFF * radius * radius;
		float dist = distance( position, origin );
		float3 lightdir = normalize( position - origin - normal ); // - normal to prevent 0,0,0

		float dlight_intensity = max( 0.0f, intensity / ( dist * dist ) - DLIGHT_CUTOFF );

		lambertlight += dlight_color * dlight_intensity * max( 0.0f, LambertLight( normal, -lightdir ) );
		specularlight += dlight_color * max( 0.0f, 1.0f - dist / radius ) * SpecularLight( normal, lightdir, viewDir, u_Shininess ) * u_Specular;
	}
}
