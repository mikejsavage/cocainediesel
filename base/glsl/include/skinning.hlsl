#pragma once

#if SKINNED

float3x4 SkinningMatrix( uint4 indices, float4 weights ) {
	return weights.x * u_Pose[ indices.x ] + weights.y * u_Pose[ indices.y ] + weights.z * u_Pose[ indices.z ] + weights.w * u_Pose[ indices.w ];
}

#endif
