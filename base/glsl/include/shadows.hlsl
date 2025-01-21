#pragma once

#if APPLY_SHADOWS

float SampleShadowmap( float2 base_uv, float u, float v, float2 inv_shadowmap_size, uint32_t cascadeIdx, float depth, float2 receiverPlaneDepthBias ) {
	float2 uv = base_uv + float2( u, v ) * inv_shadowmap_size;
	float z = depth + dot( float2( u, v ) * inv_shadowmap_size, receiverPlaneDepthBias );
	return u_ShadowmapTextureArray.SampleCmp( u_ShadowmapSampler, float3( uv, cascadeIdx ), z );
}

float2 ComputeReceiverPlaneDepthBias( float3 texCoordDX, float3 texCoordDY ) {
	float2 biasUV;
	biasUV.x = texCoordDY.y * texCoordDX.z - texCoordDX.y * texCoordDY.z;
	biasUV.y = texCoordDX.x * texCoordDY.z - texCoordDY.x * texCoordDX.z;
	biasUV *= 1.0f / ( ( texCoordDX.x * texCoordDY.y ) - ( texCoordDX.y * texCoordDY.x ) );
	return biasUV;
}

float3 GetShadowPosOffset( float nDotL, float3 normal ) {
	const float offset_scale = 1.0f;
	float2 shadowmap_size;
	float dont_care_layers;
	u_ShadowmapTextureArray.GetDimensions( shadowmap_size.x, shadowmap_size.y, dont_care_layers );
	float2 inv_shadowmap_size = 1.0f / shadowmap_size;
	float texelSize = 2.0f * inv_shadowmap_size.x;
	float nmlOffsetScale = clamp( 1.0f - nDotL, 0.0f, 1.0f );
	return texelSize * offset_scale * nmlOffsetScale * normal;
}

float SampleShadowmapOptimizedPCF( float3 shadowPos, float3 shadowPosDX, float3 shadowPosDY, uint32_t cascadeIdx ) {
	float2 shadowmap_size;
	float dont_care_layers;
	u_ShadowmapTextureArray.GetDimensions( shadowmap_size.x, shadowmap_size.y, dont_care_layers );
	float2 inv_shadowmap_size = 1.0f / shadowmap_size;

	float lightDepth = shadowPos.z;

#if 0
	float2 receiverPlaneDepthBias = ComputeReceiverPlaneDepthBias( shadowPosDX, shadowPosDY );

        float fractionalSamplingError = 2.0f * dot( inv_shadowmap_size, abs( receiverPlaneDepthBias ) );
        lightDepth -= min( fractionalSamplingError, 0.01f );

#else
	float2 receiverPlaneDepthBias = 0.0f;
	lightDepth -= 0.001f;
#endif

	float2 uv = shadowPos.xy * shadowmap_size;
	float2 base_uv = floor( uv + 0.5f );
	float s = uv.x + 0.5f - base_uv.x;
	float t = uv.y + 0.5f - base_uv.y;
	base_uv -= 0.5f;
	base_uv *= inv_shadowmap_size;

	float uw0 = (4.0f - 3.0f * s);
	float uw1 = 7.0f;
	float uw2 = (1.0f + 3.0f * s);

	float u0 = (3.0f - 2.0f * s) / uw0 - 2.0f;
	float u1 = (3.0f + s) / uw1;
	float u2 = s / uw2 + 2.0f;

	float vw0 = (4.0f - 3.0f * t);
	float vw1 = 7.0f;
	float vw2 = (1.0f + 3.0f * t);

	float v0 = (3.0f - 2.0f * t) / vw0 - 2;
	float v1 = (3.0f + t) / vw1;
	float v2 = t / vw2 + 2;

	float sum = 0.0f;

	sum += uw0 * vw0 * SampleShadowmap( base_uv, u0, v0, inv_shadowmap_size, cascadeIdx, lightDepth, receiverPlaneDepthBias );
	sum += uw1 * vw0 * SampleShadowmap( base_uv, u1, v0, inv_shadowmap_size, cascadeIdx, lightDepth, receiverPlaneDepthBias );
	sum += uw2 * vw0 * SampleShadowmap( base_uv, u2, v0, inv_shadowmap_size, cascadeIdx, lightDepth, receiverPlaneDepthBias );

	sum += uw0 * vw1 * SampleShadowmap( base_uv, u0, v1, inv_shadowmap_size, cascadeIdx, lightDepth, receiverPlaneDepthBias );
	sum += uw1 * vw1 * SampleShadowmap( base_uv, u1, v1, inv_shadowmap_size, cascadeIdx, lightDepth, receiverPlaneDepthBias );
	sum += uw2 * vw1 * SampleShadowmap( base_uv, u2, v1, inv_shadowmap_size, cascadeIdx, lightDepth, receiverPlaneDepthBias );

	sum += uw0 * vw2 * SampleShadowmap( base_uv, u0, v2, inv_shadowmap_size, cascadeIdx, lightDepth, receiverPlaneDepthBias );
	sum += uw1 * vw2 * SampleShadowmap( base_uv, u1, v2, inv_shadowmap_size, cascadeIdx, lightDepth, receiverPlaneDepthBias );
	sum += uw2 * vw2 * SampleShadowmap( base_uv, u2, v2, inv_shadowmap_size, cascadeIdx, lightDepth, receiverPlaneDepthBias );

	return sum * ( 1.0f / 144.0f );
}

float ShadowCascade( float3 position, float3 normal, uint32_t cascadeIdx ) {
	float3 cascadeOffset = u_Shadowmap[ 0 ].cascades[ cascadeIdx ].offset;
	float3 cascadeScale = u_Shadowmap[ 0 ].cascades[ cascadeIdx ].scale;

	float3 offset = GetShadowPosOffset( dot( normal, u_View[ 0 ].sun_direction ), normal ) / abs( cascadeScale.z );
	float3 shadowPos = mul( u_Shadowmap[ 0 ].VP, float4( position + offset, 1.0f ) ).xyz;
	float3 shadowPosDX = ddx( shadowPos ) * cascadeScale;
	float3 shadowPosDY = ddy( shadowPos ) * cascadeScale;
	shadowPos = ( shadowPos + cascadeOffset ) * cascadeScale;
	shadowPos.z = shadowPos.z * 0.5f + 0.5f;

	return SampleShadowmapOptimizedPCF( shadowPos, shadowPosDX, shadowPosDY, cascadeIdx );
}

float GetLight( float3 position, float3 normal ) {
	float view_distance = length( u_View[ 0 ].camera_pos - position );

	for( uint32_t i = 0; i < u_Shadowmap[ 0 ].num_cascades; i++ ) {
		float plane = u_Shadowmap[ 0 ].cascades[ i ].plane;
		if( view_distance <= plane ) {
			const float blend_threshold = 0.5f;

			float light = ShadowCascade( position, normal, i );
			float fade_factor = ( plane - view_distance ) / plane;
			if( fade_factor < blend_threshold ) {
				float next_light = 1.0f; // fade to nothing if we're on last cascade
				if( i + 1 < u_Shadowmap[ 0 ].num_cascades ) {
					next_light = ShadowCascade( position, normal, i + 1 );
				}
				float lerp_amt = smoothstep( 0.0f, blend_threshold, fade_factor );
				light = lerp( next_light, light, lerp_amt );
			}
			return light;
		}
	}

	return 1.0f;
}

#endif // #if APPLY_SHADOWS
