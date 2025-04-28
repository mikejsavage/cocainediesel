#pragma once

#ifdef APPLY_DYNAMICS

#include "../../source/client/renderer/shader_shared.h"

void AddLights( uint count, int tile_index, float3 position, float3 normal, float3 viewDir, inout float3 lambertlight, inout float3 specularlight ) {
	for( uint i = 0; i < count; i++ ) {
		Light light = u_Lights[ u_LightTiles[ tile_index ].indices[ i ] ];

		float3 origin = floor( light.origin_color.xyz );
		float3 color = frac( light.origin_color.xyz ) / 0.9f;
		float radius = light.radius;

		float intensity = LIGHT_CUTOFF * radius * radius;
		float dist = distance( position, origin );
		float3 lightdir = normalize( position - origin - normal ); // - normal to prevent 0,0,0

		float light_intensity = max( 0.0f, intensity / ( dist * dist ) - LIGHT_CUTOFF );

		lambertlight += color * light_intensity * max( 0.0f, LambertLight( normal, -lightdir ) );
		specularlight += color * max( 0.0f, 1.0f - dist / radius ) * SpecularLight( normal, lightdir, viewDir, u_MaterialProperties[ 0 ].shininess ) * u_MaterialProperties[ 0 ].specular;
	}
}

#endif // #ifdef APPLY_DYNAMICS
