#include "include/uniforms.glsl"
#include "include/common.glsl"

#if VERTEX_SHADER

layout( location = VertexAttribute_Position ) in vec4 a_Position;

void main() {
	gl_Position = a_Position;
}

#else

uniform sampler2D u_Screen;

layout( std140 ) uniform u_PostProcess {
	float u_Zoom;
};

layout( location = FragmentShaderOutput_Albedo ) out vec4 f_Albedo;

vec3 radialBlur( vec2 uv ) {
	const int SAMPLES = 16;
	const float BLUR_INTENSITY = 0.04;
	const vec2 CENTER = vec2( 0.5, 0.5 );

	vec3 col = vec3( 0.0 );
	vec2 dist = uv - CENTER;

	float len = length( dist );
	float l = sqrt( len );
	
	for( int j = 0; j < SAMPLES; j++ ) {
		float scale = 1.0 - ( l * BLUR_INTENSITY * u_Zoom * j ) / SAMPLES;
		col += texture( u_Screen, clamp( dist * scale + CENTER, vec2( 0.0, 0.0 ), vec2( 1.0, 1.0 ) ) ).rgb;
	}

	col /= SAMPLES;

	// vignette effect
	float vignette = 1.0 - max( len - 0.5, 0.0 ) * ( 1.0 + u_Zoom );
	return col * vignette;
}

void main() {
	vec2 uv = gl_FragCoord.xy / u_ViewportSize;
	f_Albedo.rgb = mix( texture( u_Screen, uv ).rgb, radialBlur( uv ), u_Zoom );
}

#endif
