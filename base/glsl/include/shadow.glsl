uniform sampler2D u_NearShadowmapTexture;
uniform sampler2D u_FarShadowmapTexture;

vec2 ShadowOffsets( vec3 N, vec3 L ) {
	float cos_alpha = clamp( dot( N, L ), 0.0, 1.0 );
	float offset_scale_N = sqrt( 1.0 - cos_alpha * cos_alpha );
	float offset_scale_L = offset_scale_N / cos_alpha;
	return vec2( offset_scale_N, min( 5.0, offset_scale_L ) );
}

float GetLightPCF( vec4 shadow_pos, vec3 normal, vec3 lightdir, sampler2D shadowmap, int pcfCount ) {
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
		}
	}
	return shadow / pcfTotal;
}

float GetLight( vec3 normal ) {
	float light = 0.0;
	vec3 light_ndc = v_NearShadowmapPosition.xyz / v_NearShadowmapPosition.w;
	vec3 light_norm = light_ndc * 0.5 + 0.5;
	if ( light_norm.z > 1.0 ) {
		light = 0.0;
	} else {
		float near_far_fract = smoothstep( 0.8, 1.0, length( v_NearShadowmapPosition.xy ) );
		float near_light = 0.0;
		if ( near_far_fract < 1.0 ) {
			near_light = GetLightPCF( v_NearShadowmapPosition, normal, u_LightDir, u_NearShadowmapTexture, 1 );
		}
		float far_light = GetLightPCF( v_FarShadowmapPosition, normal, u_LightDir, u_FarShadowmapTexture, 1 );
		light = mix( near_light, far_light, near_far_fract );
	}
  return light;
}
