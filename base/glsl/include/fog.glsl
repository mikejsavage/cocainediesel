layout( std140 ) uniform u_Fog {
	float u_FogStrength;
};

vec3 Fog( vec3 color, float dist ) {
	vec3 fog_color = vec3( 1.0 );
	float fog_amount = 1.0 - exp( -u_FogStrength * dist );
	return mix( color, fog_color, fog_amount );
}
