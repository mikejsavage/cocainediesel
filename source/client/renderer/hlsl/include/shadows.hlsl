#pragma once

/*
https://github.com/TheRealMJP/Shadows

The MIT License (MIT)

Copyright (c) 2016 MJP

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#ifdef APPLY_SHADOWS

static const bool EnableBrokenAutoBias = false;

float SampleShadowmap( float2 base_uv, float u, float v, float2 inv_shadowmap_size, uint32_t cascadeIdx, float depth, float2 dZ_dUV ) {
	float2 uv = base_uv + float2( u, v ) * inv_shadowmap_size;
	float z = depth + dot( float2( u, v ) * inv_shadowmap_size, dZ_dUV );
	return u_ShadowmapTextureArray.SampleCmp( u_ShadowmapSampler, float3( uv, cascadeIdx ), z );
}

float3 GetShadowPosOffset( float nDotL, float3 normal ) {
	const float offset_scale = 1.0f;
	float2 shadowmap_size;
	float dont_care_layers;
	u_ShadowmapTextureArray.GetDimensions( shadowmap_size.x, shadowmap_size.y, dont_care_layers );
	float texelSize = 2.0f / shadowmap_size.x;
	float nmlOffsetScale = saturate( 1.0f - nDotL );
	return texelSize * offset_scale * nmlOffsetScale * normal;
}

float SampleShadowmapOptimizedPCF( float3 shadowPos, float2 dZ_dUV, uint32_t cascadeIdx ) {
	float2 shadowmap_size;
	float dont_care_layers;
	u_ShadowmapTextureArray.GetDimensions( shadowmap_size.x, shadowmap_size.y, dont_care_layers );
	float2 inv_shadowmap_size = 1.0f / shadowmap_size;

	float lightDepth = shadowPos.z;

	if( EnableBrokenAutoBias ) {
		float fractionalSamplingError = 2.0f * dot( inv_shadowmap_size, abs( dZ_dUV ) );
		lightDepth += min( fractionalSamplingError, 0.01f );
	}
	else {
		lightDepth += 0.0015f;
	}

	float2 uv = shadowPos.xy * shadowmap_size;
	float2 base_uv = floor( uv + 0.5f );
	float s = uv.x + 0.5f - base_uv.x;
	float t = uv.y + 0.5f - base_uv.y;
	base_uv -= float2( 0.5f, 0.5f );
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

	sum += uw0 * vw0 * SampleShadowmap( base_uv, u0, v0, inv_shadowmap_size, cascadeIdx, lightDepth, dZ_dUV );
	sum += uw1 * vw0 * SampleShadowmap( base_uv, u1, v0, inv_shadowmap_size, cascadeIdx, lightDepth, dZ_dUV );
	sum += uw2 * vw0 * SampleShadowmap( base_uv, u2, v0, inv_shadowmap_size, cascadeIdx, lightDepth, dZ_dUV );

	sum += uw0 * vw1 * SampleShadowmap( base_uv, u0, v1, inv_shadowmap_size, cascadeIdx, lightDepth, dZ_dUV );
	sum += uw1 * vw1 * SampleShadowmap( base_uv, u1, v1, inv_shadowmap_size, cascadeIdx, lightDepth, dZ_dUV );
	sum += uw2 * vw1 * SampleShadowmap( base_uv, u2, v1, inv_shadowmap_size, cascadeIdx, lightDepth, dZ_dUV );

	sum += uw0 * vw2 * SampleShadowmap( base_uv, u0, v2, inv_shadowmap_size, cascadeIdx, lightDepth, dZ_dUV );
	sum += uw1 * vw2 * SampleShadowmap( base_uv, u1, v2, inv_shadowmap_size, cascadeIdx, lightDepth, dZ_dUV );
	sum += uw2 * vw2 * SampleShadowmap( base_uv, u2, v2, inv_shadowmap_size, cascadeIdx, lightDepth, dZ_dUV );

	return sum * ( 1.0f / 144.0f );
}

float ShadowCascade( float3 position, float3 normal, uint32_t cascadeIdx ) {
	float3 cascadeOffset = u_Shadowmap[ 0 ].cascades[ cascadeIdx ].offset;
	float3 cascadeScale = u_Shadowmap[ 0 ].cascades[ cascadeIdx ].scale;

	float3 offset = GetShadowPosOffset( dot( normal, u_View[ 0 ].sun_direction ), normal ) / abs( cascadeScale.z );

	// https://renderdiagrams.org/2024/12/18/shadowmap-bias/
	float3 shadow_uvz = mul( u_Shadowmap[ 0 ].m, float4( position + offset, 1.0f ) ).xyz;

	float2 dU_dXY = float2( ddx_fine( shadow_uvz.x ), ddy_fine( shadow_uvz.x ) ) * cascadeScale.x;
	float2 dV_dXY = float2( ddx_fine( shadow_uvz.y ), ddy_fine( shadow_uvz.y ) ) * cascadeScale.y;
	float2 dZ_dXY = float2( ddx_fine( shadow_uvz.z ), ddy_fine( shadow_uvz.z ) ) * cascadeScale.z;

	shadow_uvz = ( shadow_uvz + cascadeOffset ) * cascadeScale;

	float2 dZ_dUV = 0.0f;

	if( EnableBrokenAutoBias ) {
		float det = dU_dXY.x * dV_dXY.y - dU_dXY.y * dV_dXY.x;
		if( det != 0.0f ) {
			dZ_dUV.x = dV_dXY.y * dZ_dXY.x - dV_dXY.x * dZ_dXY.y;
			dZ_dUV.y = dU_dXY.x * dZ_dXY.x - dU_dXY.y * dZ_dXY.y;
			dZ_dUV *= 1.0f / det;
		}
	}

	return SampleShadowmapOptimizedPCF( shadow_uvz, dZ_dUV, cascadeIdx );
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
				float next_light = 1.0f; // fade to unshadowed if we're on last cascade
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

#endif // #ifdef APPLY_SHADOWS
