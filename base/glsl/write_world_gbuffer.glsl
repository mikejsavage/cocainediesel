#include "include/uniforms.glsl"
#include "include/common.glsl"

qf_varying vec3 v_Normal;

#if VERTEX_SHADER

in vec4 a_Position;
in vec3 a_Normal;

void main() {
	gl_Position = u_P * u_V * u_M * a_Position;
	v_Normal = mat3( u_M ) * a_Normal;
}

#else

out vec2 f_Normal;

void main() {
	f_Normal = CompressNormal( normalize( v_Normal ) );
}

#endif
