layout( location = 0 ) out vec4 f_Accum;      // GL_ONE, GL_ONE
layout( location = 1 ) out vec4 f_Modulate;   // GL_ZERO, GL_ONE_MINUS_SRC_COLOR, GL_ONE, GL_ONE

void WritePixel( vec3 reflectionAndEmission, float coverage ) {
  vec3 transmission = vec3( 0.0 );
	f_Modulate.a = 0.0; // unused atm

	f_Modulate.rgb = coverage * ( vec3( 1.0 ) - transmission );
	coverage *= 1.0 - ( transmission.r + transmission.g + transmission.b ) * ( 1.0 / 3.0 );

	float tmp = 1.0 - gl_FragCoord.z * 0.99;
	tmp *= tmp * tmp * 1e4;
	tmp = clamp( tmp, 1e-3, 1.0 );

	float w = clamp( coverage * tmp, 1e-3, 1.5e2 );
	f_Accum = vec4( reflectionAndEmission, 1.0 ) * coverage * w;
}
