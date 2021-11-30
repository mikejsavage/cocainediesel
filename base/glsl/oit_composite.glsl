#if VERTEX_SHADER

in vec4 a_Position;

void main() {
	gl_Position = a_Position;
}

#else

uniform sampler2D u_Background;
uniform sampler2D u_Accum;
uniform sampler2D u_Modulate;

out vec3 f_Albedo;

float maxComponent( vec3 v ) {
	return max( max( v.x, v.y ), v.z );
}

float minComponent( vec3 v ) {
	return min( min( v.x, v.y ), v.z );
}

void main() {
	ivec2 uv = ivec2( gl_FragCoord.xy );
	vec3 background = texelFetch( u_Background, uv, 0 ).rgb;

	vec4 backgroundModulationAndDiffusion = texelFetch( u_Modulate, uv, 0 );
	vec3 backgroundModulation = backgroundModulationAndDiffusion.rgb;
	float diffusion = backgroundModulationAndDiffusion.a;

	if( minComponent( backgroundModulation ) == 1.0 || diffusion == 1.0 ) {
		f_Albedo = background;
		return;
	}

	vec4 accum = texelFetch( u_Accum, uv, 0 );

	if( isinf( accum.a ) ) {
		accum.a = maxComponent( accum.rgb );
	}

	if( isinf( maxComponent( accum.rgb ) ) ) {
		accum = vec4( isinf( accum.a ) ? 1.0 : accum.a );
	}

	const float epsilon = 0.001;
	accum.rgb *= vec3( 0.5 ) + max( backgroundModulation, vec3( epsilon ) ) / ( 2.0 * max( epsilon, maxComponent( backgroundModulation ) ) );

	f_Albedo = background * backgroundModulation + ( vec3( 1.0 ) - backgroundModulation ) * accum.rgb * ( 1.0 / max( accum.a, 0.00001 ) );
}

#endif
