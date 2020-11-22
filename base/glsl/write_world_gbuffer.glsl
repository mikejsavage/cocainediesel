#include "include/uniforms.glsl"
#include "include/common.glsl"

#if VERTEX_SHADER

in vec4 a_Position;
in vec3 a_Normal;

void main() {
	gl_Position = u_P * u_V * u_M * a_Position;
}

#else

void main() { }

#endif
