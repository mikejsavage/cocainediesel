#if VERTEX_SHADER
#if SKINNED

layout( location = VertexAttribute_JointIndices ) in uvec4 a_JointIndices;
layout( location = VertexAttribute_JointWeights ) in vec4 a_JointWeights;

layout( std140 ) uniform u_Pose {
	AffineTransform u_SkinningMatrices[ SKINNED_MODEL_MAX_JOINTS ];
};

void Skin( inout vec4 position, inout vec3 normal ) {
	mat4 skin =
		a_JointWeights.x * AffineToMat4( u_SkinningMatrices[ a_JointIndices.x ] ) +
		a_JointWeights.y * AffineToMat4( u_SkinningMatrices[ a_JointIndices.y ] ) +
		a_JointWeights.z * AffineToMat4( u_SkinningMatrices[ a_JointIndices.z ] ) +
		a_JointWeights.w * AffineToMat4( u_SkinningMatrices[ a_JointIndices.w ] );

	position = skin * position;
	normal = normalize( mat3( skin ) * normal );
}

void Skin( inout vec4 position ) {
	mat4 skin =
		a_JointWeights.x * AffineToMat4( u_SkinningMatrices[ a_JointIndices.x ] ) +
		a_JointWeights.y * AffineToMat4( u_SkinningMatrices[ a_JointIndices.y ] ) +
		a_JointWeights.z * AffineToMat4( u_SkinningMatrices[ a_JointIndices.z ] ) +
		a_JointWeights.w * AffineToMat4( u_SkinningMatrices[ a_JointIndices.w ] );

	position = skin * position;
}

#endif
#endif
