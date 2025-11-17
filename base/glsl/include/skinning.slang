#pragma once

float3x4 SkinningMatrix( StructuredBuffer< float3x4 > pose, uint4 indices, float4 weights ) {
	return weights.x * pose[ indices.x ]
		+ weights.y * pose[ indices.y ]
		+ weights.z * pose[ indices.z ]
		+ weights.w * pose[ indices.w ];
}
