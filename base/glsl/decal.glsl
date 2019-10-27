#include "include/common.glsl"
#include "include/uniforms.glsl"
#include "include/fog.glsl"

uniform sampler2D u_DepthTexture;

#if VERTEX_SHADER

in vec4 a_Position;

void main() {
	gl_Position = a_Position;
}

#else

out vec4 f_Albedo;

vec3 WorldPosition() {
	vec2 uv = gl_FragCoord.xy / u_ViewportSize;
	float depth = qf_texture( u_DepthTexture, uv ).r;

	vec4 clip = vec4( vec3( uv, depth ) * 2.0 - 1.0, 1.0 );
	vec4 world = u_InverseP * clip;
	return ( u_InverseV * ( world / world.w ) ).xyz;
}

void main() {
	vec3 world = WorldPosition();
	f_Albedo = vec4( mod( world * 0.001, 1.0 ), 1.0 );
}

#endif
