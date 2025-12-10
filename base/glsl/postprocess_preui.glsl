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
	float u_Brightness;
	float u_Contrast;
	float u_Saturation;
};

layout( location = FragmentShaderOutput_Albedo ) out vec4 f_Albedo;

vec3 SampleScreen( vec2 uv ) {
	return texture( u_Screen, uv ).rgb;
}

vec3 colorCorrection( vec3 color ) {
	const vec3 LumCoeff = vec3( 0.2125, 0.7154, 0.0721 );

	color *= u_Brightness;

 	vec3 AvgLumin = vec3( 0.5, 0.5, 0.5 );
 	vec3 intensity = vec3( dot( color, LumCoeff ) );

	vec3 satColor = mix( intensity, color, u_Saturation );
 	vec3 conColor = mix( AvgLumin, satColor, u_Contrast );

	return conColor;
}

vec3 radialBlur( vec2 uv ) {
	const int SAMPLES = 16;
	const float BLUR_INTENSITY = 0.03;
	const vec2 CENTER = vec2( 0.5, 0.5 );
	const float VIGNETTE_OFFSET = 0.45;

	vec3 col = vec3( 0.0 );
	vec2 dist = uv - CENTER;

	float len = length( dist );
	float l = sqrt( len );
	
	for( int j = 0; j < SAMPLES; j++ ) {
		float scale = 1.0 - ( l * BLUR_INTENSITY * u_Zoom * j ) / SAMPLES;
		col += SampleScreen( clamp( dist * scale + CENTER, vec2( 0.0, 0.0 ), vec2( 1.0, 1.0 ) ) );
	}

	col /= SAMPLES;

	// vignette effect
	float vignette = 1.0 - max( len - VIGNETTE_OFFSET, 0.0 ) * ( 1.0 + u_Zoom );
	return col * vignette;
}

void main() {
	vec2 uv = gl_FragCoord.xy / u_ViewportSize;
	
	vec3 color = texture( u_Screen, uv ).rgb;
	color = mix( color, radialBlur( uv ), u_Zoom );
	color = colorCorrection( color );

	f_Albedo.rgb = color;
}

#endif
