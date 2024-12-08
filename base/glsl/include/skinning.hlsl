struct Pose {
	float3x4 pose[ SKINNED_MODEL_MAX_JOINTS ];
};

float3x4 SkinningMatrix( uint4 indices, float4 weights, Pose pose ) {
	return weights.x * pose.pose[ indices.x ] + weights.y * pose.pose[ indices.y ] + weights.z * pose.pose[ indices.z ] + weights.w * pose.pose[ indices.w ];
}
