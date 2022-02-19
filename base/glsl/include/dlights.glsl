struct DynamicLightTile {
	uint indices[ 50 ]; // NOTE(msc): 50 = MAX_DLIGHTS_PER_TILE
};

layout( std430 ) readonly buffer b_DynamicLightTiles {
	DynamicLightTile dlight_tiles[];
};

struct DynamicLight {
	vec3 origin_color;
	float radius;
};

layout( std430 ) readonly buffer b_DynamicLights {
	DynamicLight dlights[];
};

void applyDynamicLights( uint count, int tile_index, vec3 position, vec3 normal, vec3 viewDir, inout vec3 lambertlight, inout vec3 specularlight ) {
	for( uint i = 0; i < count; i++ ) {
		DynamicLight dlight = dlights[ dlight_tiles[ tile_index ].indices[ i ] ];

		vec3 origin = floor( dlight.origin_color.xyz );
		vec3 dlight_color = fract( dlight.origin_color.xyz ) / 0.9;
		float radius = dlight.radius;

		float intensity = DLIGHT_CUTOFF * radius * radius;
		float dist = distance( position, origin );
		vec3 lightdir = normalize( position - origin - normal ); // - normal to prevent 0,0,0

		float dlight_intensity = max( 0.0, intensity / ( dist * dist ) - DLIGHT_CUTOFF );

		lambertlight += dlight_color * dlight_intensity * max( 0.0, LambertLight( normal, -lightdir ) );
		specularlight += dlight_color * max( 0.0, 1.0 - dist / radius ) * SpecularLight( normal, lightdir, viewDir, u_Shininess ) * u_Specular;
	}
}
