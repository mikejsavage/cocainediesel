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
#if APPLY_DLIGHTS
	uniform isamplerBuffer u_DynamicLightTiles;
	uniform samplerBuffer u_DynamicLightData;
#endif

vec3 BRDF( vec3 n, vec3 v, vec3 l, vec3 light_color, vec3 albedo, float roughness, float metallic, float aniso, float shadow, int dlight_count, int tile_index ) {
	float roughness_squared = roughness * roughness;
	float reflectance = 1.0 - roughness_squared;
	vec3 f0 = 0.16 * reflectance * reflectance * ( 1.0 - metallic ) + albedo * metallic;

	albedo = ( 1.0 - metallic * 0.8 ) * albedo;

	// vec2 a = max( roughness_squared * vec2( 1.0 + aniso, 1.0 - aniso ), vec2( 0.001 ) );
	vec2 a = max( roughness * vec2( 1.0 + aniso, 1.0 - aniso ), vec2( 0.001 ) );
	vec3 t, b;
	basis( n, t, b );

	vec3 h = normalize( v + l );

	float NoV = max( dot( n, v ), 1e-4 );
	float NoL = clamp( dot( n, l ), 0.0, 1.0 );
	float NoH = clamp( dot( n, h ), 0.0, 1.0 );
	float LoH = clamp( dot( l, h ), 0.0, 1.0 );

	float ToV = dot( t, v );
	float BoV = dot( b, v );
	float ToL = dot( t, l );
	float BoL = dot( b, l );
	float ToH = dot( t, h );
	float BoH = dot( b, h );

	float D = DistributionGGXAniso( ToH, BoH, NoH, a ) * NoL;
	float G = GeometryDistributionGGXAniso( ToV, BoV, ToL, BoL, NoV, NoL, a );
	vec3 F = FresnelSchlick( f0, NoV );

	vec3 specular = D * F * G * light_color * shadow;
	// vec3 diffuse = Diffuse_Burley( NoV, NoL, LoH, roughness ) * albedo * ( shadow * 0.5 + 0.5 );
	vec3 diffuse = ( NoL * 0.5 + 0.5 ) * albedo * ( shadow * 0.5 + 0.5 );

#if APPLY_DLIGHTS
	for( int i = 0; i < dlight_count; i++ ) {
		int idx = tile_index * 50 + i; // NOTE(msc): 50 = MAX_DLIGHTS_PER_TILE
		int dlight_index = texelFetch( u_DynamicLightTiles, idx ).x;

		vec4 data = texelFetch( u_DynamicLightData, dlight_index );
		vec3 origin = floor( data.xyz );
		light_color = fract( data.xyz ) / 0.9;
		float radius = data.w;

		float intensity = DLIGHT_CUTOFF * radius * radius;
		float dist = distance( v_Position, origin );
		l = -normalize( v_Position - origin - n ); // - normal to prevent 0,0,0

		float dlight_intensity = max( 0.0, intensity / ( dist * dist ) - DLIGHT_CUTOFF );

		h = normalize( v + l );
		NoL = clamp( dot( n, l ), 0.0, 1.0 );
		NoH = clamp( dot( n, h ), 0.0, 1.0 );
		LoH = clamp( dot( l, h ), 0.0, 1.0 );
		ToL = dot( t, l );
		BoL = dot( b, l );
		ToH = dot( t, h );
		BoH = dot( b, h );

		D = DistributionGGXAniso( ToH, BoH, NoH, a ) * NoL;
		G = GeometryDistributionGGXAniso( ToV, BoV, ToL, BoL, NoV, NoL, a );
		F = FresnelSchlick( f0, NoV );

		specular += D * F * G * light_color * dlight_intensity;
		// diffuse += Diffuse_Burley( NoV, NoL, LoH, roughness ) * light_color * dlight_intensity;
		diffuse += NoL * light_color * dlight_intensity;
	}
#endif

	return diffuse + specular;
}
