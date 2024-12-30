struct Pose {
	float3x4 pose[ SKINNED_MODEL_MAX_JOINTS ];
};

float3x3 Adjugate( float3x4 m ) {
    return float3x3(
		cross( m[ 1 ], m[ 2 ] ),
		cross( m[ 2 ], m[ 0 ] ),
		cross( m[ 0 ], m[ 1 ] )
	);
}

float3x4 SkinningMatrix( uint4 indices, float4 weights, Pose pose ) {
	return weights.x * pose.pose[ indices.x ] + weights.y * pose.pose[ indices.y ] + weights.z * pose.pose[ indices.z ] + weights.w * pose.pose[ indices.w ];
}
