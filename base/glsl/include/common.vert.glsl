#ifdef SKINNED

qf_attribute uvec4 a_JointIndices;
qf_attribute vec4 a_JointWeights;

uniform mat4 u_SkinningMatrices[ MAX_UNIFORM_JOINTS ];

void QF_VertexSkinnedTransform( inout vec4 position, inout vec3 normal ) {
	mat4 skin =
		a_JointWeights.x * u_SkinningMatrices[ a_JointIndices.x ] +
		a_JointWeights.y * u_SkinningMatrices[ a_JointIndices.y ] +
		a_JointWeights.z * u_SkinningMatrices[ a_JointIndices.z ] +
		a_JointWeights.w * u_SkinningMatrices[ a_JointIndices.w ];

	position = skin * position;
	normal = normalize( ( skin * vec4( normal, 0.0 ) ).xyz );
}

void QF_VertexSkinnedTransform_Tangent( inout vec4 position, inout vec3 normal, inout vec3 tangent ) {
	mat4 skin =
		a_JointWeights.x * u_SkinningMatrices[ a_JointIndices.x ] +
		a_JointWeights.y * u_SkinningMatrices[ a_JointIndices.y ] +
		a_JointWeights.z * u_SkinningMatrices[ a_JointIndices.z ] +
		a_JointWeights.w * u_SkinningMatrices[ a_JointIndices.w ];

	position = skin * position;
	normal = normalize( ( skin * vec4( normal, 0.0 ) ).xyz );
	tangent = normalize( ( skin * vec4( tangent, 0.0 ) ).xyz );
}

#endif

#ifdef QF_APPLY_DEFORMVERTS

float QF_WaveFunc_Sin(float x) {
	return sin(fract(x) * M_TWOPI);
}

float QF_WaveFunc_Triangle(float x) {
	x = fract(x);
	return step(x, 0.25) * x * 4.0 + (2.0 - 4.0 * step(0.25, x) * step(x, 0.75) * x) + ((step(0.75, x) * x - 0.75) * 4.0 - 1.0);
}

float QF_WaveFunc_Square(float x) {
	return step(fract(x), 0.5) * 2.0 - 1.0;
}

float QF_WaveFunc_Sawtooth(float x) {
	return fract(x);
}

float QF_WaveFunc_InverseSawtooth(float x) {
	return 1.0 - fract(x);
}

#define WAVE_SIN(time,base,amplitude,phase,freq) (((base)+(amplitude)*QF_WaveFunc_Sin((phase)+(time)*(freq))))
#define WAVE_TRIANGLE(time,base,amplitude,phase,freq) (((base)+(amplitude)*QF_WaveFunc_Triangle((phase)+(time)*(freq))))
#define WAVE_SQUARE(time,base,amplitude,phase,freq) (((base)+(amplitude)*QF_WaveFunc_Square((phase)+(time)*(freq))))
#define WAVE_SAWTOOTH(time,base,amplitude,phase,freq) (((base)+(amplitude)*QF_WaveFunc_Sawtooth((phase)+(time)*(freq))))
#define WAVE_INVERSESAWTOOTH(time,base,amplitude,phase,freq) (((base)+(amplitude)*QF_WaveFunc_InverseSawtooth((phase)+(time)*(freq))))

#endif

#ifdef APPLY_INSTANCED_TRANSFORMS

#ifdef APPLY_INSTANCED_ATTRIB_TRANSFORMS
qf_attribute vec4 a_InstanceQuat, a_InstancePosAndScale;
#elif defined(GL_ARB_draw_instanced)
uniform vec4 u_InstancePoints[MAX_UNIFORM_INSTANCES*2];
#define a_InstanceQuat u_InstancePoints[gl_InstanceID*2]
#define a_InstancePosAndScale u_InstancePoints[gl_InstanceID*2+1]
#else
uniform vec4 u_InstancePoints[2];
#define a_InstanceQuat u_InstancePoints[0]
#define a_InstancePosAndScale u_InstancePoints[1]
#endif // APPLY_INSTANCED_ATTRIB_TRANSFORMS

void QF_InstancedTransform(inout vec4 Position, inout vec3 Normal) {
	Position.xyz = (cross(a_InstanceQuat.xyz,
			cross(a_InstanceQuat.xyz, Position.xyz) + Position.xyz*a_InstanceQuat.w)*2.0 +
		Position.xyz) * a_InstancePosAndScale.w + a_InstancePosAndScale.xyz;
	Normal = cross(a_InstanceQuat.xyz, cross(a_InstanceQuat.xyz, Normal) + Normal*a_InstanceQuat.w)*2.0 + Normal;
}

#endif

void QF_TransformVerts(inout vec4 Position, inout vec3 Normal, inout vec2 TexCoord) {
#ifdef SKINNED
	QF_VertexSkinnedTransform(Position, Normal);
#endif
#ifdef QF_APPLY_DEFORMVERTS
	QF_DeformVerts(Position, Normal, TexCoord);
#endif
#ifdef APPLY_INSTANCED_TRANSFORMS
	QF_InstancedTransform(Position, Normal);
#endif
}

void QF_TransformVerts_Tangent(inout vec4 Position, inout vec3 Normal, inout vec3 Tangent, inout vec2 TexCoord) {
#ifdef SKINNED
	QF_VertexSkinnedTransform_Tangent(Position, Normal, Tangent);
#endif
#ifdef QF_APPLY_DEFORMVERTS
	QF_DeformVerts(Position, Normal, TexCoord);
#endif
#ifdef APPLY_INSTANCED_TRANSFORMS
	QF_InstancedTransform(Position, Normal);
#endif
}
