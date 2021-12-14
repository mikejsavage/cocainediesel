uniform sampler2DArrayShadow u_ShadowmapTextureArray;

#define USE_DEPTH_PLANE_BIAS 0
#define FILTER_ACROSS_CASCADES 1
const float blend_threshold = 0.5;

float SampleShadowMap( vec2 base_uv, float u, float v, vec2 inv_shadowmap_size, int cascadeIdx, float depth, vec2 receiverPlaneDepthBias ) {
	vec2 uv = base_uv + vec2( u, v ) * inv_shadowmap_size;
#if USE_DEPTH_PLANE_BIAS
	float z = depth + dot( vec2( u, v ) * inv_shadowmap_size, receiverPlaneDepthBias );
#else
	float z = depth;
#endif
	return texture( u_ShadowmapTextureArray, vec4( uv, cascadeIdx, z ) );
}

vec2 ComputeReceiverPlaneDepthBias( vec3 texCoordDX, vec3 texCoordDY ) {
	vec2 biasUV;
	biasUV.x = texCoordDY.y * texCoordDX.z - texCoordDX.y * texCoordDY.z;
	biasUV.y = texCoordDX.x * texCoordDY.z - texCoordDY.x * texCoordDX.z;
	biasUV *= 1.0 / ( ( texCoordDX.x * texCoordDY.y ) - ( texCoordDX.y * texCoordDY.x ) );
	return biasUV;
}

vec3 GetShadowPosOffset( int cascadeIdx, float nDotL, vec3 normal ) {
	vec2 inv_shadowmap_size = 1.0 / textureSize( u_ShadowmapTextureArray, 0 ).xy;
	float texelSize = 2.0 * inv_shadowmap_size.x;
	float nmlOffsetScale = clamp( 1.0 - nDotL, 0.0, 1.0 );
	return texelSize * 1.0 * nmlOffsetScale * normal;
}

float SampleShadowMapOptimizedPCF( vec3 shadowPos, vec3 shadowPosDX, vec3 shadowPosDY, int cascadeIdx ) {
	vec2 shadowmap_size = textureSize( u_ShadowmapTextureArray, 0 ).xy;
	vec2 inv_shadowmap_size = 1.0 / shadowmap_size;

	float lightDepth = shadowPos.z;

#if USE_DEPTH_PLANE_BIAS
	vec2 receiverPlaneDepthBias = ComputeReceiverPlaneDepthBias( shadowPosDX, shadowPosDY );
	float fractionalSamplingError = 2 * dot( inv_shadowmap_size, abs( receiverPlaneDepthBias ) );
	lightDepth -= min( fractionalSamplingError, 0.01 );
#else
	vec2 receiverPlaneDepthBias = vec2( 0.0 );
	lightDepth -= 0.001;
#endif

	vec2 uv = shadowPos.xy * shadowmap_size;
	vec2 base_uv = floor( uv + 0.5 );
	float s = uv.x + 0.5 - base_uv.x;
	float t = uv.y + 0.5 - base_uv.y;
	base_uv -= 0.5;
	base_uv *= inv_shadowmap_size;

	float uw0 = (4 - 3 * s);
	float uw1 = 7;
	float uw2 = (1 + 3 * s);

	float u0 = (3 - 2 * s) / uw0 - 2;
	float u1 = (3 + s) / uw1;
	float u2 = s / uw2 + 2;

	float vw0 = (4 - 3 * t);
	float vw1 = 7;
	float vw2 = (1 + 3 * t);

	float v0 = (3 - 2 * t) / vw0 - 2;
	float v1 = (3 + t) / vw1;
	float v2 = t / vw2 + 2;

	float sum = 0.0;

	sum += uw0 * vw0 * SampleShadowMap( base_uv, u0, v0, inv_shadowmap_size, cascadeIdx, lightDepth, receiverPlaneDepthBias );
	sum += uw1 * vw0 * SampleShadowMap( base_uv, u1, v0, inv_shadowmap_size, cascadeIdx, lightDepth, receiverPlaneDepthBias );
	sum += uw2 * vw0 * SampleShadowMap( base_uv, u2, v0, inv_shadowmap_size, cascadeIdx, lightDepth, receiverPlaneDepthBias );

	sum += uw0 * vw1 * SampleShadowMap( base_uv, u0, v1, inv_shadowmap_size, cascadeIdx, lightDepth, receiverPlaneDepthBias );
	sum += uw1 * vw1 * SampleShadowMap( base_uv, u1, v1, inv_shadowmap_size, cascadeIdx, lightDepth, receiverPlaneDepthBias );
	sum += uw2 * vw1 * SampleShadowMap( base_uv, u2, v1, inv_shadowmap_size, cascadeIdx, lightDepth, receiverPlaneDepthBias );

	sum += uw0 * vw2 * SampleShadowMap( base_uv, u0, v2, inv_shadowmap_size, cascadeIdx, lightDepth, receiverPlaneDepthBias );
	sum += uw1 * vw2 * SampleShadowMap( base_uv, u1, v2, inv_shadowmap_size, cascadeIdx, lightDepth, receiverPlaneDepthBias );
	sum += uw2 * vw2 * SampleShadowMap( base_uv, u2, v2, inv_shadowmap_size, cascadeIdx, lightDepth, receiverPlaneDepthBias );

	return sum * 1.0 / 144;
}

