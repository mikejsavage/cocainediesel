#pragma once

float3x4 SkinningMatrix( vk::BufferPointer< Float3x4 > pose, uint4 indices, float4 weights ) {
	return weights.x * LoadIndex( pose, indices.x ).m
		+ weights.y * LoadIndex( pose, indices.y ).m
		+ weights.z * LoadIndex( pose, indices.z ).m
		+ weights.w * LoadIndex( pose, indices.w ).m;
}
