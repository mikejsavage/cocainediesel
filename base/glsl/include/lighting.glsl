float DistributionGGXAniso( float ToH, float BoH, float NoH, vec2 a ) {
	float a2 = a.x * a.y;
	vec3 d = vec3( a.y * ToH, a.x * BoH, a2 * NoH );
	float d2 = dot( d, d );
	float b2 = a2 / d2;
	return a2 * b2 * b2 / M_PI;
}

float GeometryDistributionGGXAniso( float ToV, float BoV, float ToL, float BoL, float NoV, float NoL, vec2 a ) {
	float lambdaV = NoL * length( vec3( a.x * ToV, a.y * BoV, NoV ) );
	float lambdaL = NoV * length( vec3( a.x * ToL, a.y * BoL, NoL ) );
	float g = 0.5 / ( lambdaV + lambdaL );
	return min( g, 1.0 );
}

vec3 FresnelSchlick( vec3 f0, float LoH ) {
	return f0 + ( 1.0 - f0 ) * pow( 1.0 - LoH, 5.0 );
}

float FresnelSchlick( float f0, float f90, float LoH ) {
	return f0 + ( f90 - f0 ) * pow( 1.0 - LoH, 5.0 );
}

float Diffuse_Burley( float NoV, float NoL, float LoH, float roughness ) {
	float f90 = 0.5 + 2.0 * roughness * LoH * LoH;
	float light_scatter = FresnelSchlick( 1.0, f90, NoL );
	float view_scatter = FresnelSchlick( 1.0, f90, NoV );
	return light_scatter * view_scatter * ( 1.0 / M_PI );
}

void basis( vec3 n, out vec3 tangent, out vec3 bitangent ){
	if( n.z < -0.999999 ) {
		tangent = vec3( 0 , -1, 0 );
		bitangent = vec3( -1, 0, 0 );
	}
	else {
		float a = 1.0 / ( 1.0 + n.z );
		float b = -n.x * n.y * a;
		tangent = vec3( 1. - n.x * n.x * a, b, -n.x );
		bitangent = vec3( b, 1. - n.y * n.y * a , -n.y );
	}
}

struct Surface {
	vec3 color;
	float roughness_squared;
	vec3 f0;
	float roughness;
	vec2 anisotropy;
	vec3 normal;
	vec3 tangent, bitangent;
};

Surface CreateSurface( vec3 normal, vec3 albedo, float roughness, float metallic, float anisotropic ) {
	Surface surface;
	surface.color = albedo * ( 1.0 - metallic * 0.8 );
	surface.roughness_squared = roughness * roughness;
	float reflectance = 1.0 - roughness;
	// surface.f0 = albedo * metallic + 0.16 * reflectance * reflectance * ( 1.0 - metallic );
	surface.f0 = albedo * metallic + reflectance * ( 1.0 - metallic );
	surface.roughness = roughness;
	surface.anisotropy = max( roughness * vec2( 1.0 + anisotropic, 1.0 - anisotropic), vec2( 0.001 ) );
	surface.normal = normal;
	basis( normal, surface.tangent, surface.bitangent );
	return surface;
}

struct Light {
	vec3 color;
	vec3 direction;
	float attenuation;
	float ambient;
};

Light GetSunLight() {
	Light light;
	light.color = vec3( 1.0 );
	light.direction = -u_LightDir;
	light.attenuation = 1.0;
	light.ambient = 0.75;
	return light;
}

#if APPLY_DLIGHTS
uniform isamplerBuffer u_DynamicLightTiles;
uniform samplerBuffer u_DynamicLightData;

Light GetDynamicLight( int index, vec3 normal ) {
	vec4 data = texelFetch( u_DynamicLightData, index );
	vec3 origin = floor( data.xyz );
	float radius = data.w;
	float intensity = DLIGHT_CUTOFF * radius * radius;
	vec3 dir = v_Position - origin;
	float dist_squared = dot( dir, dir );

	Light light;
	light.color = fract( data.xyz ) / 0.9;
	light.direction = -normalize( dir - normal ); // - normal to prevent 0,0,0
	light.attenuation = max( 0.0, intensity / dist_squared - DLIGHT_CUTOFF );
	light.ambient = 0.0;
	return light;
}
#endif

vec3 ShadePixel( Surface surface, Light light, float occlusion, vec3 v, float NoV, float ToV, float BoV ) {
	float NoL = dot( surface.normal, light.direction );
	float ambient_NoL = clamp( NoL * ( 1.0 - light.ambient ) + light.ambient, 0.0, 1.0 );
	if( ambient_NoL * light.attenuation <= 1.0 / 256.0 ) {
		return vec3( 0.0 );
	}
	NoL = clamp( NoL, 0.0, 1.0 );
	vec3 h = normalize( v + light.direction );
	float NoH = clamp( dot( surface.normal, h ), 0.0, 1.0 );
	float LoH = clamp( dot( light.direction, h ), 0.0, 1.0 );

	float ToL = dot( surface.tangent, light.direction );
	float ToH = dot( surface.tangent, h );
	float BoL = dot( surface.bitangent, light.direction );
	float BoH = dot( surface.bitangent, h );

	float D = DistributionGGXAniso( ToH, BoH, NoH, surface.anisotropy ) * NoL;
	float G = GeometryDistributionGGXAniso( ToV, BoV, ToL, BoL, NoV, NoL, surface.anisotropy );
	vec3 F = FresnelSchlick( surface.f0, NoV );

	vec3 specular = D * G * F;
	float diffuse = M_PI * Diffuse_Burley( NoV, NoL, LoH, surface.roughness ); // random scaling factor, dunno aha
	// float diffuse = 1.0;

	return ( surface.color * diffuse * ( occlusion * 0.5 + 0.5 ) + specular * occlusion ) * light.color * light.attenuation * ambient_NoL;
}

vec3 BRDF( vec3 n, vec3 v, vec3 albedo, float roughness, float metallic, float aniso, float shadow, int dlight_count, int tile_index ) {
	Surface surface = CreateSurface( n, albedo, roughness, metallic, aniso );
	Light light = GetSunLight();

	float NoV = max( dot( n, v ), 1e-4 );
	float ToV = dot( surface.tangent, v );
	float BoV = dot( surface.bitangent, v );

	vec3 color = ShadePixel( surface, light, shadow, v, NoV, ToV, BoV );

#if APPLY_DLIGHTS
	for( int i = 0; i < dlight_count; i++ ) {
		int idx = tile_index * 50 + i; // NOTE(msc): 50 = MAX_DLIGHTS_PER_TILE
		int dlight_index = texelFetch( u_DynamicLightTiles, idx ).x;
		light = GetDynamicLight( dlight_index, n );
		color += ShadePixel( surface, light, 1.0, v, NoV, ToV, BoV );
	}
#endif

	return color;
}