void GetCascadeOffsetScale( int cascadeIdx, out vec3 cascadeOffset, out vec3 cascadeScale ) {
	switch( cascadeIdx ) {
		case 0:
			cascadeOffset = u_CascadeOffsetA;
			cascadeScale = u_CascadeScaleA;
			return;
		case 1:
			cascadeOffset = u_CascadeOffsetB;
			cascadeScale = u_CascadeScaleB;
			return;
		case 2:
			cascadeOffset = u_CascadeOffsetC;
			cascadeScale = u_CascadeScaleC;
			return;
		case 3:
			cascadeOffset = u_CascadeOffsetD;
			cascadeScale = u_CascadeScaleD;
			return;
	}
}

float GetCascadePlane( int cascadeIdx ) {
	switch( cascadeIdx ) {
		case 0:
			return u_CascadePlaneA;
		case 1:
			return u_CascadePlaneB;
		case 2:
			return u_CascadePlaneC;
		case 3:
		default:
			return u_CascadePlaneD;
	}
}

float ShadowCascade( vec3 position, vec3 normal, int cascadeIdx ) {
	vec3 cascadeOffset, cascadeScale;
	GetCascadeOffsetScale( cascadeIdx, cascadeOffset, cascadeScale );

	vec3 offset = GetShadowPosOffset( cascadeIdx, dot( normal, u_LightDir ), normal ) / abs( cascadeScale.z );
	vec3 shadowPos = ( u_ShadowMatrix * vec4( position + offset, 1.0 ) ).xyz;
	vec3 shadowPosDX = dFdx( shadowPos ) * cascadeScale;
	vec3 shadowPosDY = dFdy( shadowPos ) * cascadeScale;
	shadowPos += cascadeOffset;
	shadowPos *= cascadeScale;
	shadowPos.z = shadowPos.z * 0.5 + 0.5;

	return SampleShadowMapOptimizedPCF( shadowPos, shadowPosDX, shadowPosDY, cascadeIdx );
}

float GetLight( vec3 position, vec3 normal ) {
	float view_distance = length( u_CameraPos - position );

	for( int i = 0; i < u_ShadowCascades; i++ ) {
		float plane = GetCascadePlane( i );
		if( view_distance <= plane ) {
			float light = ShadowCascade( position, normal, i );
			#if FILTER_ACROSS_CASCADES
				float fade_factor = ( plane - view_distance ) / plane;
				if( fade_factor < blend_threshold ) {
					float next_light = 1.0; // fade to nothing if we're on last cascade
					if( i + 1 < u_ShadowCascades ) {
						next_light = ShadowCascade( position, normal, i + 1 );
					}
					float lerp_amt = smoothstep( 0.0, blend_threshold, fade_factor );
					light = mix( next_light, light, lerp_amt );
				}
			#endif
			return light;
		}
	}

	return 1.0;
}
