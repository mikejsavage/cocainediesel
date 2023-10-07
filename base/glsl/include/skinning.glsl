#if VERTEX_SHADER
#if SKINNED

layout( location = VertexAttribute_JointIndices ) in uvec4 a_JointIndices;
layout( location = VertexAttribute_JointWeights ) in vec4 a_JointWeights;

#ifndef MULTIDRAW
layout( std140 ) uniform u_Pose {
	mat4 u_SkinningMatrices[ SKINNED_MODEL_MAX_JOINTS ];
};

void Skin( inout vec4 position, inout vec3 normal ) {
	mat4 skin =
		a_JointWeights.x * u_SkinningMatrices[ a_JointIndices.x ] +
		a_JointWeights.y * u_SkinningMatrices[ a_JointIndices.y ] +
		a_JointWeights.z * u_SkinningMatrices[ a_JointIndices.z ] +
		a_JointWeights.w * u_SkinningMatrices[ a_JointIndices.w ];

	position = skin * position;
	normal = normalize( mat3( skin ) * normal );
}
#else
struct SkinningInstance {
	uint offset;
};
layout( std430 ) readonly buffer b_SkinningInstances {
	SkinningInstance skinning_instances[];
};

layout( std430 ) readonly buffer b_SkinningMatrices {
	mat4 skinning_matrices[];
};

void Skin( inout vec4 position, inout vec3 normal ) {
	uint offset = skinning_instances[ gl_DrawID ].offset;
	mat4 skin =
		a_JointWeights.x * skinning_matrices[ offset + a_JointIndices.x ] +
		a_JointWeights.y * skinning_matrices[ offset + a_JointIndices.y ] +
		a_JointWeights.z * skinning_matrices[ offset + a_JointIndices.z ] +
		a_JointWeights.w * skinning_matrices[ offset + a_JointIndices.w ];

	position = skin * position;
	normal = normalize( mat3( skin ) * normal );
}
#endif

#endif
#endif
