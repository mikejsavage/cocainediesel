#if VERTEX_SHADER
#if SKINNED

in uvec4 a_JointIndices;
in vec4 a_JointWeights;

layout( std140 ) uniform u_Pose {
	mat4 u_SkinningMatrices[ MAX_JOINTS ];
};

void Skin( inout vec4 position, inout vec3 normal ) {
	// the & 127 is to work around a driver bug, Wintel ignores our vertex
	// attrib format and treats them as u32
	mat4 skin =
		a_JointWeights.x * u_SkinningMatrices[ a_JointIndices.x & 127 ] +
		a_JointWeights.y * u_SkinningMatrices[ a_JointIndices.y & 127 ] +
		a_JointWeights.z * u_SkinningMatrices[ a_JointIndices.z & 127 ] +
		a_JointWeights.w * u_SkinningMatrices[ a_JointIndices.w & 127 ];

	position = skin * position;
	normal = normalize( mat3( skin ) * normal );
}

#endif
#endif
